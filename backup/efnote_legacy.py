import sys
import os
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QTextEdit, QFileDialog, QTabWidget,
    QWidget, QVBoxLayout, QToolBar, QStatusBar, QMessageBox, QMenu,
    QInputDialog
)
from PySide6.QtGui import (
    QAction, QIcon, QTextCharFormat, QColor, QPalette,
    QSyntaxHighlighter, QTextCursor, QFont, QTextDocument
)
from PySide6.QtCore import Qt, QTimer, QSize

# Define a simple syntax highlighter for Python as an example
class PythonHighlighter(QSyntaxHighlighter):
    def __init__(self, document):
        super().__init__(document)

        # Keyword format
        keyword_format = QTextCharFormat()
        keyword_format.setForeground(QColor("#0000FF")) # Blue
        keyword_format.setFontWeight(QFont.Bold)
        keywords = [
            "False", "None", "True", "and", "as", "assert", "async", "await",
            "break", "class", "continue", "def", "del", "elif", "else",
            "except", "finally", "for", "from", "global", "if", "import",
            "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
            "return", "try", "while", "with", "yield"
        ]
        self.highlighting_rules = [(r"\b%s\b" % w, keyword_format) for w in keywords]

        # String format
        string_format = QTextCharFormat()
        string_format.setForeground(QColor("#A31515")) # Red
        self.highlighting_rules.append((r'".*?"|\'.*?\'', string_format))

        # Comment format
        comment_format = QTextCharFormat()
        comment_format.setForeground(QColor("#008000")) # Green
        self.highlighting_rules.append((r"#.*", comment_format))

        # Function format
        function_format = QTextCharFormat()
        function_format.setForeground(QColor("#800080")) # Purple
        self.highlighting_rules.append((r"\b[A-Za-z0-9_]+(?=\()", function_format))

    def highlightBlock(self, text):
        for pattern, format in self.highlighting_rules:
            import re
            for match in re.finditer(pattern, text):
                start = match.start()
                length = match.end() - start
                self.setFormat(start, length, format)
        self.setCurrentBlockState(0)

class TextEditor(QTextEdit):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFont(QFont("Inter", 12)) # Use Inter font
        self.document().setModified(False) # Set initial modified state to false
        self.highlighter = None # Will be set based on file type
        self.current_file_path = None
        self.is_dark_mode = False

        # Apply rounded corners via QSS
        self.setStyleSheet("QTextEdit { border-radius: 8px; padding: 5px; }")

    def set_syntax_highlighter(self, file_path):
        """Sets the appropriate syntax highlighter based on file extension."""
        if self.highlighter:
            self.highlighter.setDocument(None) # Clear existing highlighter

        _, ext = os.path.splitext(file_path)
        if ext.lower() == ".py":
            self.highlighter = PythonHighlighter(self.document())
        else:
            self.highlighter = None # No specific highlighter for other types

        # Corrected: Only rehighlight if a highlighter is actually set
        if self.highlighter:
            self.highlighter.rehighlight()

    def set_dark_mode(self, enabled):
        """Applies dark mode styling to the text editor."""
        self.is_dark_mode = enabled
        palette = self.palette()
        if enabled:
            palette.setColor(QPalette.Base, QColor("#282c34")) # Dracula background
            palette.setColor(QPalette.Text, QColor("#abb2bf")) # Dracula foreground
        else:
            palette.setColor(QPalette.Base, Qt.white)
            palette.setColor(QPalette.Text, Qt.black)
        self.setPalette(palette)
        # Re-apply syntax highlighting to ensure colors update
        if self.highlighter:
            self.highlighter.rehighlight()

