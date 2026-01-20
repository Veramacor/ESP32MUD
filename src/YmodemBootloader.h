#pragma once
#include <Arduino.h>
#include <LittleFS.h>

// ============================================================
// YMODEM CONSTANTS
// ============================================================

static const uint8_t SOH = 0x01;
static const uint8_t STX = 0x02;
static const uint8_t EOT = 0x04;
static const uint8_t ACK = 0x06;
static const uint8_t NAK = 0x15;
static const uint8_t CAN = 0x18;
static const uint8_t CRC_CHAR = 'C';

static const uint32_t YMODEM_BYTE_TIMEOUT_MS = 3000;
static const uint8_t  YMODEM_MAX_RETRIES     = 10;

// ============================================================
// Logging helper
// ============================================================

void ymodemLog(const String &msg) {
    File f = LittleFS.open("/ymodem_debug.txt", "a");
    if (f) {
        f.println(msg);
        f.close();
    }
}

// ============================================================
// CRC16
// ============================================================

uint16_t ymodem_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc = (crc << 1);
        }
    }
    return crc;
}

// ============================================================
// Read byte with timeout
// ============================================================

bool ymodem_readByte(uint8_t &out, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (Serial.available() > 0) {
            out = (uint8_t)Serial.read();
            return true;
        }
        delay(1);
    }
    return false;
}

// ============================================================
// Read block
// ============================================================

bool ymodem_readBlock(uint8_t &blockType,
                      uint8_t &blockNum,
                      uint8_t &blockNumInv,
                      uint8_t *data,
                      uint16_t &dataLen)
{
    blockType = 0;
    blockNum = 0;
    blockNumInv = 0;
    dataLen = 0;

    uint8_t c;
    if (!ymodem_readByte(c, YMODEM_BYTE_TIMEOUT_MS)) {
        ymodemLog("readBlock: timeout waiting for first byte");
        return false;
    }

    // Log the first byte of the block
    ymodemLog(String("readBlock: first byte 0x") + String(c, HEX));

    if (c == SOH) {
        // 128‑byte block
        blockType = SOH;
        dataLen = 128;
        ymodemLog("readBlock: SOH (128-byte block)");
    } else if (c == STX) {
        // 1024‑byte block (1K mode)
        blockType = STX;
        dataLen = 1024;
        ymodemLog("readBlock: STX (1024-byte block)");
    } else if (c == EOT) {
        blockType = EOT;
        ymodemLog("readBlock: EOT");
        return true;
    } else if (c == CAN) {
        blockType = CAN;
        ymodemLog("readBlock: CAN");
        return true;
    } else {
        ymodemLog(String("readBlock: unexpected first byte 0x") + String(c, HEX));
        return false;
    }

    if (!ymodem_readByte(blockNum, YMODEM_BYTE_TIMEOUT_MS)) {
        ymodemLog("readBlock: timeout reading blockNum");
        return false;
    }
    if (!ymodem_readByte(blockNumInv, YMODEM_BYTE_TIMEOUT_MS)) {
        ymodemLog("readBlock: timeout reading blockNumInv");
        return false;
    }

    ymodemLog(String("readBlock: blkNum=") + String(blockNum) +
              " blkNumInv=" + String(blockNumInv) +
              " dataLen=" + String(dataLen));

    for (uint16_t i = 0; i < dataLen; i++) {
        if (!ymodem_readByte(data[i], YMODEM_BYTE_TIMEOUT_MS)) {
            ymodemLog("readBlock: timeout reading data");
            return false;
        }
    }

    uint8_t crcHi, crcLo;
    if (!ymodem_readByte(crcHi, YMODEM_BYTE_TIMEOUT_MS)) {
        ymodemLog("readBlock: timeout reading crcHi");
        return false;
    }
    if (!ymodem_readByte(crcLo, YMODEM_BYTE_TIMEOUT_MS)) {
        ymodemLog("readBlock: timeout reading crcLo");
        return false;
    }

    uint16_t crcRecv = ((uint16_t)crcHi << 8) | crcLo;
    uint16_t crcCalc = ymodem_crc16(data, dataLen);

    if (crcRecv != crcCalc) {
        ymodemLog(String("readBlock: CRC mismatch recv=0x") +
                  String(crcRecv, HEX) + " calc=0x" + String(crcCalc, HEX));
        return false;
    }

    ymodemLog(String("readBlock: OK blkNum=") + String(blockNum));
    return true;
}

