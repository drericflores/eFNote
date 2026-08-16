import sys
import os
import re # For regular expressions in syntax highlighter
import subprocess # For opening files/folders cross-platform
import shutil
import pathlib # For path manipulation in cross-platform folder opening
import hashlib # For auto-save path generation

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QTextEdit, QFileDialog, QTabWidget,
    QWidget, QVBoxLayout, QToolBar, QStatusBar, QMessageBox, QMenu,
    QInputDialog, QFontComboBox, QComboBox, QTextBrowser, QDockWidget,
    QDialog, QDialogButtonBox
)
from PySide6.QtGui import (
    QAction, QIcon, QTextCharFormat, QColor, QPalette,
    QSyntaxHighlighter, QTextCursor, QFont, QTextDocument, QKeySequence
)
from PySide6.QtCore import Qt, QTimer, QSize, QSaveFile, QRegularExpression, QFileInfo
from PySide6.QtPrintSupport import QPrinter

APP_NAME = "eFNote"
APP_VERSION = "3.0.0"
APP_ID = "io.github.drericflores.efnote"
BASE_DIR = pathlib.Path(__file__).resolve().parent

# Helper function to load icons from the local 'icons' directory with fallback
def get_icon(name):
    path = BASE_DIR / "icons" / f"{name}.svg"
    # First, try to load from the local path
    if path.exists():
        return QIcon(str(path))
    # As a fallback, try to get a themed icon (e.g., from system icon theme)
    pm = QIcon.fromTheme(name)
    if not pm.isNull():
        return pm
    # Last resort: return an empty icon if neither is found
    return QIcon()

# Function to reveal a file or folder in the native file manager
def reveal_in_file_manager(path):
    p = pathlib.Path(path)
    if not p.exists():
        print(f"Path does not exist: {path}") # For debugging
        return
    if sys.platform.startswith("win"):
        os.startfile(str(p))
    elif sys.platform == "darwin":
        subprocess.Popen(["open", str(p)])
    else:
        # xdg-open is a common standard on Linux for opening files/folders
        subprocess.Popen(["xdg-open", str(p)])

# Define a simple syntax highlighter for Python, now theme-aware and using QRegularExpression
class PythonHighlighter(QSyntaxHighlighter):
    def __init__(self, document, dark_mode=False):
        super().__init__(document)
        self.dark_mode = dark_mode
        self.highlighting_rules = []

        # Keyword format (colors adjusted for light/dark mode)
        keyword_color = QColor("#569CD6") if self.dark_mode else QColor("#0000FF")
        keyword_format = QTextCharFormat()
        keyword_format.setForeground(keyword_color)
        keyword_format.setFontWeight(QFont.Bold)
        keywords = [
            "False", "None", "True", "and", "as", "assert", "async", "await",
            "break", "class", "continue", "def", "del", "elif", "else",
            "except", "finally", "for", "from", "global", "if", "import",
            "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
            "return", "try", "while", "with", "yield"
        ]
        # Use QRegularExpression for efficient pattern matching
        self.highlighting_rules.extend([(QRegularExpression(r"\b%s\b" % w), keyword_format) for w in keywords])

        # String format (colors adjusted for light/dark mode)
        string_color = QColor("#CE9178") if self.dark_mode else QColor("#A31515")
        string_format = QTextCharFormat()
        string_format.setForeground(string_color)
        self.highlighting_rules.append((QRegularExpression(r'"[^"\n]*"|\'[^\'\n]*\''), string_format))

        # Comment format (colors adjusted for light/dark mode)
        comment_color = QColor("#6A9955") if self.dark_mode else QColor("#008000")
        comment_format = QTextCharFormat()
        comment_format.setForeground(comment_color)
        self.highlighting_rules.append((QRegularExpression(r"#.*"), comment_format))

        # Function format (colors adjusted for light/dark mode)
        function_color = QColor("#C586C0") if self.dark_mode else QColor("#800080")
        function_format = QTextCharFormat()
        function_format.setForeground(function_color)
        self.highlighting_rules.append((QRegularExpression(r"\b[A-Za-z_][A-Za-z0-9_]+(?=\()"), function_format))

    def highlightBlock(self, text):
        for pattern_regex, format in self.highlighting_rules:
            it = pattern_regex.globalMatch(text)
            while it.hasNext():
                match = it.next()
                self.setFormat(match.capturedStart(), match.capturedLength(), format)
        self.setCurrentBlockState(0)

class TextEditor(QTextEdit):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFont(QFont("Inter", 12)) # Use Inter font
        self.document().setModified(False) # Set initial modified state to false
        self.highlighter = None # Will be set based on file type
        self.current_file_path = None
        self.is_dark_mode = False
        self.rich_mode = False # New: Flag to indicate if content has rich text formatting

        # Apply rounded corners via QSS
        self.setStyleSheet("QTextEdit { border-radius: 8px; padding: 5px; }")

        # Context menu setup (example for custom context actions)
        self.setContextMenuPolicy(Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self.show_context_menu)

    def show_context_menu(self, pos):
        menu = self.createStandardContextMenu() # Get the default text edit context menu
        menu.addSeparator()

        # Custom action: Open Containing Folder
        open_folder_action = QAction("Open Containing Folder", self)
        if self.current_file_path:
            folder = os.path.dirname(self.current_file_path)
            if os.path.isdir(folder):
                open_folder_action.triggered.connect(lambda f=folder: reveal_in_file_manager(f))
            else:
                open_folder_action.setEnabled(False) # Disable if path is not a directory
        else:
            open_folder_action.setEnabled(False) # Disable if no file is open
        menu.addAction(open_folder_action)

        menu.exec(self.mapToGlobal(pos))


    def set_syntax_highlighter(self, file_path, dark_mode_enabled):
        """
        Sets the appropriate syntax highlighter based on file extension.
        For file types without a specific highlighter, it clears any active one.
        Note: .docx files are not natively supported by QTextEdit for editing or saving.
        """
        if self.highlighter:
            self.highlighter.setDocument(None) # Clear existing highlighter

        _, ext = os.path.splitext(file_path)
        ext = ext.lower()

        # Syntax highlighting is typically for plain text code, not rich text HTML
        if self.rich_mode and (ext == ".html" or ext == ".htm"):
            self.highlighter = None # No highlighter for HTML rich text
        elif ext == ".py":
            # Recreate highlighter with current dark_mode setting
            self.highlighter = PythonHighlighter(self.document(), dark_mode=dark_mode_enabled)
        # Add more 'elif' blocks here for other language highlighters (e.g., C++, Java, JS)
        # elif ext == ".cpp" or ext == ".h" or ext == ".hpp":
        #     self.highlighter = CppHighlighter(self.document(), dark_mode=dark_mode_enabled)
        # elif ext == ".js":
        #     self.highlighter = JavaScriptHighlighter(self.document(), dark_mode=dark_mode_enabled)
        else:
            self.highlighter = None # No specific highlighter for other types (e.g., .txt, .c, .java, .html, .css)

        # Re-apply syntax highlighting if a highlighter is set (or was cleared)
        if self.highlighter:
            self.highlighter.rehighlight()
        # For plain text modes, clearing undo/redo is generally good practice if formatting changes dramatically.
        # This will also ensure any previous rich text formatting is cleared when switching to plain text mode.
        if not self.rich_mode: # Only clear for plain text modes
            self.document().clearUndoRedoStacks()


    def set_dark_mode(self, enabled):
        """Applies dark mode styling to the text editor and updates highlighter."""
        self.is_dark_mode = enabled
        palette = self.palette()
        if enabled:
            palette.setColor(QPalette.Base, QColor("#21252b")) # Dracula background for editor
            palette.setColor(QPalette.Text, QColor("#abb2bf")) # Dracula foreground for editor
        else:
            palette.setColor(QPalette.Base, Qt.white)
            palette.setColor(QPalette.Text, Qt.black)
        self.setPalette(palette)
        # Recreate and apply syntax highlighting to ensure colors update based on new theme
        if self.current_file_path:
            self.set_syntax_highlighter(self.current_file_path, enabled)


