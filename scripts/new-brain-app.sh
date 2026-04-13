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
SDK_URL="$(git -C "$SDK_DIR" remote get-url origin 2>/dev/null || echo "git@github.com:shmoergh/brain-sdk.git")"
SDK_BRANCH="$(git -C "$SDK_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "main")"

if [ "$SDK_BRANCH" = "HEAD" ] || [ -z "$SDK_BRANCH" ]; then
  SDK_BRANCH="main"
fi

if ! git ls-remote --exit-code --heads "$SDK_URL" "$SDK_BRANCH" >/dev/null 2>&1; then
  echo "Warning: Branch '$SDK_BRANCH' not found on remote '$SDK_URL'. Falling back to 'main'."
  SDK_BRANCH="main"
fi

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

# Default board/platform. Can be overridden at configure time with:
#   -DPICO_BOARD=<board>
#   -DPICO_PLATFORM=<platform>
if(NOT DEFINED PICO_BOARD)
	set(PICO_BOARD pico2)
endif()

if(NOT DEFINED PICO_PLATFORM)
	set(PICO_PLATFORM rp2350-arm-s)
endif()

# Include Pico SDK (from brain-sdk subdirectory)
include(brain-sdk/pico_sdk_import.cmake)

project($APP_NAME C CXX ASM)

# Reserve top-of-flash sectors for SDK-managed persistent storage.
set(BRAIN_STORAGE_ENABLE_FLASH_RESERVATION ON CACHE BOOL "" FORCE)
set(BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT OFF CACHE BOOL "" FORCE)
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()

# Initialize the Pico SDK
pico_sdk_init()

# Add brain-sdk which includes brain libraries
add_subdirectory(brain-sdk)

add_executable($APP_NAME main.cpp)

target_link_libraries($APP_NAME
	pico_stdlib
	brain)

pico_enable_stdio_usb($APP_NAME 1)
pico_enable_stdio_uart($APP_NAME 1)
pico_add_extra_outputs($APP_NAME)
EOF

# Create Brain feature config header (recommended shared config)
cat > "$APP_DIR/brain_user_config.h" <<EOF
#pragma once

// Explicit Brain SDK feature configuration.
// Set BRAIN_USE_ALL=1 for the easiest full-feature setup, or replace this with
// selective BRAIN_USE_* defines (LEDS, BUTTONS, STORAGE, OUTPUTS, INPUTS, POTS, MIDI_PARSER, MIDI_TO_CV, POT_MULTI_FUNCTION).
#define BRAIN_USE_ALL 1
EOF

# Create main.cpp with simple boilerplate
cat > "$APP_DIR/main.cpp" <<EOF
#include <pico/stdlib.h>
#include <stdio.h>

#include "brain_user_config.h"
#include "brain/brain.h"