class NoteTabWidget(QTabWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTabsClosable(True)
        self.tabCloseRequested.connect(self.close_tab)
        self.setMovable(True) # Allow tabs to be reordered
        self.setUsesScrollButtons(True) # Show scroll buttons if many tabs
        self.setStyleSheet("QTabWidget::pane { border: 0; } QTabBar::tab { border-radius: 5px 5px 0 0; padding: 5px; margin: 2px;} QTabBar::tab:selected { background: #e0e0e0; }") # Rounded tabs

    def add_new_tab(self, file_path=None, content=""):
        """Adds a new text editor tab."""
        editor = TextEditor()
        editor.setText(content)
        editor.current_file_path = file_path
        if file_path:
            editor.set_syntax_highlighter(file_path)
            tab_name = os.path.basename(file_path)
        else:
            tab_name = "Untitled"
        index = self.addTab(editor, tab_name)
        self.setCurrentIndex(index)
        editor.document().contentsChanged.connect(lambda: self.set_tab_modified(index, True))
        return editor

    def set_tab_modified(self, index, modified):
        """Adds/removes '*' to/from tab title to indicate modification."""
        if modified and not self.tabText(index).endswith("*"):
            self.setTabText(index, self.tabText(index) + "*")
        elif not modified and self.tabText(index).endswith("*"):
            self.setTabText(index, self.tabText(index).rstrip("*"))
        self.widget(index).document().setModified(modified)

    def close_tab(self, index):
        """Handles closing a tab, prompting to save if modified."""
        editor = self.widget(index)
        if editor.document().isModified():
            reply = QMessageBox.question(
                self,
                "Save Changes",
                f"Do you want to save changes to {self.tabText(index).rstrip('*')}?",
                QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel
            )
            if reply == QMessageBox.StandardButton.Save:
                if not self.parent().save_file(): # Parent (MainWindow) handles saving
                    return # Cancelled save, don't close tab
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
        self.setWindowTitle("eFNote")
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
        self.tabs.add_new_tab() # Start with an empty tab

        self.create_actions()
        self.create_menus()
        self.create_toolbar()
        self.create_status_bar()

    def create_actions(self):
        """Creates QActions for menu and toolbar."""
        # File Actions
        self.new_action = QAction(QIcon(":/icons/new.svg"), "&New", self)
        self.new_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_N)
        self.new_action.setStatusTip("Create a new note")
        self.new_action.triggered.connect(self.new_file)

        self.open_action = QAction(QIcon(":/icons/open.svg"), "&Open...", self)
        self.open_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_O)
        self.open_action.setStatusTip("Open an existing note")
        self.open_action.triggered.connect(self.open_file)

        self.save_action = QAction(QIcon(":/icons/save.svg"), "&Save", self)
        self.save_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_S)
        self.save_action.setStatusTip("Save the current note")
        self.save_action.triggered.connect(self.save_file)

        self.save_as_action = QAction(QIcon(":/icons/save_as.svg"), "Save &As...", self)
        self.save_as_action.setShortcut(Qt.Modifier.CTRL | Qt.Modifier.SHIFT | Qt.Key_S)
        self.save_as_action.setStatusTip("Save the current note with a new name")
        self.save_as_action.triggered.connect(self.save_file_as)

        self.exit_action = QAction(QIcon(":/icons/exit.svg"), "E&xit", self)
        self.exit_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_Q)
        self.exit_action.setStatusTip("Exit the application")
        self.exit_action.triggered.connect(self.close)

        # Edit Actions
        self.undo_action = QAction(QIcon(":/icons/undo.svg"), "&Undo", self)
        self.undo_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_Z)
        self.undo_action.setStatusTip("Undo the last action")
        self.undo_action.triggered.connect(lambda: self.tabs.current_editor().undo())

        self.redo_action = QAction(QIcon(":/icons/redo.svg"), "&Redo", self)
        self.redo_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_Y)
        self.redo_action.setStatusTip("Redo the last action")
        self.redo_action.triggered.connect(lambda: self.tabs.current_editor().redo())

        self.cut_action = QAction(QIcon(":/icons/cut.svg"), "Cu&t", self)
        self.cut_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_X)
        self.cut_action.setStatusTip("Cut selected text")
        self.cut_action.triggered.connect(lambda: self.tabs.current_editor().cut())

        self.copy_action = QAction(QIcon(":/icons/copy.svg"), "&Copy", self)
        self.copy_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_C)
        self.copy_action.setStatusTip("Copy selected text")
        self.copy_action.triggered.connect(lambda: self.tabs.current_editor().copy())

        self.paste_action = QAction(QIcon(":/icons/paste.svg"), "&Paste", self)
        self.paste_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_V)
        self.paste_action.setStatusTip("Paste text from clipboard")
        self.paste_action.triggered.connect(lambda: self.tabs.current_editor().paste())

        self.select_all_action = QAction("Select &All", self)
        self.select_all_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_A)
        self.select_all_action.setStatusTip("Select all text")
        self.select_all_action.triggered.connect(lambda: self.tabs.current_editor().selectAll())

        self.search_action = QAction(QIcon(":/icons/search.svg"), "&Search/Replace", self)
        self.search_action.setShortcut(Qt.Modifier.CTRL | Qt.Key_F)
        self.search_action.setStatusTip("Find or replace text")
        self.search_action.triggered.connect(self.show_search_dialog)

        # View Actions
        self.dark_mode_action = QAction("Dark &Mode", self, checkable=True)
        self.dark_mode_action.setShortcut(Qt.Key_F10)
        self.dark_mode_action.setStatusTip("Toggle dark mode")
        self.dark_mode_action.triggered.connect(self.toggle_dark_mode)

    def create_menus(self):
        """Creates the application's menu bar."""
        menu_bar = self.menuBar()

        file_menu = menu_bar.addMenu("&File")
        file_menu.addAction(self.new_action)
        file_menu.addAction(self.open_action)
        file_menu.addSeparator()
        file_menu.addAction(self.save_action)
        file_menu.addAction(self.save_as_action)
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

        view_menu = menu_bar.addMenu("&View")
        view_menu.addAction(self.dark_mode_action)

    def create_toolbar(self):
        """Creates the application's toolbar."""
        toolbar = QToolBar("Main Toolbar")
        toolbar.setIconSize(QSize(24, 24)) # Set icon size
        self.addToolBar(toolbar)

        toolbar.addAction(self.new_action)
        toolbar.addAction(self.open_action)
        toolbar.addAction(self.save_action)
        toolbar.addSeparator()
        toolbar.addAction(self.undo_action)
        toolbar.addAction(self.redo_action)
        toolbar.addSeparator()
        toolbar.addAction(self.search_action)

        # Style the toolbar
        toolbar.setStyleSheet("""
            QToolBar {
                background: #f0f0f0;
                border-bottom: 1px solid #d0d0d0;
                spacing: 5px;
            }
            QToolButton {
                border: 1px solid transparent;
                border-radius: 5px;
                padding: 5px;
                margin: 2px;
            }
            QToolButton:hover {
                background: #e0e0e0;
                border: 1px solid #c0c0c0;
            }
            QToolButton:pressed {
                background: #d0d0d0;
                border: 1px solid #b0b0b0;
            }
        """)

    def create_status_bar(self):
        """Creates the application's status bar."""
        self.status_bar = self.statusBar()
        self.status_bar.showMessage("Ready")
        # Update status bar when tab changes
        self.tabs.currentChanged.connect(self.update_status_bar)
        # Update status bar when text changes (to show modified state)
        if self.tabs.current_editor():
            self.tabs.current_editor().document().contentsChanged.connect(self.update_status_bar)

    def update_status_bar(self):
        """Updates the status bar with current file info and modified status."""
        editor = self.tabs.current_editor()
        if editor:
            file_path = editor.current_file_path
            status_text = os.path.basename(file_path) if file_path else "Untitled"
            if editor.document().isModified():
                status_text += " (Modified)"
            cursor = editor.textCursor()
            line = cursor.blockNumber() + 1
            col = cursor.columnNumber() + 1
            status_text += f" | Line: {line}, Col: {col}"
            self.status_bar.showMessage(status_text)
        else:
            self.status_bar.showMessage("Ready")


    def new_file(self):
        """Creates a new, empty tab."""
        self.tabs.add_new_tab()
        self.update_status_bar()

    def open_file(self):
        """Opens a file and loads its content into a new tab."""
        file_path, _ = QFileDialog.getOpenFileName(self, "Open Note", "", "All Files (*);;Text Files (*.txt);;Python Files (*.py);;Markdown Files (*.md)")
        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                editor = self.tabs.add_new_tab(file_path, content)
                editor.document().setModified(False) # File is not modified right after opening
                self.update_status_bar()
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Could not open file: {e}")

    def save_file(self):
        """Saves the content of the current tab to its associated file path."""
        editor = self.tabs.current_editor()
        if not editor:
            return False # No editor active

        if not editor.current_file_path:
            return self.save_file_as() # If no path, prompt for Save As
        else:
            try:
                with open(editor.current_file_path, 'w', encoding='utf-8') as f:
                    f.write(editor.toPlainText())
                editor.document().setModified(False)
                self.tabs.set_tab_modified(self.tabs.currentIndex(), False)
                self.status_bar.showMessage(f"Saved {os.path.basename(editor.current_file_path)}")
                self.update_status_bar()
                return True
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Could not save file: {e}")
                return False

    def save_file_as(self):
        """Saves the content of the current tab to a new file path."""
        editor = self.tabs.current_editor()
        if not editor:
            return False # No editor active

        file_path, _ = QFileDialog.getSaveFileName(self, "Save Note As", "", "All Files (*);;Text Files (*.txt);;Python Files (*.py);;Markdown Files (*.md)")
        if file_path:
            editor.current_file_path = file_path
            editor.set_syntax_highlighter(file_path) # Update highlighter for new extension
            self.tabs.setTabText(self.tabs.currentIndex(), os.path.basename(file_path)) # Update tab name
            return self.save_file() # Now save to the new path
        return False

    def toggle_dark_mode(self, checked):
        """Toggles between light and dark themes."""
        self.dark_mode_enabled = checked
        self.apply_theme()

    def apply_theme(self):
        """Applies the selected theme to the application."""
        app = QApplication.instance()
        palette = QPalette()

        if self.dark_mode_enabled:
            # Dark mode palette (Dracula-like)
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
        else:
            # Light mode palette (default Qt)
            app.setPalette(QApplication.instance().style().standardPalette()) # Reset to default
            self.setStyleSheet("") # Clear custom stylesheet
            self.tabs.setStyleSheet("QTabWidget::pane { border: 0; } QTabBar::tab { border-radius: 5px 5px 0 0; padding: 5px; margin: 2px;} QTabBar::tab:selected { background: #e0e0e0; }")

        app.setPalette(palette)

        # Apply dark mode to all existing TextEditor instances
        for i in range(self.tabs.count()):
            editor = self.tabs.widget(i)
            if isinstance(editor, TextEditor):
                editor.set_dark_mode(self.dark_mode_enabled)

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

    def auto_save_all_tabs(self):
        """Auto-saves all modified tabs to a temporary directory."""
        auto_save_dir = os.path.join(os.path.expanduser("~"), ".efnote_autosave")
        os.makedirs(auto_save_dir, exist_ok=True)

        for i in range(self.tabs.count()):
            editor = self.tabs.widget(i)
            if isinstance(editor, TextEditor) and editor.document().isModified():
                # Use a hash of content or a unique ID for temp file names
                # For simplicity, using tab index here, but a more robust ID is better
                temp_file_name = f"tab_{i}_{os.urandom(4).hex()}.tmp"
                temp_file_path = os.path.join(auto_save_dir, temp_file_name)
                try:
                    with open(temp_file_path, 'w', encoding='utf-8') as f:
                        f.write(editor.toPlainText())
                    self.status_bar.showMessage(f"Auto-saved tab {i+1}", 2000) # Message for 2 seconds
                except Exception as e:
                    print(f"Error auto-saving tab {i}: {e}") # Print error to console

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
                        # Add as a new untitled tab, indicating it's restored
                        editor = self.tabs.add_new_tab(content=content)
                        editor.document().setModified(True) # Treat as modified until saved
                        self.tabs.setTabText(self.tabs.currentIndex(), f"Restored: {temp_file_name}")
                    except Exception as e:
                        print(f"Error loading auto-saved file {temp_file_name}: {e}")
                # Clean up auto-saved directory after restoration
                for f_name in os.listdir(auto_save_dir):
                    os.remove(os.path.join(auto_save_dir, f_name))


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
                    if not self.save_file(): # If save is cancelled, prevent closing
                        event.ignore()
                        return
                elif reply == QMessageBox.StandardButton.Cancel:
                    event.ignore()
                    return
        # If all tabs are handled or no unsaved changes, accept close event
        event.accept()

