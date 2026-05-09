# Claude Code skills for Brain firmwares

The Brain SDK ships a small set of [Claude Code](https://docs.claude.com/en/docs/claude-code) skills that encode the patterns, gotchas, and decisions that come up every time you build a Brain firmware. They're plain markdown files with YAML frontmatter — Claude Code auto-discovers any markdown file in `.claude/skills/` and uses the `description` to decide when to apply each one.

## What ships

| Skill | When it fires |
|---|---|
| `brain-scaffold-app` | You want to start a new Brain firmware. Asks about archetype, output ranges, and target board, then runs `scripts/new-brain-app.sh` and patches the generated files. |
| `brain-audio-effect` | You're writing or editing an audio-effect firmware (filter, distortion, delay, reverb, etc.). Carries the no-floats-in-the-callback rule, `AudioProcessor` setup, and the bipolar coupling gotcha. |
| `brain-sequencer` | Sequencers, arpeggiators, MIDI tools, MIDI-to-CV. Pitch CV via the calibrated voltage API, integer tempo math, gate/trigger patterns. |
| `brain-cv-utility` | LFOs, ADSRs, sample-and-hold, slew limiters, quantizers. Lookup-table patterns, integer phase counters. |
| `brain-migrate` | An existing firmware needs to move forward to a newer Brain SDK version. Detects era, walks `docs/2.0_MIGRATION.md` / `docs/2.1_MIGRATION.md` step by step. |
| `brain-calibration` | A change touches storage, flash layout, or the output voltage API in a way that could break CV calibration preservation. Owns the full rule (CMake helper order, `load_calibration_from_flash()`, calibrated vs raw API, forbidden flags). Fires defensively when the user is about to remove storage code or simplify CMake in risky ways. |

Three cross-cutting rules are baked into every archetype skill:

1. **No `float` / `double` in hot loops.** RP2040 has no FPU; RP2350 has one but transcendentals are still expensive. Use Q15/Q31 fixed-point and lookup tables. Floats are fine in init code that runs once.
2. **Ask about output range up front.** Before writing code that drives an output, the skill asks unipolar (0..10 V) or bipolar (−5..+5 V) and inserts the appropriate `Outputs::set_output_range(...)` calls. The skills also cover the `AudioProcessor` gotcha: it forces bipolar on its claimed channel and `stop()` does not restore the prior range.
3. **Preserve CV calibration.** Every firmware needs `brain_storage_configure_flash_reservation()` in `CMakeLists.txt` (between `project()` and `pico_sdk_init()`) and `outputs.load_calibration_from_flash()` after `brain.init_all()`. The dedicated `brain-calibration` skill owns the full rule and fires defensively when changes risk breaking either.

## Installation

There are four ways to install. Pick whichever fits your workflow.

### 1. Script (recommended)

From the **firmware project root** (the project that has `brain-sdk` as a submodule):

```bash
./brain-sdk/scripts/install-claude-skills.sh
```

This creates `.claude/skills/` if needed and symlinks each `brain-sdk/.claude/skills/*.md` into it. Idempotent — re-run any time. Symlinks point into the submodule, so when you advance the submodule the skills update automatically.

### 2. Manual symlink (per-skill control)

If you only want a subset:

```bash
mkdir -p .claude/skills
ln -s ../../brain-sdk/.claude/skills/brain-audio-effect.md .claude/skills/
ln -s ../../brain-sdk/.claude/skills/brain-scaffold-app.md .claude/skills/
```

### 3. Manual copy (snapshot)

If you'd rather pin the skills to a particular SDK revision and update them deliberately:

```bash
cp brain-sdk/.claude/skills/brain-*.md .claude/skills/
```

Trade-off: the skills no longer auto-update when you bump the submodule. Re-copy when you want the new versions.

### 4. User-level install (every project)

Symlink (or copy) into `~/.claude/skills/` to make the skills available in every project on your machine. Useful if you only ever work with one Brain SDK version at a time:

```bash
ln -s "$(pwd)/brain-sdk/.claude/skills/brain-"*.md ~/.claude/skills/
```

## Should I commit `.claude/skills/brain-*.md` to my firmware repo?

Either is fine.

- **Commit them** — collaborators and CI agents pick up the skills automatically. Symlinks are tiny and version-locked to your `brain-sdk` submodule revision. Recommended for shared firmwares.
- **Gitignore them** — add `.claude/skills/brain-*.md` to `.gitignore` and let each contributor opt in via the installer. Good if you don't want skills tracked in the firmware repo.

## Writing your own skills

The skills are plain markdown. To add a project-specific skill, drop a new `.md` file into your firmware's `.claude/skills/` (not the submodule) with frontmatter:

```markdown
---
name: my-firmware-thing
description: Use when ... (specific enough to trigger correctly, narrow enough not to misfire)
---

# Body
…
```

Claude Code reads the `description` to decide whether the skill is relevant to a given user request, so make it specific. Reference real file paths and example code from your firmware in the body.

## See also

- [`scripts/install-claude-skills.sh`](../scripts/install-claude-skills.sh) — the installer.
- [`.claude/skills/`](../.claude/skills/) — the skill files themselves.
- [Claude Code docs: skills](https://docs.claude.com/en/docs/claude-code).