int main() {
	stdio_init_all();

	Brain brain;
	BrainInitStatus init_status = brain.init_leds(LedMode::kSimple);
	if (!brain_init_succeeded(init_status)) {
		printf("Failed to initialize LEDs\\n");
		return 1;
	}
	brain.leds.off_all();

	printf("$APP_NAME started\\n");

	// Simple LED blink pattern
	uint led_index = 0;
	while (true) {
		brain.leds.toggle(led_index);
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
build-pico
build-pico-2
.DS_Store
*.lck
**/ai
EOF

# Create .gitmodules
cat > "$APP_DIR/.gitmodules" <<EOF
[submodule "brain-sdk"]
	path = brain-sdk
	url = $SDK_URL
	branch = $SDK_BRANCH
EOF

# Create README.md
cat > "$APP_DIR/README.md" <<EOF
# $APP_NAME

A Brain module application built with the brain-sdk.

## Build

\`\`\`bash
./build-firmware.sh
\`\`\`

## Flash

Flash one of the generated UF2 files to your Brain module by holding the BOOTSEL button while connecting it to your computer, then copy the .uf2 file to the mounted drive:

- \`$APP_NAME-pico.uf2\` (RP2040)
- \`$APP_NAME-pico-2.uf2\` (RP2350)

## Development

This project includes brain-sdk as a git submodule. To update the SDK:

\`\`\`bash
cd brain-sdk
git pull origin $SDK_BRANCH
cd ..
git add brain-sdk
git commit -m "Update brain-sdk"
\`\`\`

## Brain Feature Config

The app uses an explicit Brain SDK config in \`brain_user_config.h\`.
Edit that file to switch between \`BRAIN_USE_ALL\` and selective \`BRAIN_USE_*\` feature flags.
EOF

# Create .vscode/launch.json
cat > "$APP_DIR/.vscode/launch.json" <<EOF
{
    "version": "0.2.0",
    "configurations": [
        {   "name": "Pico Debug (RP2040)",
            "device": "RP2040",
            "gdbPath": "arm-none-eabi-gdb",
            "cwd": "\${workspaceFolder}",
            "executable": "\${workspaceFolder}/build-pico/$APP_NAME.elf",
            "preLaunchTask": "Build Pico (RP2040)",
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "openocd",
            "configFiles": [
                "interface/cmsis-dap.cfg",
                "target/rp2040.cfg"
            ],
            "svdFile": "\${env:PICO_SDK_PATH}/src/rp2040/hardware_regs/RP2040.svd",
            "runToEntryPoint": "main",
            "postRestartCommands": [
                "break main",
                "continue"
            ]
        },
        {   "name": "Pico 2 Debug (RP2350)",
            "device": "RP2350",
            "gdbPath": "arm-none-eabi-gdb",
            "cwd": "\${workspaceFolder}",
            "executable": "\${workspaceFolder}/build-pico-2/$APP_NAME.elf",
            "preLaunchTask": "Build Pico 2 (RP2350)",
            "request": "launch",
            "type": "cortex-debug",
            "servertype": "openocd",
            "configFiles": [
                "interface/cmsis-dap.cfg",
                "target/rp2350.cfg"
            ],
            "svdFile": "\${env:PICO_SDK_PATH}/src/rp2350/hardware_regs/RP2350.svd",
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
			"label": "Configure Pico (RP2040)",
			"type": "shell",
			"command": "cmake -S \${workspaceFolder} -B \${workspaceFolder}/build-pico -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040",
			"problemMatcher": []
		},
		{
			"label": "Build Pico (RP2040)",
			"type": "shell",
			"dependsOn": "Configure Pico (RP2040)",
			"command": "cmake --build \${workspaceFolder}/build-pico",
			"group": {
				"kind": "build",
				"isDefault": true
			},
			"problemMatcher": []
		},
		{
			"label": "Configure Pico 2 (RP2350)",
			"type": "shell",
			"command": "cmake -S \${workspaceFolder} -B \${workspaceFolder}/build-pico-2 -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s",
			"problemMatcher": []
		},
		{
			"label": "Build Pico 2 (RP2350)",
			"type": "shell",
			"dependsOn": "Configure Pico 2 (RP2350)",
			"command": "cmake --build \${workspaceFolder}/build-pico-2",
			"group": "build",
			"problemMatcher": []
		}
	]
}
EOF

# Create build-firmware.sh script
cat > "$APP_DIR/build-firmware.sh" <<EOF
#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
TARGET_NAME="$APP_NAME"

build_target() {
	local board="\$1"
	local platform="\$2"
	local build_dir="\$3"
	local output_file="\$4"

	echo "Configuring \${board} (\${platform})..."
	cmake -S "\$ROOT_DIR" -B "\$ROOT_DIR/\$build_dir" -DPICO_BOARD="\$board" -DPICO_PLATFORM="\$platform"

	echo "Building \${board} (\${platform})..."
	cmake --build "\$ROOT_DIR/\$build_dir"

	local uf2_path="\$ROOT_DIR/\$build_dir/\${TARGET_NAME}.uf2"
	if [[ ! -f "\$uf2_path" ]]; then
		echo "Expected UF2 file not found: \$uf2_path" >&2
		exit 1
	fi

	cp "\$uf2_path" "\$ROOT_DIR/\$output_file"
	echo "Saved \$output_file"
}

# Build order: Pico first, then Pico 2.
build_target "pico" "rp2040" "build-pico" "${APP_NAME}-pico.uf2"
build_target "pico2" "rp2350-arm-s" "build-pico-2" "${APP_NAME}-pico-2.uf2"

echo "Done."
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

SDK_BRANCH="$(git config -f .gitmodules submodule.brain-sdk.branch || echo "main")"

echo "Updating brain-sdk submodule..."
echo "Target branch: $SDK_BRANCH"
echo ""

cd brain-sdk
git fetch origin
git checkout "$SDK_BRANCH"
git pull origin "$SDK_BRANCH"
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
chmod +x "$APP_DIR/build-firmware.sh"

echo "✓ Created directory structure"
echo "✓ Created CMakeLists.txt"
echo "✓ Created main.cpp"
echo "✓ Created .gitignore"
echo "✓ Created .gitmodules"
echo "✓ Created README.md"
echo "✓ Created .vscode configuration"
echo "✓ Created build-firmware.sh script"
echo "✓ Created update-sdk.sh script"
echo ""
echo "Initializing git repository..."

# Initialize git repo and add brain-sdk as submodule
cd "$APP_DIR"
git init
git submodule add -b "$SDK_BRANCH" "$SDK_URL" brain-sdk
git submodule update --init --recursive

echo ""
echo "✓ Initialized git repository"
echo "✓ Added brain-sdk as submodule"
echo ""
echo "Success! New Brain app created at: $APP_DIR"
echo ""
echo "Next steps:"
echo "  cd $APP_DIR"
echo "  ./build-firmware.sh"
echo ""
echo "Generated UF2 files:"
echo "  $APP_NAME-pico.uf2"
echo "  $APP_NAME-pico-2.uf2"