# Create dummy icon files (for demonstration, in a real app these would be proper SVG/PNGs)
# In a real PySide app, you'd use Qt resource files (.qrc) for icons.
# For this MVP, we simulate it.
def create_dummy_icons():
    icons_dir = "icons"
    os.makedirs(icons_dir, exist_ok=True)
    icon_names = ["new", "open", "save", "save_as", "exit", "undo", "redo", "cut", "copy", "paste", "search"]
    for name in icon_names:
        # Create a simple SVG icon placeholder
        svg_content = f"""
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
        <rect x="2" y="2" width="20" height="20" rx="4" stroke="currentColor" stroke-width="2"/>
        <text x="12" y="16" font-family="Arial" font-size="12" text-anchor="middle" fill="currentColor">{name[0].upper()}</text>
        </svg>
        """
        with open(os.path.join(icons_dir, f"{name}.svg"), "w") as f:
            f.write(svg_content)
    # Register the icon path
    QIcon.setThemeSearchPaths(QIcon.themeSearchPaths() + [icons_dir])
    QIcon.setThemeName("efnote_icons") # A dummy theme name to make QIcon work


if __name__ == "__main__":
    create_dummy_icons() # Create dummy icons for the MVP

    app = QApplication(sys.argv)

    # Set application name and icon (optional but good practice)
    app.setApplicationName("eFNote")
    app.setWindowIcon(QIcon(":/icons/new.svg")) # Using a dummy icon as app icon

    # Set global font for the application
    app.setFont(QFont("Inter", 10))

    window = MainWindow()
    window.show()

    sys.exit(app.exec())