// ============================================================
// Parse header
// ============================================================

bool ymodem_parseHeader(const uint8_t *data,
                        uint16_t dataLen,
                        String &outName,
                        uint32_t &outSize)
{
    if (dataLen == 0) return false;

    if (data[0] == 0) {
        outName = "";
        outSize = 0;
        ymodemLog("parseHeader: empty header (end of session)");
        return true;
    }

    uint16_t i = 0;
    outName = "";
    while (i < dataLen && data[i] != 0) {
        outName += (char)data[i++];
    }

    if (i >= dataLen) {
        ymodemLog("parseHeader: no size field");
        return false;
    }
    i++;

    String sizeStr;
    while (i < dataLen && data[i] != 0) {
        sizeStr += (char)data[i++];
    }

    outSize = (uint32_t)sizeStr.toInt();

    ymodemLog(String("parseHeader: filename='") + outName +
              "' size=" + String(outSize));
    return true;
}

// ============================================================
// Receive file
// ============================================================

bool ymodem_receiveFile(const String &filename, uint32_t expectedSize) {
    ymodemLog("Receiving file: " + filename +
              " expectedSize=" + String(expectedSize));

    File f = LittleFS.open("/" + filename, "w");
    if (!f) {
        ymodemLog("Failed to open file");
        return false;
    }

    uint8_t dataBuf[1024];
    uint32_t received = 0;
    uint8_t expectedBlockNum = 1;
    uint8_t retries = 0;

    while (true) {
        uint8_t blockType, blkNum, blkNumInv;
        uint16_t dataLen;

        if (!ymodem_readBlock(blockType, blkNum, blkNumInv, dataBuf, dataLen)) {
            retries++;
            ymodemLog(String("receiveFile: readBlock failed, retries=") + String(retries));
            if (retries > YMODEM_MAX_RETRIES) {
                ymodemLog("Too many errors, aborting file");
                f.close();
                return false;
            }
            Serial.write(NAK);
            continue;
        }

        retries = 0; // successful block resets retry count

        if (blockType == EOT) {
            Serial.write(ACK);
            Serial.write(CRC_CHAR);
            ymodemLog("EOT received in file, sent ACK + 'C'");
            break;
        }

        if (blockType == CAN) {
            ymodemLog("Cancelled by sender (CAN)");
            f.close();
            return false;
        }

        if ((uint8_t)(blkNum + blkNumInv) != 0xFF) {
            ymodemLog("Bad block number complement");
            Serial.write(NAK);
            continue;
        }

        if (blkNum == expectedBlockNum) {
            uint16_t toWrite = dataLen;
            if (expectedSize > 0 && (received + toWrite) > expectedSize) {
                toWrite = expectedSize - received;
            }
            if (toWrite > 0) {
                f.write(dataBuf, toWrite);
                received += toWrite;
            }
            expectedBlockNum++;
            Serial.write(ACK);
            ymodemLog(String("Block OK blkNum=") + String(blkNum) +
                      " received=" + String(received));
        } else if (blkNum == (uint8_t)(expectedBlockNum - 1)) {
            // Duplicate block, ACK again
            Serial.write(ACK);
            ymodemLog(String("Duplicate block blkNum=") + String(blkNum) + " ACK again");
        } else {
            ymodemLog(String("Unexpected block number blkNum=") + String(blkNum) +
                      " expected=" + String(expectedBlockNum));
            Serial.write(NAK);
        }
    }

    f.flush();
    f.close();
    ymodemLog("File complete: " + filename +
              " finalSize=" + String(received));
    return true;
}

// ============================================================
// Receive session
// ============================================================

