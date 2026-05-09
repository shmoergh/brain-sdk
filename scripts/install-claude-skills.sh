#!/bin/bash

# install-claude-skills.sh — Install Brain SDK Claude Code skills into a firmware project.
#
# Run this from the root of a firmware project that has brain-sdk as a submodule
# (or pass the path to the submodule as $1). It symlinks every skill file from
# brain-sdk/.claude/skills/ into ./.claude/skills/ so the firmware project picks
# them up automatically and stays in sync as the submodule advances.
#
# Usage:
#   ./brain-sdk/scripts/install-claude-skills.sh
#   ./brain-sdk/scripts/install-claude-skills.sh path/to/brain-sdk

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Resolve SDK location: arg overrides; otherwise the script's parent directory.
if [ -n "$1" ]; then
  SDK_DIR="$(cd "$1" 2>/dev/null && pwd)" || {
    echo "Error: '$1' is not a directory."
    exit 1
  }
else
  SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
fi

SKILLS_SRC="$SDK_DIR/.claude/skills"
if [ ! -d "$SKILLS_SRC" ]; then
  echo "Error: no skills directory at $SKILLS_SRC"
  exit 1
fi

# Target = current working directory's .claude/skills/
TARGET_DIR="$(pwd)/.claude/skills"
mkdir -p "$TARGET_DIR"

INSTALLED=0
UPDATED=0
SKIPPED=0

shopt -s nullglob
for src in "$SKILLS_SRC"/*.md; do
  base="$(basename "$src")"
  dst="$TARGET_DIR/$base"

  # Compute relative path from $TARGET_DIR to $src so the symlink survives moves.
  if command -v python3 >/dev/null 2>&1; then
    rel="$(python3 -c "import os,sys; print(os.path.relpath(sys.argv[1], sys.argv[2]))" "$src" "$TARGET_DIR")"
  else
    rel="$src"  # fall back to absolute path
  fi

  if [ -L "$dst" ]; then
    # Existing symlink — refresh it.
    ln -sfn "$rel" "$dst"
    UPDATED=$((UPDATED + 1))
  elif [ -e "$dst" ]; then
    echo "  skip: $base (a non-symlink file already exists at $dst — leaving it alone)"
    SKIPPED=$((SKIPPED + 1))
  else
    ln -s "$rel" "$dst"
    INSTALLED=$((INSTALLED + 1))
  fi
done

echo
echo "Brain SDK skills installed into $TARGET_DIR"
echo "  new:     $INSTALLED"
echo "  updated: $UPDATED"
echo "  skipped: $SKIPPED"
echo
echo "Next steps:"
echo "  - Open this project in Claude Code; the brain-* skills are auto-discovered."
echo "  - Optional: commit .claude/skills/brain-*.md so collaborators get them too,"
echo "    or add 'brain-*.md' under .claude/skills/ to .gitignore for opt-in install."
