#!/usr/bin/env python
# Reset ESP32 and then upload firmware
# This script is executed by PlatformIO before upload

import os
import sys
import time

Import("env", "projenv")

def before_upload(source, target, env):
    """Execute before upload starts - reset the device"""
    port = env.get("UPLOAD_PORT")
    
    if not port:
        print("[WARNING] No upload port configured, skipping reset")
        return
    
    try:
        print("\n" + "="*70)
        print("[RESET] Resetting ESP32 before upload...")
        print("="*70)
        
        # Try to import and use PySerial for a clean reset
        try:
            import serial
            
            # Open serial port with timeout
            ser = serial.Serial(port, 115200, timeout=0.5)
            time.sleep(0.1)
            
            # Send reset sequence via DTR and RTS
            # Set both to 0 (hold in reset)
            ser.dtr = False
            ser.rts = False
            time.sleep(0.5)
            
            # Release reset
            ser.dtr = True
            ser.rts = True
            time.sleep(0.5)
            
            ser.close()
            print("[OK] ESP32 hardware reset successful (via serial)")
            time.sleep(1)
            
        except ImportError:
            print("[WARNING] PySerial not available - skipping hardware reset")
            print("    (This is okay - platformio will handle the reset)")
        except Exception as e:
            print("[WARNING] Reset attempt failed: " + str(e))
            print("    (Continuing with normal upload)")
        
        print("[OK] Ready to upload\n")
        
    except Exception as e:
        print("[WARNING] Error in reset handler: " + str(e) + "\n")

# Register the pre-upload action
env.AddPreAction("upload", before_upload)

# Define a custom target "reset_upload" that will appear in PlatformIO tasks
def reset_upload_target(source, target, env):
    """Custom target for Reset and Upload"""
    print("[INFO] Running Reset and Upload target...")
    # The upload action will be automatically called
    env.Execute("$PLATFORMIO_BUILD_DIR/upload")

env.AddTarget(
    name="reset_upload",
    dependencies=["upload"],
    actions=[],
    title="Reset and Upload",
    description="Reset device and upload firmware with automatic hardware reset"
)

print("[INFO] Reset & Upload handler loaded")
print("[INFO] Available targets: build, upload, reset_upload, ...")




