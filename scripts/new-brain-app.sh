#!/bin/bash

# new-brain-app.sh - Create a new Brain app with all necessary boilerplate
# This creates a new app directory with brain-sdk as a submodule

set -e

if [ $# -ne 1 ]; then
  echo "Usage: ./scripts/new-brain-app.sh <app-name-or-path>"
  echo ""
  echo "Examples:"
  echo "  ./scripts/new-brain-app.sh my-synth              # Creates ../my-synth"
  echo "  ./scripts/new-brain-app.sh ~/projects/my-synth   # Creates at absolute path"
  echo "  ./scripts/new-brain-app.sh ./my-synth            # Creates at relative path"
  echo ""
  exit 1
fi

INPUT="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Determine if input is a path or just a name
if [[ "$INPUT" == */* ]] || [[ "$INPUT" == ~* ]]; then
  # Contains path separators or starts with ~, treat as path
  APP_DIR="$(cd "$(dirname "$INPUT")" 2>/dev/null && pwd)/$(basename "$INPUT")" || {
    # Directory doesn't exist yet, construct the path
    if [[ "$INPUT" = /* ]]; then
      # Absolute path
      APP_DIR="$INPUT"
    else
      # Relative path
      APP_DIR="$(pwd)/$INPUT"
    fi
  }
  APP_NAME="$(basename "$APP_DIR")"
else
  # Just a name, create one level up from brain-sdk
  APP_NAME="$INPUT"
  PARENT_DIR="$(cd "$SDK_DIR/.." && pwd)"
  APP_DIR="$PARENT_DIR/$APP_NAME"
fi

# Validate app name (basic check - alphanumeric and hyphens)
if ! [[ "$APP_NAME" =~ ^[a-zA-Z0-9_-]+$ ]]; then
  echo "Error: App name must contain only letters, numbers, hyphens, and underscores."
  exit 1
fi

# Check if directory already exists
if [ -d "$APP_DIR" ]; then
  echo "Error: Directory '$APP_DIR' already exists."
  exit 1
fi

# Show confirmation prompt
echo "This will create a new Brain app:"
echo "  Name: $APP_NAME"
echo "  Location: $APP_DIR"
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  echo "Cancelled."
  exit 0
fi

echo "Creating new Brain app: $APP_NAME"
echo "Location: $APP_DIR"
echo ""

# Create app directory
mkdir -p "$APP_DIR"

# Create .vscode directory
mkdir -p "$APP_DIR/.vscode"

# Create CMakeLists.txt
cat > "$APP_DIR/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.22)

# ============================================================================
# BOARD CONFIGURATION - Comment/uncomment to switch between boards
# ============================================================================

# Raspberry Pi Pico (RP2040 - ARM Cortex-M0+) - ACTIVE
set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

# Raspberry Pi Pico 2 (RP2350 - ARM Cortex-M33)
# set(PICO_BOARD pico2)
# set(PICO_PLATFORM rp2350-arm-s)

# Alternative: Raspberry Pi Pico 2 with RISC-V cores
# set(PICO_BOARD pico2)
# set(PICO_PLATFORM rp2350-riscv)

# ============================================================================

# Include Pico SDK (from brain-sdk subdirectory)
include(brain-sdk/pico_sdk_import.cmake)

project($APP_NAME C CXX ASM)

# Initialize the Pico SDK
pico_sdk_init()

# Add brain-sdk which includes brain libraries
add_subdirectory(brain-sdk)

add_executable($APP_NAME main.cpp)

target_link_libraries($APP_NAME
	pico_stdlib
	brain-common
	brain-ui
	brain-io
  brain-utils)

pico_enable_stdio_usb($APP_NAME 1)
pico_enable_stdio_uart($APP_NAME 1)
pico_add_extra_outputs($APP_NAME)
EOF

# Create main.cpp with simple boilerplate
cat > "$APP_DIR/main.cpp" <<EOF
#include <pico/stdlib.h>
#include <stdio.h>

#include "brain-common/brain-common.h"
#include "brain-ui/led.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;

using brain::ui::Led;

const uint LED_PINS[] = {
	BRAIN_LED_1, BRAIN_LED_2, BRAIN_LED_3, BRAIN_LED_4, BRAIN_LED_5, BRAIN_LED_6};

int main() {
	stdio_init_all();

	// Initialize LEDs
	brain::ui::Led leds[6] = {
		brain::ui::Led(LED_PINS[0]), brain::ui::Led(LED_PINS[1]),
		brain::ui::Led(LED_PINS[2]), brain::ui::Led(LED_PINS[3]),
		brain::ui::Led(LED_PINS[4]), brain::ui::Led(LED_PINS[5])};

	for (uint i = 0; i < 6; i++) {
		leds[i].init();
	}

	printf("$APP_NAME started\\n");

	// Simple LED blink pattern
	uint led_index = 0;
	while (true) {
		leds[led_index].toggle();
		led_index = (led_index + 1) % 6;
		sleep_ms(100);
	}

	return 0;
}
EOF

# Create .gitignore
cat > "$APP_DIR/.gitignore" <<EOF
.idea
_deps
cmake-*
build
.DS_Store
*.lck
**/ai
EOF

# Create .gitmodules
cat > "$APP_DIR/.gitmodules" <<EOF
[submodule "brain-sdk"]
	path = brain-sdk
	url = git@github.com:shmoergh/brain-sdk.git
EOF

# Create README.md
cat > "$APP_DIR/README.md" <<EOF
# $APP_NAME

A Brain module application built with the brain-sdk.

## Build

\`\`\`bash
mkdir build
cd build
cmake ..
make
\`\`\`

## Flash

Flash the generated \`$APP_NAME.uf2\` file to your Brain module by holding the BOOTSEL button while connecting it to your computer, then copy the .uf2 file to the mounted drive.

## Development

This project includes brain-sdk as a git submodule. To update the SDK:

\`\`\`bash
cd brain-sdk
git pull origin main
cd ..
git add brain-sdk
git commit -m "Update brain-sdk"
\`\`\`
EOF

# Create .vscode/launch.json
cat > "$APP_DIR/.vscode/launch.json" <<EOF
{
    "version": "0.2.0",
    "configurations": [
        {   "name": "Pico Debug",
            "device": "RP2040",
            "gdbPath": "arm-none-eabi-gdb",
            "cwd": "\${workspaceRoot}",
            "executable": "\${workspaceFolder}/build/$APP_NAME.elf",
            "preLaunchTask": "CMake Build",
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "openocd",
            "configFiles": [
                "interface/picoprobe.cfg",
                "target/rp2040.cfg"
            ],
            "svdFile": "\${env:PICO_SDK_PATH}/src/rp2040/hardware_regs/rp2040.svd",
            "runToEntryPoint": "main",
            "postRestartCommands": [
                "break main",
                "continue"
            ]
        }
    ]
}
EOF

# Create .vscode/settings.json
cat > "$APP_DIR/.vscode/settings.json" <<EOF
{
    "cmake.environment": {
        "PICO_SDK_PATH": "\${env:PICO_SDK_PATH}"
    },
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "files.associations": {
        "cmath": "cpp",
        "iostream": "cpp"
    }
}
EOF

# Create .vscode/tasks.json
cat > "$APP_DIR/.vscode/tasks.json" <<EOF
{
	"version": "2.0.0",
	"tasks": [
		{
			"label": "CMake Build",
			"type": "shell",
			"command": "cmake --build build",
			"group": "build",
			"problemMatcher": []
		}
	]
}
EOF

# Create update-sdk.sh script
cat > "$APP_DIR/update-sdk.sh" <<'EOF'
#!/bin/bash

# update-sdk.sh - Update the brain-sdk submodule to the latest version from GitHub
# Usage: ./update-sdk.sh [--push]
#   --push: Automatically commit and push the update

set -e

PUSH=false
if [[ "$1" == "--push" ]]; then
  PUSH=true
fi

echo "Updating brain-sdk submodule..."
echo ""

cd brain-sdk
git fetch origin
git checkout main
git pull origin main
cd ..

echo ""
echo "✓ brain-sdk updated to latest version"
echo ""

if [ "$PUSH" = true ]; then
  echo "Committing and pushing update..."
  git add brain-sdk
  git commit -m "Updated brain-sdk"
  git push
  echo ""
  echo "✓ Changes committed and pushed"
else
  echo "Don't forget to commit the update:"
  echo "  git add brain-sdk"
  echo "  git commit -m \"Updated brain-sdk\""
  echo "  git push"
  echo ""
  echo "Or run: ./update-sdk.sh --push"
fi
EOF

chmod +x "$APP_DIR/update-sdk.sh"

echo "✓ Created directory structure"
echo "✓ Created CMakeLists.txt"
echo "✓ Created main.cpp"
echo "✓ Created .gitignore"
echo "✓ Created .gitmodules"
echo "✓ Created README.md"
echo "✓ Created .vscode configuration"
echo "✓ Created update-sdk.sh script"
echo ""
echo "Initializing git repository..."

# Initialize git repo and add brain-sdk as submodule
cd "$APP_DIR"
git init
git submodule add git@github.com:shmoergh/brain-sdk.git brain-sdk
git submodule update --init --recursive

echo ""
echo "✓ Initialized git repository"
echo "✓ Added brain-sdk as submodule"
echo ""
echo "Success! New Brain app created at: $APP_DIR"
echo ""
echo "Next steps:"
echo "  cd $APP_DIR"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make"
echo ""
echo "The compiled .uf2 file will be in build/$APP_NAME.uf2"