bool ymodem_receiveSession(uint32_t startTimeoutMs) {
    // Ensure USB-CDC (Serial) is ready before starting YMODEM
    while (!Serial) {
        delay(10);
    }
    delay(300);

    LittleFS.remove("/ymodem_debug.txt");
    ymodemLog("Session start, timeoutMs=" + String(startTimeoutMs));

    uint32_t start = millis();
    uint8_t dataBuf[1024];

    while (millis() - start < startTimeoutMs) {
        ymodemLog("Session: sending initial 'C' to start transfer");
        Serial.write(CRC_CHAR);
        Serial.flush();

        uint8_t c;
        if (ymodem_readByte(c, 1000)) {

            ymodemLog(String("Session: first received byte 0x") + String(c, HEX));

            if (c == SOH || c == STX) {
                uint8_t blockType = c;
                uint8_t blkNum, blkNumInv;
                uint16_t dataLen = (blockType == SOH ? 128 : 1024);

                ymodemLog(String("Session: header blockType=") +
                          (blockType == SOH ? "SOH" : "STX") +
                          " dataLen=" + String(dataLen));

                if (!ymodem_readByte(blkNum, YMODEM_BYTE_TIMEOUT_MS)) {
                    ymodemLog("Session: timeout reading header blkNum");
                    return false;
                }
                if (!ymodem_readByte(blkNumInv, YMODEM_BYTE_TIMEOUT_MS)) {
                    ymodemLog("Session: timeout reading header blkNumInv");
                    return false;
                }

                ymodemLog(String("Session: header blkNum=") + String(blkNum) +
                          " blkNumInv=" + String(blkNumInv));

                for (uint16_t i = 0; i < dataLen; i++) {
                    if (!ymodem_readByte(dataBuf[i], YMODEM_BYTE_TIMEOUT_MS)) {
                        ymodemLog("Session: timeout reading header data");
                        return false;
                    }
                }

                uint8_t crcHi, crcLo;
                if (!ymodem_readByte(crcHi, YMODEM_BYTE_TIMEOUT_MS)) {
                    ymodemLog("Session: timeout reading header crcHi");
                    return false;
                }
                if (!ymodem_readByte(crcLo, YMODEM_BYTE_TIMEOUT_MS)) {
                    ymodemLog("Session: timeout reading header crcLo");
                    return false;
                }

                uint16_t crcRecv = ((uint16_t)crcHi << 8) | crcLo;
                uint16_t crcCalc = ymodem_crc16(dataBuf, dataLen);

                if (crcRecv != crcCalc) {
                    ymodemLog(String("Header CRC mismatch recv=0x") +
                              String(crcRecv, HEX) + " calc=0x" + String(crcCalc, HEX));
                    Serial.write(NAK);
                    return false;
                }

                String filename;
                uint32_t filesize = 0;
                if (!ymodem_parseHeader(dataBuf, dataLen, filename, filesize)) {
                    ymodemLog("Header parse failed");
                    Serial.write(NAK);
                    return false;
                }

                if (filename.length() == 0) {
                    Serial.write(ACK);
                    ymodemLog("End of session header received (empty filename)");
                    return true;
                }

                Serial.write(ACK);
                Serial.write(CRC_CHAR);
                ymodemLog(String("Header OK, filename='") + filename +
                          "' size=" + String(filesize) + " sent ACK+'C'");

                if (!ymodem_receiveFile(filename, filesize)) {
                    ymodemLog("File receive failed");
                    return false;
                }

                start = millis();
                continue;
            }

            else if (c == EOT) {
                Serial.write(ACK);
                Serial.write(CRC_CHAR);
                ymodemLog("Session: EOT received, sent ACK+'C'");
                continue;
            }

            else if (c == CAN) {
                ymodemLog("Session: Cancelled by sender (CAN)");
                return false;
            } else {
                ymodemLog(String("Session: unexpected byte 0x") + String(c, HEX));
            }
        }

        delay(100);
    }

    ymodemLog("No transfer started (session timeout)");
    return false;
}