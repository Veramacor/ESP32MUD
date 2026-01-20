#!/usr/bin/env python3
"""
PlatformIO pre-build script to generate version header
Set as extra_scripts in platformio.ini
"""

import os
from datetime import datetime

# In PlatformIO pre-scripts, the SCons environment is passed
# We can access it through the Import() function
Import("env")

# Get the project directory
project_dir = env.get("PROJECT_DIR")
include_dir = os.path.join(project_dir, "include")
output_file = os.path.join(include_dir, "version.h")

# Create include directory if it doesn't exist
os.makedirs(include_dir, exist_ok=True)

# Get current date and time
now = datetime.now()
year = now.strftime("%y")
month = now.strftime("%m")
day = now.strftime("%d")
version = f"{year}.{month}.{day}"

# Format date/time strings
compile_date = now.strftime("%b %d %Y")
compile_time = now.strftime("%H:%M:%S")

# Generate header content
header_content = f"""// Auto-generated version header
// Generated at build time
#ifndef VERSION_H
#define VERSION_H

#define ESP32MUD_VERSION "{version}"
#define COMPILE_DATE "{compile_date}"
#define COMPILE_TIME "{compile_time}"

#endif // VERSION_H
"""

# Write the header file
try:
    with open(output_file, 'w') as f:
        f.write(header_content)
    env.Execute(env.VerboseAction(
        "echo [VERSION] Generated version.h: v" + version,
        "[VERSION] Generating version.h..."
    ))
except Exception as e:
    print(f"[ERROR] Failed to generate version.h: {e}")
    Exit(1)


