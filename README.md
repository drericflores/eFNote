# eFNote 4.0.0

eFNote 4 is the native C++20/Qt6 evolution of eFNote 3. It is a tabbed
Linux text and Markdown editor created by Dr. Eric O. Flores.

## Phase 1 capabilities

- Native C++20 and Qt6 Widgets application
- Multiple movable and closable document tabs
- New, Open, Save, Save As, Close, and Exit
- Atomic UTF-8 file saving with `QSaveFile`
- Protection against losing unsaved documents
- Live GitHub-flavored Markdown preview
- Light and dark modes with persistent preferences
- Find with wraparound
- Line, column, and character status information
- Built-in Markdown cheat sheet
- Command-line opening of one or more files
- Embedded SVG resources

## Phase 2 capabilities

- Native PDF export through Qt PrintSupport
- DOCX and ODT export through Pandoc
- Modeless Find and Replace with next, previous, replace, replace-all, case,
  and whole-word controls
- Automatic 60-second recovery snapshots for modified documents
- Startup recovery after an interruption
- Editor-only, split, and preview-only Markdown modes
- Persistent preview-mode selection
- Live line, word, and character statistics

The finalized Python 3.0.0 implementation remains in `reference-python/` as the
behavioral reference during the native conversion.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/efnote
```

Open a document directly:

```bash
./build/efnote README.md
```

## Requirements

- CMake 3.21 or later
- C++20 compiler
- Qt 6.4 or later: Widgets, PrintSupport, and SVG

On Pop!_OS 24.04:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-svg-dev
```

## Next conversion phases

- Code-focused editor engine and automatic language recognition
- Markdown formatting commands
- Full session restoration and recent files
- Recent files and external-file change detection
- Syntax highlighting and document statistics
- Automated Qt tests

## License

GNU General Public License version 3 or later. See `LICENSE`.
