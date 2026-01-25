#ifndef CHESS_GAME_H
#define CHESS_GAME_H

#include <Arduino.h>

// Forward declarations
struct Player;
struct ChessSession;

// Function declarations only - implementations in main CPP file
void initChessGame(ChessSession &session, bool playerIsWhite);
void renderChessBoard(Player &p, ChessSession &session);
String formatTime(unsigned long ms);
bool parseChessMove(String moveStr, int &fromCol, int &fromRow, int &toCol, int &toRow);
void startChessGame(Player &p, int playerIndex, ChessSession &session);
void processChessMove(Player &p, int playerIndex, ChessSession &session, String moveStr);
void endChessGame(Player &p, int playerIndex);

#endif // CHESS_GAME_H
