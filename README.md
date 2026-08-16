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

## Phase 3 capabilities

- Automatic language recognition
- Syntax highlighting for C/C++, Python, Markdown, JSON, and HTML
- Detected language displayed in the status bar

## Phase 4 capabilities

- Line-number gutter and active-line highlighting
- Matching-bracket assistance and automatic indentation
- Persistent word-wrap and visible-whitespace controls

## Phase 5 capabilities

- Markdown bold, italic, code, link, heading, quote, and list commands
- Recent-files menu with missing-file cleanup
- Restoration of previously open saved documents
- Go to Line, duplicate line, and move-line commands
- Uppercase and lowercase conversion

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

- External-file change detection, encoding, and line-ending safeguards
- Large-file protection and improved recovery management
- Automated Qt tests and Linux release integration

## License

GNU General Public License version 3 or later. See `LICENSE`.
