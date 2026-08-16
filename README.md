# QuickNote 3.0.0

QuickNote is a tabbed Linux text and Markdown editor created by Dr. Eric O.
Flores. It combines straightforward editing with live Markdown rendering,
recovery, dark mode, source highlighting, and document export.

## Features

- UTF-8 text, Markdown, HTML, and source-code editing
- Live publication-style Markdown preview (`Ctrl+Shift+M`)
- PDF export through Qt
- DOCX and ODT export through Pandoc
- Atomic saves and automatic recovery
- Tabs, search/replace, dark mode, and Python highlighting
- Built-in Markdown cheat sheet
- Command-line and Linux file-manager integration

## Run from source

Requirements are Python 3, PySide6, and Pandoc:

```bash
sudo apt install python3-pyside6.qtwidgets python3-pyside6.qtprintsupport pandoc
python3 quicknote.py
python3 quicknote.py document.md
```

## Install for the current user

```bash
chmod +x scripts/install_local.sh
./scripts/install_local.sh
quicknote
```

This installs beneath `~/.local`, adds the desktop launcher and icon, and does
not require administrator privileges.

## Build a Debian package

```bash
chmod +x scripts/build_deb.sh
./scripts/build_deb.sh
sudo apt install ./dist/quicknote_2.0.0_all.deb
```

## Markdown workflow

Open a `.md` or `.markdown` file to display the live preview. Use
`View → Markdown Preview` or `Ctrl+Shift+M` to toggle it. The source remains
plain UTF-8 Markdown. Use `Help → Markdown Cheat Sheet` for syntax examples.

## Export

Use `File → Export` to create PDF, Microsoft Word (`.docx`), or OpenDocument
Text (`.odt`) output. PDF generation is native to Qt. DOCX and ODT use Pandoc.

## License

GNU General Public License, version 3 or later. See `LICENSE`.