class NoteTabWidget(QTabWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTabsClosable(True)
        self.tabCloseRequested.connect(self.close_tab)
        self.setMovable(True) # Allow tabs to be reordered
        self.setUsesScrollButtons(True) # Show scroll buttons if many tabs
        # Styles for light mode (dark mode QSS is handled by MainWindow)
        self.setStyleSheet("QTabWidget::pane { border: 0; } QTabBar::tab { border-radius: 5px 5px 0 0; padding: 5px; margin: 2px;} QTabBar::tab:selected { background: #e0e0e0; }")

    def add_new_tab(self, file_path=None, content=""):
        """Adds a new text editor tab."""
        editor = TextEditor()
        editor.setText(content)
        editor.current_file_path = file_path
        # New: Initialize rich_mode for the editor
        is_html = file_path and file_path.lower().endswith((".html", ".htm"))
        editor.rich_mode = is_html

        # Pass the current dark mode status to the new editor and its highlighter
        editor.set_dark_mode(self.parent().dark_mode_enabled)
        if file_path:
            editor.set_syntax_highlighter(file_path, self.parent().dark_mode_enabled)
            tab_name = os.path.basename(file_path)
        else:
            tab_name = "Untitled"
        index = self.addTab(editor, tab_name)
        self.setCurrentIndex(index)
        # Reliable modified marker: connect to modificationChanged
        editor.document().modificationChanged.connect(
            lambda modified_state, ed=editor: self.set_tab_modified(self.indexOf(ed), modified_state)
        )
        # Wire up other editor signals (e.g., for status bar updates)
        self.parent().wire_editor_signals(editor)
        return editor

    def set_tab_modified(self, index, modified):
        """Adds/removes '*' to/from tab title to indicate modification."""
        if index < 0 or index >= self.count(): return # Handle invalid index
        base_text = self.tabText(index).rstrip("*") # Always remove '*' first
        self.setTabText(index, base_text + ("*" if modified else ""))
        # Removed: self.widget(index).document().setModified(modified)
        # This is now managed by Qt's modificationChanged signal
        self.parent().setWindowModified(modified) # Update main window's modified status

    def close_tab(self, index):
        """Handles closing a tab, prompting to save if modified."""
        editor_to_close = self.widget(index)
        if editor_to_close.document().isModified():
            reply = QMessageBox.question(
                self,
                "Save Changes",
                f"Do you want to save changes to {self.tabText(index).rstrip('*')}?",
                QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel
            )
            if reply == QMessageBox.StandardButton.Save:
                # Pass the specific editor to be saved
                if not self.parent().save_file(editor=editor_to_close):
                    return # Save was cancelled, don't close tab
            elif reply == QMessageBox.StandardButton.Cancel:
                return # Don't close tab
        self.removeTab(index)
        # If no tabs left, create a new untitled tab
        if self.count() == 0:
            self.add_new_tab()

    def current_editor(self):
        """Returns the TextEditor widget of the currently active tab."""
        return self.currentWidget()

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("eFNote[*]")
        self.setGeometry(100, 100, 800, 600)

        self.dark_mode_enabled = False
        self.current_theme = "light" # "light" or "dark"

        self.setup_ui()
        self.apply_theme() # Apply initial theme

        # Auto-save timer
        self.auto_save_timer = QTimer(self)
        self.auto_save_timer.setInterval(60000) # Auto-save every 60 seconds (1 minute)
        self.auto_save_timer.timeout.connect(self.auto_save_all_tabs)
        self.auto_save_timer.start()

        self.load_auto_saved_files()

    def setup_ui(self):
        """Sets up the main window UI components."""
        self.tabs = NoteTabWidget(self)
        self.setCentralWidget(self.tabs)
        # Wire signals for the initial tab
        initial_editor = self.tabs.add_new_tab()
        self.wire_editor_signals(initial_editor)

        self.create_actions()
        self.create_menus()
        self.create_toolbar() # Toolbar now created without local styling
        self.create_markdown_preview()

        self.create_status_bar()

        # Connect current tab changed signal for status bar updates
        self.tabs.currentChanged.connect(self.update_status_bar_and_format_ui)
        self.tabs.currentChanged.connect(self.update_markdown_preview)
        # Initial update for format UI
        self.update_format_ui(initial_editor.currentCharFormat())


    def wire_editor_signals(self, editor):
        """Connects signals from a TextEditor instance for UI updates."""
        editor.cursorPositionChanged.connect(self.update_status_bar)
        editor.currentCharFormatChanged.connect(self.update_format_ui) # New: Connect for rich text UI update
        editor.textChanged.connect(
            lambda ed=editor: self.update_markdown_preview()
            if ed is self.tabs.current_editor() else None
        )

    def create_actions(self):
        """Creates QActions for menu and toolbar using standard key sequences and custom icon loader."""
        # File Actions
        self.new_action = QAction(get_icon("new"), "&New", self)
        self.new_action.setShortcut(QKeySequence.New)
        self.new_action.setStatusTip("Create a new note")
        self.new_action.triggered.connect(self.new_file)

        self.open_action = QAction(get_icon("open"), "&Open...", self)
        self.open_action.setShortcut(QKeySequence.Open)
        self.open_action.setStatusTip("Open an existing note")
        self.open_action.triggered.connect(self.open_file)

        self.save_action = QAction(get_icon("save"), "&Save", self)
        self.save_action.setShortcut(QKeySequence.Save)
        self.save_action.setStatusTip("Save the current note")
        self.save_action.triggered.connect(self.save_file)

        self.save_as_action = QAction(get_icon("save_as"), "Save &As...", self)
        self.save_as_action.setShortcut(QKeySequence.SaveAs)
        self.save_as_action.setStatusTip("Save the current note with a new name")
        self.save_as_action.triggered.connect(self.save_file_as)

        self.close_action = QAction("&Close", self)
        self.close_action.setShortcut(QKeySequence.Close)
        self.close_action.setStatusTip("Close the current document")
        self.close_action.triggered.connect(self.close_current_document)

        self.export_pdf_action = QAction("Export as &PDF...", self)
        self.export_pdf_action.setStatusTip("Export the rendered document as PDF")
        self.export_pdf_action.triggered.connect(lambda: self.export_document("pdf"))

        self.export_docx_action = QAction("Export as &Word (.docx)...", self)
        self.export_docx_action.setStatusTip("Export the document as Microsoft Word")
        self.export_docx_action.triggered.connect(lambda: self.export_document("docx"))

        self.export_odt_action = QAction("Export as &OpenDocument (.odt)...", self)
        self.export_odt_action.setStatusTip("Export the document as OpenDocument Text")
        self.export_odt_action.triggered.connect(lambda: self.export_document("odt"))

        self.exit_action = QAction(get_icon("exit"), "E&xit", self)
        self.exit_action.setShortcut(QKeySequence.Quit)
        self.exit_action.setStatusTip("Exit the application")
        self.exit_action.triggered.connect(self.close)

        # Edit Actions
        self.undo_action = QAction(get_icon("undo"), "&Undo", self)
        self.undo_action.setShortcut(QKeySequence.Undo)
        self.undo_action.setStatusTip("Undo the last action")
        self.undo_action.triggered.connect(lambda: self.tabs.current_editor().undo())

        self.redo_action = QAction(get_icon("redo"), "&Redo", self)
        self.redo_action.setShortcut(QKeySequence.Redo)
        self.redo_action.setStatusTip("Redo the last action")
        self.redo_action.triggered.connect(lambda: self.tabs.current_editor().redo())

        self.cut_action = QAction(get_icon("cut"), "Cu&t", self)
        self.cut_action.setShortcut(QKeySequence.Cut)
        self.cut_action.setStatusTip("Cut selected text")
        self.cut_action.triggered.connect(lambda: self.tabs.current_editor().cut())

        self.copy_action = QAction(get_icon("copy"), "&Copy", self)
        self.copy_action.setShortcut(QKeySequence.Copy)
        self.copy_action.setStatusTip("Copy selected text")
        self.copy_action.triggered.connect(lambda: self.tabs.current_editor().copy())

        self.paste_action = QAction(get_icon("paste"), "&Paste", self)
        self.paste_action.setShortcut(QKeySequence.Paste)
        self.paste_action.setStatusTip("Paste text from clipboard")
        self.paste_action.triggered.connect(lambda: self.tabs.current_editor().paste())

        self.select_all_action = QAction("Select &All", self)
        self.select_all_action.setShortcut(QKeySequence.SelectAll)
        self.select_all_action.setStatusTip("Select all text")
        self.select_all_action.triggered.connect(lambda: self.tabs.current_editor().selectAll())

        self.search_action = QAction(get_icon("search"), "&Search/Replace", self)
        self.search_action.setShortcut(QKeySequence.Find) # Using standard Find shortcut
        self.search_action.setStatusTip("Find or replace text")
        self.search_action.triggered.connect(self.show_search_dialog)

        # View Actions
        self.dark_mode_action = QAction("Dark &Mode", self, checkable=True)
        self.dark_mode_action.setShortcut(Qt.Key_F10)
        self.dark_mode_action.setStatusTip("Toggle dark mode")
        self.dark_mode_action.triggered.connect(self.toggle_dark_mode)

        self.markdown_preview_action = QAction("Markdown &Preview", self, checkable=True)
        self.markdown_preview_action.setShortcut(QKeySequence("Ctrl+Shift+M"))
        self.markdown_preview_action.setStatusTip("Show or hide the live Markdown preview")
        self.markdown_preview_action.setChecked(True)
        self.markdown_preview_action.triggered.connect(self.toggle_markdown_preview)

        # Help Actions
        self.about_action = QAction("&About eFNote", self)
        self.about_action.setStatusTip("Show information about eFNote")
        self.about_action.triggered.connect(self.show_about_dialog)

        self.how_to_use_action = QAction("&How to Use", self) # New action for How to Use
        self.how_to_use_action.setStatusTip("Learn how to use eFNote")
        self.how_to_use_action.triggered.connect(self.show_how_to_use_dialog)

        self.markdown_cheat_sheet_action = QAction("&Markdown Cheat Sheet", self)
        self.markdown_cheat_sheet_action.setStatusTip("Show Markdown syntax and examples")
        self.markdown_cheat_sheet_action.triggered.connect(self.show_markdown_cheat_sheet)

        # New: Formatting Actions
        self.bold_action = QAction(get_icon("bold"), "Bold", self)
        self.bold_action.setShortcut(QKeySequence.Bold)
        self.bold_action.setCheckable(True)
        self.bold_action.toggled.connect(self.toggle_bold)

        self.italic_action = QAction(get_icon("italic"), "Italic", self)
        self.italic_action.setShortcut(QKeySequence.Italic)
        self.italic_action.setCheckable(True)
        self.italic_action.toggled.connect(self.toggle_italic)

        self.underline_action = QAction(get_icon("underline"), "Underline", self)
        self.underline_action.setShortcut(QKeySequence.Underline)
        self.underline_action.setCheckable(True)
        self.underline_action.toggled.connect(self.toggle_underline)

        self.strike_action = QAction(get_icon("strikethrough"), "Strikethrough", self)
        self.strike_action.setCheckable(True)
        self.strike_action.toggled.connect(self.toggle_strike)


    def create_menus(self):
        """Creates the application's menu bar."""
        menu_bar = self.menuBar()

        file_menu = menu_bar.addMenu("&File")
        file_menu.addAction(self.new_action)
        file_menu.addAction(self.open_action)
        file_menu.addAction(self.close_action)
        file_menu.addSeparator()
        file_menu.addAction(self.save_action)
        file_menu.addAction(self.save_as_action)
        export_menu = file_menu.addMenu("&Export")
        export_menu.addAction(self.export_pdf_action)
        export_menu.addAction(self.export_docx_action)
        export_menu.addAction(self.export_odt_action)
        file_menu.addSeparator()
        file_menu.addAction(self.exit_action)

        edit_menu = menu_bar.addMenu("&Edit")
        edit_menu.addAction(self.undo_action)
        edit_menu.addAction(self.redo_action)
        edit_menu.addSeparator()
        edit_menu.addAction(self.cut_action)
        edit_menu.addAction(self.copy_action)
        edit_menu.addAction(self.paste_action)
        edit_menu.addSeparator()
        edit_menu.addAction(self.select_all_action)
        edit_menu.addAction(self.search_action)
        edit_menu.addSeparator() # Add separator for formatting
        edit_menu.addAction(self.bold_action)
        edit_menu.addAction(self.italic_action)
        edit_menu.addAction(self.underline_action)
        edit_menu.addAction(self.strike_action)

        view_menu = menu_bar.addMenu("&View")
        view_menu.addAction(self.dark_mode_action)
        view_menu.addAction(self.markdown_preview_action)

        help_menu = menu_bar.addMenu("&Help") # New Help menu
        help_menu.addAction(self.how_to_use_action) # Add How to Use action
        help_menu.addAction(self.markdown_cheat_sheet_action)
        help_menu.addAction(self.about_action)


    def create_toolbar(self):
        """Creates the application's toolbar and adds actions."""
        self.toolbar = QToolBar("Main Toolbar") # Assign to self.toolbar
        self.toolbar.setIconSize(QSize(24, 24)) # Set icon size
        self.toolbar.setMovable(False) # Prevent toolbar from being moved by user
        self.addToolBar(self.toolbar)

        # File actions
        self.toolbar.addAction(self.new_action)
        self.toolbar.addAction(self.open_action)
        self.toolbar.addAction(self.save_action)
        self.toolbar.addSeparator()

        # Edit actions
        self.toolbar.addAction(self.undo_action)
        self.toolbar.addAction(self.redo_action)
        self.toolbar.addAction(self.cut_action)
        self.toolbar.addAction(self.copy_action)
        self.toolbar.addAction(self.paste_action)
        self.toolbar.addSeparator()

        # Formatting actions
        self.toolbar.addAction(self.bold_action)
        self.toolbar.addAction(self.italic_action)
        self.toolbar.addAction(self.underline_action)
        self.toolbar.addAction(self.strike_action)

        # Font family and size controls
        self.font_box = QFontComboBox(self)
        self.font_box.currentFontChanged.connect(self.set_font_family)
        self.font_box.setToolTip("Select Font Family")
        self.toolbar.addWidget(self.font_box)

        self.size_box = QComboBox(self)
        self.size_box.setEditable(True)
        self.size_box.addItems([str(s) for s in (8,9,10,11,12,14,16,18,20,22,24,28,32,36,48,72)])
        self.size_box.currentTextChanged.connect(self.set_font_size)
        self.size_box.setToolTip("Select Font Size")
        self.toolbar.addWidget(self.size_box)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self.search_action)

        # Removed toolbar.setStyleSheet(...) - styling is now in apply_theme()

    def create_status_bar(self):
        """Creates the application's status bar."""
        self.status_bar = self.statusBar()
        self.status_bar.showMessage("Ready")
        # Wiring for status bar updates on tab changes is done in setup_ui

    def create_markdown_preview(self):
        """Creates the docked, publication-style Markdown preview."""
        self.markdown_preview = QTextBrowser(self)
        self.markdown_preview.setOpenExternalLinks(True)
        self.markdown_preview.document().setDefaultStyleSheet("""
            body { font-family: sans-serif; font-size: 11pt; line-height: 1.5; }
            h1 { font-size: 22pt; margin-bottom: 12px; }
            h2 { font-size: 18pt; border-bottom: 2px solid #d0d7de;
                 padding-bottom: 6px; margin-top: 22px; }
            h3 { font-size: 14pt; margin-top: 18px; }
            blockquote { color: #57606a; border-left: 4px solid #d0d7de;
                         margin-left: 0; padding-left: 12px; }
            code { font-family: monospace; background: #eaeef2; }
            pre { font-family: monospace; background: #eaeef2;
                  padding: 10px; white-space: pre-wrap; }
            table { border-collapse: collapse; }
            th, td { border: 1px solid #d0d7de; padding: 6px; }
        """)

        self.markdown_preview_dock = QDockWidget("Markdown Preview", self)
        self.markdown_preview_dock.setObjectName("MarkdownPreviewDock")
        self.markdown_preview_dock.setWidget(self.markdown_preview)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea,
                           self.markdown_preview_dock)
        self.markdown_preview_dock.hide()

    def current_file_is_markdown(self):
        editor = self.tabs.current_editor()
        if not editor or not editor.current_file_path:
            return False
        return editor.current_file_path.lower().endswith((".md", ".markdown"))

    def toggle_markdown_preview(self, checked):
        """Shows the preview only when the active document is Markdown."""
        if checked and not self.current_file_is_markdown():
            self.status_bar.showMessage(
                "Markdown preview becomes available when a .md file is active.", 4000
            )
        self.update_markdown_preview()

    def update_markdown_preview(self, *_args):
        """Renders the active Markdown document whenever its source changes."""
        editor = self.tabs.current_editor()
        should_show = (
            self.markdown_preview_action.isChecked()
            and self.current_file_is_markdown()
            and editor is not None
        )
        self.markdown_preview_dock.setVisible(should_show)
        if should_show:
            self.markdown_preview.setMarkdown(editor.toPlainText())

    def update_status_bar_and_format_ui(self):
        """Updates both the status bar and the formatting toolbar UI."""
        self.update_status_bar()
        editor = self.tabs.current_editor()
        if editor:
            self.update_format_ui(editor.currentCharFormat())
        else:
            # Reset format UI if no editor is active
            self.update_format_ui(QTextCharFormat())


    def update_status_bar(self):
        """
        Updates the status bar with current file info and modified status.
        Also updates the main window title.
        """
        editor = self.tabs.current_editor()
        if editor:
            file_path = editor.current_file_path
            display_name = os.path.basename(file_path) if file_path else "Untitled"
            is_modified = editor.document().isModified()

            # Update window title and modified dot
            self.setWindowTitle(f"eFNote - {display_name}[*]")
            self.setWindowModified(is_modified) # For OS-native dirty indicator

            cursor = editor.textCursor()
            line = cursor.blockNumber() + 1
            col = cursor.columnNumber() + 1
            status_text = f"{display_name}{' (Modified)' if is_modified else ''} | Line: {line}, Col: {col}"
            self.status_bar.showMessage(status_text)
        else:
            self.setWindowTitle("eFNote[*]")
            self.setWindowModified(False)
            self.status_bar.showMessage("Ready")


    def new_file(self):
        """Creates a new, empty tab."""
        new_editor = self.tabs.add_new_tab()
        self.update_status_bar_and_format_ui() # Update status bar and format UI for the new tab

    def close_current_document(self):
        """Closes the active document while preserving the existing save prompt."""
        index = self.tabs.currentIndex()
        if index >= 0:
            self.tabs.close_tab(index)

    def open_file(self):
        """
        Opens a file and loads its content into a new tab.
        Supports HTML for .html/.htm, otherwise plain text.
        """
        file_filters = (
            "All Files (*);;"
            "HTML Files (*.html *.htm);;" # HTML filter first for rich text
            "Text Files (*.txt);;"
            "Python Files (*.py);;"
            "C++ Files (*.cpp *.cxx *.cc *.h *.hpp);;"
            "Java Files (*.java);;"
            "JavaScript Files (*.js);;"
            "CSS Files (*.css);;"
            "JSON Files (*.json);;"
            "Markdown Files (*.md *.markdown)"
        )
        file_path, _ = QFileDialog.getOpenFileName(self, "Open Note", "", file_filters)
        if file_path:
            self.open_file_path(file_path)

    def open_file_path(self, file_path):
        """Opens a path supplied by the dialog, command line, or desktop shell."""
        try:
            is_html = file_path.lower().endswith((".html", ".htm"))
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()

            current = self.tabs.current_editor()
            reuse_empty_tab = (
                self.tabs.count() == 1
                and current is not None
                and current.current_file_path is None
                and not current.document().isModified()
                and not current.toPlainText()
            )
            if reuse_empty_tab:
                editor = current
                editor.current_file_path = file_path
                self.tabs.setTabText(self.tabs.currentIndex(), os.path.basename(file_path))
            else:
                editor = self.tabs.add_new_tab(file_path)
            if is_html:
                editor.setHtml(content)
                editor.rich_mode = True
            else:
                editor.setPlainText(content)
                editor.rich_mode = False

            editor.set_syntax_highlighter(file_path, self.dark_mode_enabled)
            editor.document().setModified(False)
            self.update_status_bar_and_format_ui()
            self.update_markdown_preview()
            return True
        except Exception as error:
            QMessageBox.warning(
                self, "Open Failed",
                f"Could not open '{file_path}':\n{error}\n\n"
                "eFNote supports UTF-8 text, Markdown, source code, and HTML files."
            )
            return False

    def save_file(self, editor=None):
        """
        Saves the content of the given editor (or current tab if None) to its associated file path.
        Saves as HTML if rich_mode or HTML extension, otherwise plain text.
        Uses QSaveFile for atomic, safer writes. No overwrite confirmation here.
        """
        editor = editor or self.tabs.current_editor()
        if not editor:
            return False # No editor active

        # If no file path, call save_file_as to get one
        if not editor.current_file_path:
            # save_file_as will set editor.current_file_path and then call save_file again.
            # If save_file_as returns False (user cancelled), then return False here.
            return self.save_file_as(editor=editor)

        file_path = editor.current_file_path
        # Use QSaveFile for safer, atomic writes
        save_file_obj = QSaveFile(file_path)

        # Removed: QFileInfo(file_path).exists() and overwrite confirmation from here.
        # This check and prompt are now only in save_file_as.

        if not save_file_obj.open(QSaveFile.WriteOnly | QSaveFile.Text):
            QMessageBox.warning(self, "Error", f"Could not open '{file_path}' for writing:\n{save_file_obj.errorString()}")
            return False

        try:
            # Determine content to save: HTML if rich_mode or HTML extension, else plain text
            is_html_extension = file_path.lower().endswith((".html", ".htm"))
            if is_html_extension:
                data = editor.document().toHtml().encode("utf-8")
            else:
                data = editor.toPlainText().encode("utf-8")
            
            save_file_obj.write(data)
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Error writing to file '{file_path}':\n{e}")
            save_file_obj.cancelWriting() # Discard partial write
            return False

        if not save_file_obj.commit():
            QMessageBox.warning(self, "Error", f"Could not commit changes to '{file_path}':\n{save_file_obj.errorString()}")
            return False

        editor.document().setModified(False) # Qt's own tracking, not forced
        self.tabs.set_tab_modified(self.tabs.indexOf(editor), False)
        self.status_bar.showMessage(f"Saved {os.path.basename(file_path)}", 3000) # Message for 3 seconds
        self.update_status_bar_and_format_ui() # Update window title etc. and format UI
        # Delete auto-saved temporary file after successful real save
        self.cleanup_auto_save_file(file_path)
        return True

    def save_file_as(self, editor=None):
        """
        Saves the content of the given editor (or current tab if None) to a new file path.
        Includes common programming language extensions in the filter.
        Prompts for overwrite confirmation if file exists.
        Warns if saving rich text to a plain-text format.
        """
        editor = editor or self.tabs.current_editor()
        if not editor:
            return False # No editor active

        file_filters = (
            "HTML Files (*.html *.htm);;" # HTML first
            "All Files (*);;"
            "Text Files (*.txt);;"
            "Python Files (*.py);;"
            "C++ Files (*.cpp *.cxx *.cc *.h *.hpp);;"
            "Java Files (*.java);;"
            "JavaScript Files (*.js);;"
            "CSS Files (*.css);;"
            "JSON Files (*.json);;"
            "Markdown Files (*.md *.markdown)"
        )
        file_path, _ = QFileDialog.getSaveFileName(self, "Save Note As", "", file_filters)
        if file_path:
            target_ext = os.path.splitext(file_path)[1].lower()

            # Warn if saving rich text to a plain-text format
            if editor.rich_mode and not (target_ext == ".html" or target_ext == ".htm"):
                QMessageBox.information(self, "Formatting Will Be Lost",
                                        "You are saving to a plain-text format; rich formatting will be discarded. "
                                        "To preserve formatting, save as .html or .htm.")

            # Note: Saving as .docx is not supported. This will save plain text with a .docx extension.
            elif target_ext == ".docx":
                 QMessageBox.information(self, "Warning", "Saving as .docx is not supported. The file will be saved as plain text with a .docx extension.")


            # Confirmation for overwrite belongs only in Save As
            if QFileInfo(file_path).exists():
                reply = QMessageBox.question(
                    self, "Confirm Overwrite",
                    f"The file '{os.path.basename(file_path)}' already exists. Do you want to overwrite it?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )
                if reply == QMessageBox.StandardButton.No:
                    return False # User chose not to overwrite

            editor.current_file_path = file_path
            editor.set_syntax_highlighter(file_path, self.dark_mode_enabled) # Update highlighter for new extension
            self.tabs.setTabText(self.tabs.indexOf(editor), os.path.basename(file_path)) # Update tab name
            return self.save_file(editor=editor) # Now save to the new path (which will use QSaveFile)
        return False

    def export_document(self, output_format):
        """Exports the active document to PDF, DOCX, or ODT."""
        editor = self.tabs.current_editor()
        if not editor:
            QMessageBox.information(self, "Export", "No document is open.")
            return

        formats = {
            "pdf": ("PDF Document (*.pdf)", ".pdf"),
            "docx": ("Microsoft Word Document (*.docx)", ".docx"),
            "odt": ("OpenDocument Text (*.odt)", ".odt"),
        }
        file_filter, extension = formats[output_format]

        if editor.current_file_path:
            source_path = pathlib.Path(editor.current_file_path)
            suggested_path = str(source_path.with_suffix(extension))
        else:
            suggested_path = f"eFNote-Document{extension}"

        output_path, _ = QFileDialog.getSaveFileName(
            self,
            f"Export as {output_format.upper()}",
            suggested_path,
            file_filter,
        )
        if not output_path:
            return
        if not output_path.lower().endswith(extension):
            output_path += extension

        if output_format == "pdf":
            self.export_pdf(editor, output_path)
        else:
            self.export_with_pandoc(editor, output_path, output_format)

    def export_pdf(self, editor, output_path):
        """Uses Qt to produce PDF without requiring a LaTeX installation."""
        document = QTextDocument(self)
        file_path = (editor.current_file_path or "").lower()
        if file_path.endswith((".md", ".markdown")):
            document.setMarkdown(editor.toPlainText())
        elif file_path.endswith((".html", ".htm")) or editor.rich_mode:
            document.setHtml(editor.document().toHtml())
        else:
            document.setPlainText(editor.toPlainText())

        printer = QPrinter(QPrinter.PrinterMode.HighResolution)
        printer.setOutputFormat(QPrinter.OutputFormat.PdfFormat)
        printer.setOutputFileName(output_path)
        printer.setDocName(pathlib.Path(output_path).stem)

        try:
            document.print_(printer)
        except Exception as error:
            QMessageBox.critical(self, "PDF Export Failed", str(error))
            return

        self.status_bar.showMessage(f"Exported {os.path.basename(output_path)}", 5000)

    def export_with_pandoc(self, editor, output_path, output_format):
        """Uses Pandoc for standards-compliant DOCX and ODT generation."""
        pandoc = shutil.which("pandoc")
        if not pandoc:
            QMessageBox.critical(
                self,
                "Pandoc Required",
                "DOCX and ODT export require Pandoc. Install it with:\n\n"
                "sudo apt install pandoc",
            )
            return

        file_path = (editor.current_file_path or "").lower()
        if file_path.endswith((".html", ".htm")) or editor.rich_mode:
            input_format = "html"
            source = editor.document().toHtml()
        else:
            input_format = "gfm" if file_path.endswith((".md", ".markdown")) else "markdown"
            source = editor.toPlainText()

        title = pathlib.Path(editor.current_file_path).stem if editor.current_file_path else "eFNote Document"
        command = [
            pandoc,
            "--from", input_format,
            "--to", output_format,
            "--standalone",
            "--metadata", f"title={title}",
            "--output", output_path,
        ]
        result = subprocess.run(
            command,
            input=source,
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            detail = result.stderr.strip() or "Pandoc returned an unknown error."
            QMessageBox.critical(self, "Export Failed", detail)
            return

        self.status_bar.showMessage(f"Exported {os.path.basename(output_path)}", 5000)

    def toggle_dark_mode(self, checked):
        """Toggles between light and dark themes."""
        self.dark_mode_enabled = checked
        self.apply_theme()

    def apply_theme(self):
        """Applies the selected theme to the application."""
        app = QApplication.instance()

        if self.dark_mode_enabled:
            # Dark mode palette (Dracula-like)
            palette = QPalette()
            palette.setColor(QPalette.Window, QColor("#282c34")) # Background
            palette.setColor(QPalette.WindowText, QColor("#abb2bf")) # General Text
            palette.setColor(QPalette.Base, QColor("#21252b")) # TextEdit background
            palette.setColor(QPalette.AlternateBase, QColor("#3e4452"))
            palette.setColor(QPalette.ToolTipBase, QColor("#282c34"))
            palette.setColor(QPalette.ToolTipText, QColor("#abb2bf"))
            palette.setColor(QPalette.Text, QColor("#abb2bf")) # Foreground
            palette.setColor(QPalette.Button, QColor("#3e4452")) # Button background
            palette.setColor(QPalette.ButtonText, QColor("#abb2bf")) # Button text
            palette.setColor(QPalette.BrightText, Qt.red)
            palette.setColor(QPalette.Link, QColor("#61afef")) # Link color
            palette.setColor(QPalette.Highlight, QColor("#61afef")) # Selection highlight
            palette.setColor(QPalette.HighlightedText, QColor("#282c34")) # Selected text color
            app.setPalette(palette)
            self.setStyleSheet("""
                QMainWindow { background-color: #282c34; }
                QMenuBar { background-color: #21252b; color: #abb2bf; }
                QMenuBar::item:selected { background-color: #3e4452; }
                QMenu { background-color: #21252b; color: #abb2bf; border: 1px solid #3e4452; }
                QMenu::item:selected { background-color: #3e4452; }
                QToolBar { background-color: #21252b; border-bottom: 1px solid #3e4452; }
                QStatusBar { background-color: #21252b; color: #abb2bf; border-top: 1px solid #3e4452; }
                QTabWidget::pane { border: 1px solid #3e4452; }
                QTabBar::tab { background: #3e4452; color: #abb2bf; border: 1px solid #3e4452; border-bottom-color: #282c34; border-radius: 5px 5px 0 0; padding: 5px; margin: 2px;}
                QTabBar::tab:selected { background: #282c34; border-bottom-color: #282c34; }
                QTextEdit { background-color: #21252b; color: #abb2bf; border: 1px solid #3e4452; border-radius: 8px; padding: 5px; }
            """)
            if hasattr(self, "toolbar"):
                self.toolbar.setToolButtonStyle(Qt.ToolButtonTextBesideIcon) # Set to text beside icon
                self.toolbar.setStyleSheet("") # Inherit style from QMainWindow QSS
        else:
            # Light mode palette (reset to default system palette)
            app.setPalette(app.style().standardPalette())
            self.setStyleSheet("") # Clear custom stylesheet for main window
            self.tabs.setStyleSheet("QTabWidget::pane { border: 0; } QTabBar::tab { border-radius: 5px 5px 0 0; padding: 5px; margin: 2px;} QTabBar::tab:selected { background: #e0e0e0; }")
            if hasattr(self, "toolbar"):
                self.toolbar.setToolButtonStyle(Qt.ToolButtonTextBesideIcon) # Set to text beside icon
                self.toolbar.setStyleSheet("") # Inherit style from QMainWindow QSS (native look)

        # Apply dark mode setting to all existing TextEditor instances (which will re-apply highlighters)
        for i in range(self.tabs.count()):
            editor = self.tabs.widget(i)
            if isinstance(editor, TextEditor):
                editor.set_dark_mode(self.dark_mode_enabled)

    # New: Formatting Helper Functions
    def apply_char_format(self, fmt: QTextCharFormat):
        ed = self.tabs.current_editor()
        if not ed: return
        cursor = ed.textCursor()
        if cursor.hasSelection():
            cursor.mergeCharFormat(fmt)
            ed.setTextCursor(cursor) # Apply changes
        ed.mergeCurrentCharFormat(fmt)  # affects future typing
        ed.rich_mode = True  # mark as rich once any format applied
        # Re-apply syntax highlighter based on rich_mode
        ed.set_syntax_highlighter(ed.current_file_path or "", self.dark_mode_enabled)


    def toggle_bold(self, checked):
        fmt = QTextCharFormat()
        fmt.setFontWeight(QFont.Weight.Bold if checked else QFont.Weight.Normal)
        self.apply_char_format(fmt)

    def toggle_italic(self, checked):
        fmt = QTextCharFormat()
        fmt.setFontItalic(checked)
        self.apply_char_format(fmt)

    def toggle_underline(self, checked):
        fmt = QTextCharFormat()
        fmt.setFontUnderline(checked)
        self.apply_char_format(fmt)

    def toggle_strike(self, checked):
        fmt = QTextCharFormat()
        fmt.setFontStrikeOut(checked)
        self.apply_char_format(fmt)

    def set_font_family(self, font: QFont):
        fmt = QTextCharFormat()
        fmt.setFontFamily(font.family())
        self.apply_char_format(fmt)

    def set_font_size(self, pts_str: str):
        try:
            pts = float(pts_str)
        except ValueError:
            return
        fmt = QTextCharFormat()
        fmt.setFontPointSize(pts)
        self.apply_char_format(fmt)

    def update_format_ui(self, fmt: QTextCharFormat):
        """Updates the formatting toolbar buttons/comboboxes to reflect the current format."""
        # Block signals to prevent feedback loops when setting UI states
        self.bold_action.blockSignals(True)
        self.bold_action.setChecked(fmt.fontWeight() >= QFont.Bold)
        self.bold_action.blockSignals(False)

        self.italic_action.blockSignals(True)
        self.italic_action.setChecked(fmt.fontItalic())
        self.italic_action.blockSignals(False)

        self.underline_action.blockSignals(True)
        self.underline_action.setChecked(fmt.fontUnderline())
        self.underline_action.blockSignals(False)

        self.strike_action.blockSignals(True)
        self.strike_action.setChecked(fmt.fontStrikeOut())
        self.strike_action.blockSignals(False)

        self.font_box.blockSignals(True)
        self.font_box.setCurrentFont(fmt.font())
        self.font_box.blockSignals(False)

        if fmt.fontPointSize() > 0: # Only set if a valid point size exists
            self.size_box.blockSignals(True)
            # Find the index of the point size in the combobox, if it exists
            idx = self.size_box.findText(str(int(fmt.fontPointSize())))
            if idx != -1:
                self.size_box.setCurrentIndex(idx)
            else: # If not in list, add it or just set text
                self.size_box.setEditText(str(int(fmt.fontPointSize())))
            self.size_box.blockSignals(False)
        else:
            # Optionally clear the size box or set to a default if no size is found
            self.size_box.blockSignals(True)
            self.size_box.setCurrentText("") # Or a default like "12"
            self.size_box.blockSignals(False)


    def show_search_dialog(self):
        """Shows a basic search/replace dialog."""
        editor = self.tabs.current_editor()
        if not editor:
            QMessageBox.information(self, "Search", "No document open to search.")
            return

        search_text, ok = QInputDialog.getText(self, "Search", "Enter text to find:")
        if ok and search_text:
            cursor = editor.textCursor()
            # Start search from current cursor position, or from start if not found
            found = editor.find(search_text, QTextDocument.FindFlags(0))
            if not found:
                # If not found from current position, try from beginning
                cursor.setPosition(0)
                editor.setTextCursor(cursor)
                found = editor.find(search_text, QTextDocument.FindFlags(0))

            if not found:
                QMessageBox.information(self, "Search", f"'{search_text}' not found.")
            else:
                self.status_bar.showMessage(f"Found '{search_text}'")

            # Basic Replace (could be extended to a full dialog)
            replace_reply = QMessageBox.question(self, "Replace", f"Replace '{search_text}'?",
                                                 QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if replace_reply == QMessageBox.StandardButton.Yes:
                replace_text, ok_replace = QInputDialog.getText(self, "Replace", f"Replace '{search_text}' with:")
                if ok_replace:
                    cursor.insertText(replace_text)
                    self.status_bar.showMessage(f"Replaced '{search_text}' with '{replace_text}'")

    def show_about_dialog(self):
        """Displays the 'About eFNote' information dialog."""
        about_text = (
            "<h2>eFNote</h2>"
            "<p>By: Dr. Eric O. Flores</p>"
            f"<p>Version: {APP_VERSION}</p>"
            "<p>Date: August 2026</p>"
            "<p>EFRAD Generated Application</p>"
            "<hr>"
            "<h3>Technologies Used:</h3>"
            "<ul>"
            "<li>Python</li>"
            "<li>PySide6 (Qt for Python)</li>"
            "<li>Qt Framework (underlying GUI library)</li>"
            "<li><code>os</code> module (file system operations)</li>"
            "<li><code>subprocess</code> module (cross-platform file manager)</li>"
            "<li><code>pathlib</code> module (object-oriented file paths)</li>"
            "<li><code>re</code> module (regular expressions for syntax highlighting)</li>"
            "<li><code>hashlib</code> module (for auto-save path generation)</li>"
            "</ul>"
        )
        QMessageBox.about(self, "About eFNote", about_text)

    def show_markdown_cheat_sheet(self):
        """Displays a scrollable reference for commonly used Markdown syntax."""
        dialog = QDialog(self)
        dialog.setWindowTitle("eFNote Markdown Cheat Sheet")
        dialog.resize(820, 680)

        browser = QTextBrowser(dialog)
        browser.setOpenExternalLinks(True)
        browser.setHtml("""
        <h1>Markdown Cheat Sheet</h1>
        <p>Write the syntax shown in the <b>Markdown source</b> column. The live
        preview displays the published result immediately.</p>
        <table cellspacing="0" cellpadding="7" border="1">
          <tr><th>Purpose</th><th>Markdown source</th><th>Rendered result</th></tr>
          <tr><td>Heading 1</td><td><code># Heading</code></td><td><h1>Heading</h1></td></tr>
          <tr><td>Heading 2</td><td><code>## Heading</code></td><td><h2>Heading</h2></td></tr>
          <tr><td>Bold</td><td><code>**bold text**</code></td><td><b>bold text</b></td></tr>
          <tr><td>Italic</td><td><code>*italic text*</code></td><td><i>italic text</i></td></tr>
          <tr><td>Bold italic</td><td><code>***important***</code></td><td><b><i>important</i></b></td></tr>
          <tr><td>Strikethrough</td><td><code>~~removed~~</code></td><td><s>removed</s></td></tr>
          <tr><td>Bullet list</td><td><code>- First<br>- Second</code></td><td>• First<br>• Second</td></tr>
          <tr><td>Numbered list</td><td><code>1. First<br>2. Second</code></td><td>1. First<br>2. Second</td></tr>
          <tr><td>Task list</td><td><code>- [x] Done<br>- [ ] Pending</code></td><td>☑ Done<br>☐ Pending</td></tr>
          <tr><td>Link</td><td><code>[OpenAI](https://openai.com)</code></td><td><a href="https://openai.com">OpenAI</a></td></tr>
          <tr><td>Image</td><td><code>![Description](image.png)</code></td><td>Embedded image</td></tr>
          <tr><td>Blockquote</td><td><code>&gt; Quoted text</code></td><td><blockquote>Quoted text</blockquote></td></tr>
          <tr><td>Inline code</td><td><code>`command`</code></td><td><code>command</code></td></tr>
          <tr><td>Horizontal rule</td><td><code>---</code></td><td><hr></td></tr>
        </table>

        <h2>Fenced code block</h2>
        <pre>```python
print("Hello from eFNote")
```</pre>

        <h2>Table</h2>
        <pre>| Instrument | Status |
|------------|--------|
| Fluke 287  | Ready  |
| OWON DMM   | Review |</pre>

        <h2>Useful controls</h2>
        <ul>
          <li><b>Ctrl+Shift+M</b> — show or hide Markdown preview</li>
          <li><b>Ctrl+S</b> — save the Markdown source</li>
          <li><b>Ctrl+W</b> — close the current document</li>
          <li><b>File → Export</b> — create PDF, DOCX, or ODT</li>
        </ul>
        """)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close, dialog)
        buttons.rejected.connect(dialog.reject)
        buttons.accepted.connect(dialog.accept)
        layout = QVBoxLayout(dialog)
        layout.addWidget(browser)
        layout.addWidget(buttons)
        dialog.exec()

    def show_how_to_use_dialog(self):
        """Displays a dialog with instructions on how to use eFNote."""
        how_to_use_text = (
            "<h2>How to Use eFNote</h2>"
            "<p>eFNote is a simple, tabbed text editor. Here's how to get started:</p>"
            "<h3>File Operations:</h3>"
            "<ul>"
            "<li><b>New (Ctrl+N):</b> Create a new, empty note in a new tab.</li>"
            "<li><b>Open (Ctrl+O):</b> Open an existing text file. eFNote supports various programming language files as plain text, and HTML files for rich text.</li>"
            "<li><b>Save (Ctrl+S):</b> Save the current note to its file. If it's a new note, it will prompt for a file name. Formatting is preserved for .html/.htm files, lost for others.</li>"
            "<li><b>Save As (Ctrl+Shift+S):</b> Save the current note to a new file, prompting for a file name and location. Warns if rich formatting will be lost.</li>"
            "<li><b>Exit (Ctrl+Q):</b> Close the application. You'll be prompted to save any unsaved changes.</li>"
            "</ul>"
            "<h3>Editing & Formatting:</h3>"
            "<ul>"
            "<li><b>Basic Editing:</b> Undo (Ctrl+Z), Redo (Ctrl+Y), Cut (Ctrl+X), Copy (Ctrl+C), Paste (Ctrl+V), Select All (Ctrl+A).</li>"
            "<li><b>Search/Replace (Ctrl+F):</b> Find text within your note.</li>"
            "<li><b>Bold (Ctrl+B), Italic (Ctrl+I), Underline (Ctrl+U), Strikethrough:</b> Apply text formatting.</li>"
            "<li><b>Font Family & Size:</b> Change the font and size of selected text or future typing using the toolbar controls.</li>"
            "<li><b>Toolbar State:</b> Formatting buttons on the toolbar will automatically reflect the style of the text under your cursor.</li>"
            "</ul>"
            "<h3>Tabs:</h3>"
            "<ul>"
            "<li>Click on a tab to switch between notes.</li>"
            "<li>Click the 'x' button on a tab to close it. You'll be prompted to save if there are unsaved changes.</li>"
            "</ul>"
            "<h3>Appearance:</h3>"
            "<ul>"
            "<li><b>Dark Mode (F10):</b> Toggle between light and dark themes for comfortable viewing.</li>"
            "</ul>"
            "<h3>Auto-Save:</h3>"
            "<p>Your notes are automatically saved to a temporary location every minute. If eFNote closes unexpectedly, you may be prompted to restore these auto-saved files on next launch. Auto-saved files will preserve rich text formatting if any has been applied.</p>"
        )
        QMessageBox.information(self, "How to Use eFNote", how_to_use_text)


    def get_auto_save_path(self, original_file_path):
        """Generates a stable auto-save path based on original file path or a unique ID."""
        auto_save_dir = os.path.join(os.path.expanduser("~"), ".efnote_autosave")
        os.makedirs(auto_save_dir, exist_ok=True)
        if original_file_path:
            # Use a hash of the absolute path for a stable temp name
            hash_object = hashlib.sha256(original_file_path.encode())
            return os.path.join(auto_save_dir, f"{hash_object.hexdigest()}.tmp")
        else:
            # For "Untitled" files, generate a random ID
            return os.path.join(auto_save_dir, f"untitled_{os.urandom(8).hex()}.tmp")

    def auto_save_all_tabs(self):
        """Auto-saves all modified tabs to a temporary directory using stable paths."""
        for i in range(self.tabs.count()):
            editor = self.tabs.widget(i)
            if isinstance(editor, TextEditor) and editor.document().isModified():
                temp_file_path = self.get_auto_save_path(editor.current_file_path)
                try:
                    # Save as HTML if rich_mode is enabled, otherwise plain text
                    data_to_save = editor.document().toHtml() if editor.rich_mode else editor.toPlainText()
                    with open(temp_file_path, 'w', encoding='utf-8') as f:
                        f.write(data_to_save)
                    self.status_bar.showMessage(f"Auto-saved: {os.path.basename(editor.current_file_path or 'Untitled')}", 2000)
                except Exception as e:
                    print(f"Error auto-saving tab {i} ('{editor.current_file_path or 'Untitled'}'): {e}")

    def cleanup_auto_save_file(self, original_file_path):
        """Deletes the auto-saved temporary file associated with a successfully saved original file."""
        if original_file_path:
            temp_file_path = self.get_auto_save_path(original_file_path)
            if os.path.exists(temp_file_path):
                try:
                    os.remove(temp_file_path)
                    print(f"Cleaned up auto-save for: {original_file_path}")
                except Exception as e:
                    print(f"Error cleaning up auto-save file '{temp_file_path}': {e}")


    def load_auto_saved_files(self):
        """Loads auto-saved files when the application starts."""
        auto_save_dir = os.path.join(os.path.expanduser("~"), ".efnote_autosave")
        if not os.path.exists(auto_save_dir):
            return

        auto_saved_files = [f for f in os.listdir(auto_save_dir) if f.endswith(".tmp")]
        if auto_saved_files:
            reply = QMessageBox.question(
                self,
                "Restore Auto-saved Files",
                "eFNote found auto-saved files from a previous session. Do you want to restore them?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.Yes:
                for temp_file_name in auto_saved_files:
                    temp_file_path = os.path.join(auto_save_dir, temp_file_name)
                    try:
                        with open(temp_file_path, 'r', encoding='utf-8') as f:
                            content = f.read()
                        
                        # Add as a new untitled tab
                        editor = self.tabs.add_new_tab(content=content)
                        # Determine if the auto-saved content was rich text (heuristic: check for common HTML tags)
                        # A more robust check might be needed for production if HTML is very sparse
                        if "<html" in content.lower() and "<body" in content.lower():
                            editor.setHtml(content)
                            editor.rich_mode = True
                            self.tabs.setTabText(self.tabs.currentIndex(), f"Restored (HTML): {temp_file_name}")
                        else:
                            editor.setPlainText(content)
                            editor.rich_mode = False
                            self.tabs.setTabText(self.tabs.currentIndex(), f"Restored: {temp_file_name}")
                        
                        editor.document().setModified(True) # Treat as modified until saved
                    except Exception as e:
                        print(f"Error loading auto-saved file {temp_file_name}: {e}")
                # Clean up auto-saved directory after restoration (after all files are potentially loaded)
                # It's better to clean up individual files as they are successfully saved in save_file
                # For remaining files (e.g., if user chose not to restore all), they'll persist.

    def closeEvent(self, event):
        """Overrides close event to prompt user to save modified files."""
        # Iterate over all tabs to check for unsaved changes
        for i in range(self.tabs.count()):
            editor = self.tabs.widget(i)
            if editor.document().isModified():
                self.tabs.setCurrentIndex(i) # Bring the unsaved tab to front
                reply = QMessageBox.question(
                    self,
                    "Unsaved Changes",
                    f"The document '{self.tabs.tabText(i).rstrip('*')}' has unsaved changes. Do you want to save it before closing?",
                    QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel
                )
                if reply == QMessageBox.StandardButton.Save:
                    # Pass the specific editor to be saved
                    if not self.save_file(editor=editor):
                        event.ignore()
                        return
                elif reply == QMessageBox.StandardButton.Cancel:
                    event.ignore()
                    return
        # If all tabs are handled or no unsaved changes, accept close event
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)

    # Set application name and icon (optional but good practice)
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(APP_VERSION)
    app.setDesktopFileName(APP_ID)
    app.setWindowIcon(get_icon("efnote"))

    # Set global font for the application
    # Added font fallback for "Inter"
    try:
        app.setFont(QFont("Inter", 10))
    except Exception:
        app.setFont(QFont("Noto Sans", 10)) # Fallback font if Inter not found

    window = MainWindow()
    window.show()

    for argument in sys.argv[1:]:
        candidate = pathlib.Path(argument).expanduser()
        if candidate.is_file():
            window.open_file_path(str(candidate.resolve()))

    sys.exit(app.exec())
