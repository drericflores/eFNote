#pragma once

#include <QMainWindow>
#include <QSet>
#include <QStringList>

class QAction;
class QCloseEvent;
class QDockWidget;
class QMenu;
class QFileSystemWatcher;
class QTabWidget;
class QTextBrowser;
class QTimer;
class DocumentEditor;
class SearchReplaceDialog;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QStringList& initialFiles = {}, QWidget* parent = nullptr);

    bool openPath(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class ViewMode { EditorOnly = 0, Split = 1, PreviewOnly = 2 };

    void createActions();
    void createMenus();
    void createToolBar();
    void createPreview();
    void restoreSettings();
    void saveSettings();
    int restoreSession();
    void addRecentFile(const QString& path);
    void updateRecentFilesMenu();
    bool loadFile(DocumentEditor* editor, const QString& path);
    void watchFile(const QString& path);
    void handleExternalFileChange(const QString& path);

    DocumentEditor* addDocument(const QString& path = {});
    DocumentEditor* currentEditor() const;
    void newDocument();
    void openDialog();
    bool saveDocument(DocumentEditor* editor);
    bool saveDocumentAs(DocumentEditor* editor);
    bool maybeSave(DocumentEditor* editor);
    void closeDocument(int index);
    void closeCurrentDocument();
    void showSearchReplace();
    void goToLine();
    void formatMarkdown(const QString& prefix, const QString& suffix);
    void prefixMarkdownLines(const QString& prefix);
    void duplicateCurrentLine();
    void moveCurrentLine(int direction);
    void convertSelectionCase(bool upper);
    void exportDocument(const QString& format);
    void exportPdf(DocumentEditor* editor, const QString& outputPath);
    void exportWithPandoc(DocumentEditor* editor, const QString& outputPath,
                          const QString& format);
    void autoSaveAll();
    int restoreRecoveries();
    QString recoveryPath(DocumentEditor* editor) const;
    void removeRecovery(DocumentEditor* editor);
    void setViewMode(ViewMode mode);
    void applyViewMode();
    void toggleDarkMode(bool enabled);
    void updateCurrentUi();
    void updateTabTitle(DocumentEditor* editor);
    void updatePreview();
    void showHowToUse();
    void showMarkdownCheatSheet();
    void showHelpDialog(const QString& title, const QString& html);
    void showAbout();

    QTabWidget* tabs_{};
    QDockWidget* previewDock_{};
    QTextBrowser* preview_{};
    QMenu* recentFilesMenu_{};
    QFileSystemWatcher* fileWatcher_{};

    QAction* saveAction_{};
    QAction* saveAsAction_{};
    QAction* closeAction_{};
    QAction* previewAction_{};
    QAction* darkModeAction_{};
    QAction* wordWrapAction_{};
    QAction* whitespaceAction_{};
    QAction* editorOnlyAction_{};
    QAction* splitViewAction_{};
    QAction* previewOnlyAction_{};
    SearchReplaceDialog* searchDialog_{};
    QTimer* autoSaveTimer_{};
    ViewMode viewMode_{ViewMode::Split};
    QStringList recentFiles_;
    QSet<QString> savingPaths_;
};
