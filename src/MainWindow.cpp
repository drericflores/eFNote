#include "MainWindow.hpp"

#include "DocumentEditor.hpp"
#include "SearchReplaceDialog.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QCryptographicHash>
#include <QFontDatabase>
#include <QIcon>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QSaveFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QStringConverter>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QUuid>
#include <QVBoxLayout>
#include <QtPrintSupport/QPrinter>

namespace {
constexpr auto kMarkdownFilter =
    "All supported files (*.md *.markdown *.txt *.html *.htm *.cpp *.hpp *.c *.h *.py *.json);;"
    "Markdown (*.md *.markdown);;Text (*.txt);;HTML (*.html *.htm);;"
    "Source code (*.cpp *.hpp *.c *.h *.py);;All files (*)";

QIcon icon(const char* name) {
    return QIcon(QStringLiteral(":/icons/") + QString::fromLatin1(name) + QStringLiteral(".svg"));
}

struct DecodedText {
    QString text;
    QString encoding{QStringLiteral("UTF-8")};
    QString lineEnding{QStringLiteral("\n")};
    bool byteOrderMark{false};
};

DecodedText decodeText(const QByteArray& data) {
    DecodedText result;
    QByteArray payload = data;
    if (payload.startsWith(QByteArray::fromHex("EFBBBF"))) {
        result.byteOrderMark = true;
        payload.remove(0, 3);
    } else if (payload.startsWith(QByteArray::fromHex("FFFE"))) {
        result.encoding = QStringLiteral("UTF-16LE");
        result.byteOrderMark = true;
        payload.remove(0, 2);
    } else if (payload.startsWith(QByteArray::fromHex("FEFF"))) {
        result.encoding = QStringLiteral("UTF-16BE");
        result.byteOrderMark = true;
        payload.remove(0, 2);
    }

    if (result.encoding == QStringLiteral("UTF-16LE")) {
        QStringDecoder decoder(QStringConverter::Utf16LE);
        result.text = decoder(payload);
    } else if (result.encoding == QStringLiteral("UTF-16BE")) {
        QStringDecoder decoder(QStringConverter::Utf16BE);
        result.text = decoder(payload);
    } else {
        QStringDecoder decoder(QStringConverter::Utf8);
        result.text = decoder(payload);
        if (decoder.hasError()) {
            result.encoding = QStringLiteral("ISO-8859-1");
            result.byteOrderMark = false;
            result.text = QString::fromLatin1(payload);
        }
    }
    if (result.text.contains(QStringLiteral("\r\n")))
        result.lineEnding = QStringLiteral("\r\n");
    else if (result.text.contains(QLatin1Char('\r')))
        result.lineEnding = QStringLiteral("\r");
    result.text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    result.text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return result;
}

QByteArray encodeText(QString text, const DocumentEditor* editor) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (editor->lineEnding() != QStringLiteral("\n"))
        text.replace(QStringLiteral("\n"), editor->lineEnding());

    if (editor->textEncoding() == QStringLiteral("UTF-16LE")) {
        QStringEncoder encoder(QStringConverter::Utf16LE);
        QByteArray output = encoder(text);
        if (editor->hasByteOrderMark()) output.prepend(QByteArray::fromHex("FFFE"));
        return output;
    }
    if (editor->textEncoding() == QStringLiteral("UTF-16BE")) {
        QStringEncoder encoder(QStringConverter::Utf16BE);
        QByteArray output = encoder(text);
        if (editor->hasByteOrderMark()) output.prepend(QByteArray::fromHex("FEFF"));
        return output;
    }
    if (editor->textEncoding() == QStringLiteral("ISO-8859-1"))
        return text.toLatin1();
    QByteArray output = text.toUtf8();
    if (editor->hasByteOrderMark()) output.prepend(QByteArray::fromHex("EFBBBF"));
    return output;
}
}

MainWindow::MainWindow(const QStringList& initialFiles, QWidget* parent)
    : QMainWindow(parent), tabs_(new QTabWidget(this)),
      fileWatcher_(new QFileSystemWatcher(this)) {
    setWindowTitle(tr("eFNote[*]"));
    resize(1100, 720);

    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    tabs_->setDocumentMode(true);
    setCentralWidget(tabs_);

    createActions();
    createMenus();
    createToolBar();
    createPreview();
    restoreSettings();
    toggleDarkMode(darkModeAction_->isChecked());

    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closeDocument);
    connect(fileWatcher_, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::handleExternalFileChange);
    connect(tabs_, &QTabWidget::currentChanged, this, [this] {
        updateCurrentUi();
        updatePreview();
        if (searchDialog_) searchDialog_->setEditor(currentEditor());
    });

    const int restoredCount = restoreRecoveries();
    bool openedAny = false;
    for (const auto& path : initialFiles) {
        openedAny = openPath(path) || openedAny;
    }
    if (initialFiles.isEmpty() && restoredCount == 0) {
        openedAny = restoreSession() > 0;
    }
    if (!openedAny && restoredCount == 0) {
        newDocument();
    }

    autoSaveTimer_ = new QTimer(this);
    autoSaveTimer_->setInterval(60'000);
    connect(autoSaveTimer_, &QTimer::timeout, this, &MainWindow::autoSaveAll);
    autoSaveTimer_->start();
    updateCurrentUi();
}

void MainWindow::createActions() {
    auto* newAction = new QAction(icon("new"), tr("&New"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);
    addAction(newAction);

    auto* openAction = new QAction(icon("open"), tr("&Open…"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openDialog);
    addAction(openAction);

    saveAction_ = new QAction(icon("save"), tr("&Save"), this);
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveDocument(currentEditor()); });
    addAction(saveAction_);

    saveAsAction_ = new QAction(icon("save_as"), tr("Save &As…"), this);
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, [this] { saveDocumentAs(currentEditor()); });
    addAction(saveAsAction_);

    auto* exportPdfAction = new QAction(tr("Export as &PDF…"), this);
    connect(exportPdfAction, &QAction::triggered, this,
            [this] { exportDocument(QStringLiteral("pdf")); });
    addAction(exportPdfAction);
    auto* exportDocxAction = new QAction(tr("Export as &Word (.docx)…"), this);
    connect(exportDocxAction, &QAction::triggered, this,
            [this] { exportDocument(QStringLiteral("docx")); });
    addAction(exportDocxAction);
    auto* exportOdtAction = new QAction(tr("Export as &OpenDocument (.odt)…"), this);
    connect(exportOdtAction, &QAction::triggered, this,
            [this] { exportDocument(QStringLiteral("odt")); });
    addAction(exportOdtAction);

    closeAction_ = new QAction(tr("&Close"), this);
    closeAction_->setShortcut(QKeySequence::Close);
    connect(closeAction_, &QAction::triggered, this, &MainWindow::closeCurrentDocument);
    addAction(closeAction_);

    auto* exitAction = new QAction(icon("exit"), tr("E&xit"), this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    addAction(exitAction);

    auto* undoAction = new QAction(icon("undo"), tr("&Undo"), this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this] {
        if (auto* editor = currentEditor()) editor->undo();
    });
    addAction(undoAction);

    auto* redoAction = new QAction(icon("redo"), tr("&Redo"), this);
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this] {
        if (auto* editor = currentEditor()) editor->redo();
    });
    addAction(redoAction);

    auto* cutAction = new QAction(icon("cut"), tr("Cu&t"), this);
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this] {
        if (auto* editor = currentEditor()) editor->cut();
    });
    addAction(cutAction);

    auto* copyAction = new QAction(icon("copy"), tr("&Copy"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this] {
        if (auto* editor = currentEditor()) editor->copy();
    });
    addAction(copyAction);

    auto* pasteAction = new QAction(icon("paste"), tr("&Paste"), this);
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this] {
        if (auto* editor = currentEditor()) editor->paste();
    });
    addAction(pasteAction);

    auto* findAction = new QAction(icon("search"), tr("Find and &Replace…"), this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::showSearchReplace);
    addAction(findAction);

    auto* goToLineAction = new QAction(tr("&Go to Line…"), this);
    goToLineAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(goToLineAction, &QAction::triggered, this, &MainWindow::goToLine);
    addAction(goToLineAction);

    auto* duplicateLineAction = new QAction(tr("&Duplicate Line"), this);
    duplicateLineAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    connect(duplicateLineAction, &QAction::triggered, this, &MainWindow::duplicateCurrentLine);
    addAction(duplicateLineAction);

    auto* moveLineUpAction = new QAction(tr("Move Line &Up"), this);
    moveLineUpAction->setShortcut(QKeySequence(QStringLiteral("Alt+Up")));
    connect(moveLineUpAction, &QAction::triggered, this, [this] { moveCurrentLine(-1); });
    addAction(moveLineUpAction);

    auto* moveLineDownAction = new QAction(tr("Move Line &Down"), this);
    moveLineDownAction->setShortcut(QKeySequence(QStringLiteral("Alt+Down")));
    connect(moveLineDownAction, &QAction::triggered, this, [this] { moveCurrentLine(1); });
    addAction(moveLineDownAction);

    auto* upperCaseAction = new QAction(tr("Convert to &Uppercase"), this);
    connect(upperCaseAction, &QAction::triggered, this, [this] { convertSelectionCase(true); });
    addAction(upperCaseAction);

    auto* lowerCaseAction = new QAction(tr("Convert to &Lowercase"), this);
    connect(lowerCaseAction, &QAction::triggered, this, [this] { convertSelectionCase(false); });
    addAction(lowerCaseAction);

    auto markdownAction = [this](const QString& text, const QString& shortcut,
                                 const QString& prefix, const QString& suffix) {
        auto* action = new QAction(text, this);
        if (!shortcut.isEmpty()) action->setShortcut(QKeySequence(shortcut));
        connect(action, &QAction::triggered, this,
                [this, prefix, suffix] { formatMarkdown(prefix, suffix); });
        addAction(action);
    };
    markdownAction(tr("Markdown &Bold"), QStringLiteral("Ctrl+B"),
                   QStringLiteral("**"), QStringLiteral("**"));
    markdownAction(tr("Markdown &Italic"), QStringLiteral("Ctrl+I"),
                   QStringLiteral("*"), QStringLiteral("*"));
    markdownAction(tr("Markdown &Code"), QStringLiteral("Ctrl+Shift+C"),
                   QStringLiteral("`"), QStringLiteral("`"));
    markdownAction(tr("Markdown &Link"), QStringLiteral("Ctrl+K"),
                   QStringLiteral("["), QStringLiteral("](https://)"));

    auto* headingAction = new QAction(tr("Markdown &Heading"), this);
    connect(headingAction, &QAction::triggered, this,
            [this] { prefixMarkdownLines(QStringLiteral("## ")); });
    addAction(headingAction);

    auto* quoteAction = new QAction(tr("Markdown &Quote"), this);
    connect(quoteAction, &QAction::triggered, this,
            [this] { prefixMarkdownLines(QStringLiteral("> ")); });
    addAction(quoteAction);

    auto* listAction = new QAction(tr("Markdown &List"), this);
    connect(listAction, &QAction::triggered, this,
            [this] { prefixMarkdownLines(QStringLiteral("- ")); });
    addAction(listAction);

    previewAction_ = new QAction(tr("Markdown &Preview"), this);
    previewAction_->setCheckable(true);
    previewAction_->setChecked(true);
    previewAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    connect(previewAction_, &QAction::toggled, this, &MainWindow::updatePreview);
    addAction(previewAction_);

    darkModeAction_ = new QAction(tr("Dark &Mode"), this);
    darkModeAction_->setCheckable(true);
    darkModeAction_->setShortcut(QKeySequence(Qt::Key_F10));
    connect(darkModeAction_, &QAction::toggled, this, &MainWindow::toggleDarkMode);
    addAction(darkModeAction_);

    wordWrapAction_ = new QAction(tr("&Word Wrap"), this);
    wordWrapAction_->setCheckable(true);
    connect(wordWrapAction_, &QAction::toggled, this, [this](bool enabled) {
        for (int i = 0; i < tabs_->count(); ++i)
            if (auto* editor = qobject_cast<DocumentEditor*>(tabs_->widget(i)))
                editor->setWordWrapEnabled(enabled);
    });
    addAction(wordWrapAction_);

    whitespaceAction_ = new QAction(tr("Show &Whitespace"), this);
    whitespaceAction_->setCheckable(true);
    connect(whitespaceAction_, &QAction::toggled, this, [this](bool enabled) {
        for (int i = 0; i < tabs_->count(); ++i)
            if (auto* editor = qobject_cast<DocumentEditor*>(tabs_->widget(i)))
                editor->setWhitespaceVisible(enabled);
    });
    addAction(whitespaceAction_);

    auto* viewModes = new QActionGroup(this);
    viewModes->setExclusive(true);
    editorOnlyAction_ = new QAction(tr("Editor Only"), viewModes);
    splitViewAction_ = new QAction(tr("Split Editor and Preview"), viewModes);
    previewOnlyAction_ = new QAction(tr("Preview Only"), viewModes);
    for (auto* action : {editorOnlyAction_, splitViewAction_, previewOnlyAction_}) {
        action->setCheckable(true);
    }
    splitViewAction_->setChecked(true);
    connect(editorOnlyAction_, &QAction::triggered, this,
            [this] { setViewMode(ViewMode::EditorOnly); });
    connect(splitViewAction_, &QAction::triggered, this,
            [this] { setViewMode(ViewMode::Split); });
    connect(previewOnlyAction_, &QAction::triggered, this,
            [this] { setViewMode(ViewMode::PreviewOnly); });
}

void MainWindow::createMenus() {
    const auto actions = this->actions();
    auto findByText = [&actions](const QString& text) {
        for (auto* action : actions) if (action->text() == text) return action;
        return static_cast<QAction*>(nullptr);
    };

    auto* file = menuBar()->addMenu(tr("&File"));
    file->addAction(findByText(tr("&New")));
    file->addAction(findByText(tr("&Open…")));
    recentFilesMenu_ = file->addMenu(tr("Open &Recent"));
    updateRecentFilesMenu();
    file->addAction(closeAction_);
    file->addSeparator();
    file->addAction(saveAction_);
    file->addAction(saveAsAction_);
    auto* exportMenu = file->addMenu(tr("&Export"));
    exportMenu->addAction(findByText(tr("Export as &PDF…")));
    exportMenu->addAction(findByText(tr("Export as &Word (.docx)…")));
    exportMenu->addAction(findByText(tr("Export as &OpenDocument (.odt)…")));
    file->addSeparator();
    file->addAction(findByText(tr("E&xit")));

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(findByText(tr("&Undo")));
    edit->addAction(findByText(tr("&Redo")));
    edit->addSeparator();
    edit->addAction(findByText(tr("Cu&t")));
    edit->addAction(findByText(tr("&Copy")));
    edit->addAction(findByText(tr("&Paste")));
    edit->addSeparator();
    edit->addAction(findByText(tr("Find and &Replace…")));
    edit->addAction(findByText(tr("&Go to Line…")));
    edit->addSeparator();
    edit->addAction(findByText(tr("&Duplicate Line")));
    edit->addAction(findByText(tr("Move Line &Up")));
    edit->addAction(findByText(tr("Move Line &Down")));
    auto* caseMenu = edit->addMenu(tr("Convert &Case"));
    caseMenu->addAction(findByText(tr("Convert to &Uppercase")));
    caseMenu->addAction(findByText(tr("Convert to &Lowercase")));

    auto* markdown = menuBar()->addMenu(tr("&Markdown"));
    markdown->addAction(findByText(tr("Markdown &Bold")));
    markdown->addAction(findByText(tr("Markdown &Italic")));
    markdown->addAction(findByText(tr("Markdown &Code")));
    markdown->addAction(findByText(tr("Markdown &Link")));
    markdown->addSeparator();
    markdown->addAction(findByText(tr("Markdown &Heading")));
    markdown->addAction(findByText(tr("Markdown &Quote")));
    markdown->addAction(findByText(tr("Markdown &List")));

    auto* view = menuBar()->addMenu(tr("&View"));
    view->addAction(previewAction_);
    auto* modeMenu = view->addMenu(tr("Preview &Mode"));
    modeMenu->addAction(editorOnlyAction_);
    modeMenu->addAction(splitViewAction_);
    modeMenu->addAction(previewOnlyAction_);
    view->addAction(darkModeAction_);
    view->addSeparator();
    view->addAction(wordWrapAction_);
    view->addAction(whitespaceAction_);

    auto* help = menuBar()->addMenu(tr("&Help"));
    auto* howToUse = help->addAction(tr("&How to Use eFNote"));
    connect(howToUse, &QAction::triggered, this, &MainWindow::showHowToUse);
    auto* cheatSheet = help->addAction(tr("&Markdown Cheat Sheet"));
    connect(cheatSheet, &QAction::triggered, this, &MainWindow::showMarkdownCheatSheet);
    help->addSeparator();
    auto* about = help->addAction(tr("&About eFNote"));
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::createToolBar() {
    auto* bar = addToolBar(tr("Main Toolbar"));
    bar->setObjectName(QStringLiteral("MainToolbar"));
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    const QStringList toolbarItems = {
        tr("&New"), tr("&Open…"), tr("&Save"), tr("&Undo"), tr("&Redo"),
        tr("Cu&t"), tr("&Copy"), tr("&Paste"), tr("Find and &Replace…")
    };
    for (auto* action : actions()) {
        if (toolbarItems.contains(action->text())) {
            bar->addAction(action);
        }
    }
}

void MainWindow::createPreview() {
    preview_ = new QTextBrowser(this);
    preview_->setOpenExternalLinks(true);
    preview_->document()->setDefaultStyleSheet(QStringLiteral(
        "body{font-family:sans-serif;font-size:11pt;line-height:1.5}"
        "h1{font-size:22pt}h2{font-size:18pt;border-bottom:2px solid #d0d7de;padding-bottom:6px}"
        "blockquote{color:#57606a;border-left:4px solid #d0d7de;padding-left:12px}"
        "code,pre{font-family:monospace;background:#eaeef2}"
        "pre{padding:10px;white-space:pre-wrap}table{border-collapse:collapse}"
        "th,td{border:1px solid #d0d7de;padding:6px}"));

    previewDock_ = new QDockWidget(tr("Markdown Preview"), this);
    previewDock_->setObjectName(QStringLiteral("MarkdownPreviewDock"));
    previewDock_->setWidget(preview_);
    addDockWidget(Qt::RightDockWidgetArea, previewDock_);
    previewDock_->hide();
}

DocumentEditor* MainWindow::addDocument(const QString& path) {
    auto* editor = new DocumentEditor(this);
    editor->setWordWrapEnabled(wordWrapAction_->isChecked());
    editor->setWhitespaceVisible(whitespaceAction_->isChecked());
    editor->setFilePath(path);
    editor->setProperty("recoveryId", QUuid::createUuid().toString(QUuid::WithoutBraces));
    const int index = tabs_->addTab(editor, editor->displayName());
    tabs_->setCurrentIndex(index);

    connect(editor->document(), &QTextDocument::modificationChanged, this,
            [this, editor] { updateTabTitle(editor); updateCurrentUi(); });
    connect(editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateCurrentUi);
    connect(editor, &QTextEdit::textChanged, this, [this, editor] {
        if (editor == currentEditor()) updatePreview();
    });
    return editor;
}

DocumentEditor* MainWindow::currentEditor() const {
    return qobject_cast<DocumentEditor*>(tabs_->currentWidget());
}

void MainWindow::newDocument() {
    addDocument();
}

void MainWindow::openDialog() {
    const auto paths = QFileDialog::getOpenFileNames(this, tr("Open Document"), {}, tr(kMarkdownFilter));
    for (const auto& path : paths) openPath(path);
}

bool MainWindow::openPath(const QString& path) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    for (int index = 0; index < tabs_->count(); ++index) {
        auto* existing = qobject_cast<DocumentEditor*>(tabs_->widget(index));
        if (existing && existing->filePath() == absolutePath) {
            tabs_->setCurrentIndex(index);
            return true;
        }
    }
    const QFileInfo info(absolutePath);
    constexpr qint64 largeFileThreshold = 10LL * 1024LL * 1024LL;
    if (info.size() > largeFileThreshold) {
        const auto answer = QMessageBox::warning(
            this, tr("Large File"),
            tr("'%1' is %2 MB. Large files may reduce editor performance. Open it?")
                .arg(info.fileName())
                .arg(info.size() / (1024.0 * 1024.0), 0, 'f', 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return false;
    }

    auto* editor = addDocument(absolutePath);
    editor->setProperty("largeFile", info.size() > largeFileThreshold);
    if (!loadFile(editor, absolutePath)) {
        const int index = tabs_->indexOf(editor);
        tabs_->removeTab(index);
        editor->deleteLater();
        return false;
    }
    removeRecovery(editor);
    updateTabTitle(editor);
    updatePreview();
    addRecentFile(editor->filePath());
    return true;
}

bool MainWindow::loadFile(DocumentEditor* editor, const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not open '%1':\n%2").arg(path, file.errorString()));
        return false;
    }
    const DecodedText decoded = decodeText(file.readAll());
    editor->setFilePath(QFileInfo(path).absoluteFilePath());
    editor->setProperty("largeFile", QFileInfo(path).size() > 10LL * 1024LL * 1024LL);
    editor->setTextEncoding(decoded.encoding, decoded.byteOrderMark);
    editor->setLineEnding(decoded.lineEnding);
    if (editor->isHtml()) editor->setHtml(decoded.text);
    else editor->setPlainText(decoded.text);
    editor->document()->setModified(false);
    watchFile(editor->filePath());
    return true;
}

void MainWindow::watchFile(const QString& path) {
    if (!path.isEmpty() && QFileInfo::exists(path) && !fileWatcher_->files().contains(path))
        fileWatcher_->addPath(path);
}

void MainWindow::handleExternalFileChange(const QString& path) {
    if (savingPaths_.contains(path)) return;
    DocumentEditor* editor = nullptr;
    for (int index = 0; index < tabs_->count(); ++index) {
        auto* candidate = qobject_cast<DocumentEditor*>(tabs_->widget(index));
        if (candidate && candidate->filePath() == path) {
            editor = candidate;
            break;
        }
    }
    if (!editor) return;
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("File Removed"),
                             tr("'%1' was removed or renamed outside eFNote.")
                                 .arg(QFileInfo(path).fileName()));
        return;
    }
    const QString detail = editor->document()->isModified()
        ? tr("The document also has unsaved changes in eFNote.")
        : tr("Reload the updated file from disk?");
    const auto answer = QMessageBox::question(
        this, tr("File Changed Outside eFNote"),
        tr("'%1' changed on disk.\n\n%2").arg(QFileInfo(path).fileName(), detail),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        loadFile(editor, path);
        updateTabTitle(editor);
        updatePreview();
    } else {
        watchFile(path);
    }
}

bool MainWindow::saveDocument(DocumentEditor* editor) {
    if (!editor) return false;
    if (editor->filePath().isEmpty()) return saveDocumentAs(editor);

    const QString path = editor->filePath();
    savingPaths_.insert(path);
    fileWatcher_->removePath(path);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Failed"), file.errorString());
        savingPaths_.remove(path);
        watchFile(path);
        return false;
    }
    const QString source = editor->isHtml()
        ? editor->document()->toHtml() : editor->toPlainText();
    const QByteArray data = encodeText(source, editor);
    if (file.write(data) != data.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Save Failed"), file.errorString());
        savingPaths_.remove(path);
        watchFile(path);
        return false;
    }
    savingPaths_.remove(path);
    watchFile(path);
    editor->document()->setModified(false);
    removeRecovery(editor);
    statusBar()->showMessage(tr("Saved %1").arg(editor->displayName()), 3000);
    return true;
}

bool MainWindow::saveDocumentAs(DocumentEditor* editor) {
    if (!editor) return false;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Document As"),
                                                editor->filePath(), tr(kMarkdownFilter));
    if (path.isEmpty()) return false;
    if (!editor->filePath().isEmpty()) fileWatcher_->removePath(editor->filePath());
    editor->setFilePath(QFileInfo(path).absoluteFilePath());
    addRecentFile(editor->filePath());
    updateTabTitle(editor);
    updatePreview();
    return saveDocument(editor);
}

bool MainWindow::maybeSave(DocumentEditor* editor) {
    if (!editor || !editor->document()->isModified()) return true;
    const auto answer = QMessageBox::warning(
        this, tr("Unsaved Changes"),
        tr("Save changes to '%1'?").arg(editor->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveDocument(editor);
    removeRecovery(editor);
    return true;
}

void MainWindow::closeDocument(int index) {
    auto* editor = qobject_cast<DocumentEditor*>(tabs_->widget(index));
    if (!maybeSave(editor)) return;
    if (editor && !editor->filePath().isEmpty())
        fileWatcher_->removePath(editor->filePath());
    tabs_->removeTab(index);
    removeRecovery(editor);
    editor->deleteLater();
    if (tabs_->count() == 0) newDocument();
}

void MainWindow::closeCurrentDocument() {
    if (tabs_->currentIndex() >= 0) closeDocument(tabs_->currentIndex());
}

void MainWindow::showSearchReplace() {
    if (!searchDialog_) searchDialog_ = new SearchReplaceDialog(this);
    searchDialog_->setEditor(currentEditor());
    searchDialog_->show();
    searchDialog_->raise();
    searchDialog_->activateWindow();
}

void MainWindow::goToLine() {
    auto* editor = currentEditor();
    if (!editor) return;
    const int maximum = qMax(1, editor->document()->blockCount());
    bool accepted = false;
    const int line = QInputDialog::getInt(this, tr("Go to Line"),
                                          tr("Line number:"), 1, 1, maximum, 1, &accepted);
    if (!accepted) return;
    QTextCursor cursor(editor->document()->findBlockByNumber(line - 1));
    editor->setTextCursor(cursor);
    editor->setFocus();
    editor->ensureCursorVisible();
}

void MainWindow::formatMarkdown(const QString& prefix, const QString& suffix) {
    auto* editor = currentEditor();
    if (!editor) return;
    auto cursor = editor->textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText();
        cursor.insertText(prefix + selected + suffix);
    } else {
        cursor.insertText(prefix + suffix);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, suffix.size());
    }
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    editor->setFocus();
}

void MainWindow::prefixMarkdownLines(const QString& prefix) {
    auto* editor = currentEditor();
    if (!editor) return;
    auto cursor = editor->textCursor();
    int first = cursor.selectionStart();
    int last = cursor.selectionEnd();
    cursor.setPosition(first);
    first = cursor.block().position();
    cursor.setPosition(last);
    if (last > first && cursor.atBlockStart()) cursor.movePosition(QTextCursor::PreviousBlock);
    const int lastBlock = cursor.blockNumber();
    cursor.setPosition(first);
    cursor.beginEditBlock();
    for (;;) {
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.insertText(prefix);
        if (cursor.blockNumber() >= lastBlock) break;
        cursor.movePosition(QTextCursor::NextBlock);
    }
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    editor->setFocus();
}

void MainWindow::duplicateCurrentLine() {
    auto* editor = currentEditor();
    if (!editor) return;
    auto cursor = editor->textCursor();
    const int column = cursor.positionInBlock();
    cursor.select(QTextCursor::BlockUnderCursor);
    const QString line = cursor.selectedText();
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.beginEditBlock();
    cursor.insertBlock();
    cursor.insertText(line);
    cursor.endEditBlock();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        qMin(column, line.size()));
    editor->setTextCursor(cursor);
}

void MainWindow::moveCurrentLine(int direction) {
    auto* editor = currentEditor();
    if (!editor || (direction != -1 && direction != 1)) return;
    const auto original = editor->textCursor();
    const QTextBlock current = original.block();
    const QTextBlock adjacent = direction < 0 ? current.previous() : current.next();
    if (!adjacent.isValid()) return;
    const int column = original.positionInBlock();
    const int target = adjacent.blockNumber();
    const QString first = direction < 0 ? adjacent.text() : current.text();
    const QString second = direction < 0 ? current.text() : adjacent.text();
    const QTextBlock startBlock = direction < 0 ? adjacent : current;
    const QTextBlock endBlock = direction < 0 ? current : adjacent;
    QTextCursor edit(startBlock);
    edit.setPosition(startBlock.position());
    edit.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
    edit.beginEditBlock();
    edit.insertText(second + QLatin1Char('\n') + first);
    edit.endEditBlock();
    QTextCursor moved(editor->document()->findBlockByNumber(target));
    moved.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                       qMin(column, moved.block().length() - 1));
    editor->setTextCursor(moved);
}

void MainWindow::convertSelectionCase(bool upper) {
    auto* editor = currentEditor();
    if (!editor) return;
    auto cursor = editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    if (!cursor.hasSelection()) return;
    const QString replacement = upper ? cursor.selectedText().toUpper()
                                      : cursor.selectedText().toLower();
    cursor.insertText(replacement);
    editor->setTextCursor(cursor);
}

void MainWindow::addRecentFile(const QString& path) {
    if (path.isEmpty()) return;
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    recentFiles_.removeAll(absolutePath);
    recentFiles_.prepend(absolutePath);
    while (recentFiles_.size() > 10) recentFiles_.removeLast();
    QSettings settings;
    settings.setValue(QStringLiteral("files/recent"), recentFiles_);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu() {
    if (!recentFilesMenu_) return;
    recentFilesMenu_->clear();
    recentFiles_.removeIf([](const QString& path) { return !QFileInfo::exists(path); });
    if (recentFiles_.isEmpty()) {
        auto* empty = recentFilesMenu_->addAction(tr("(No Recent Files)"));
        empty->setEnabled(false);
        return;
    }
    for (int index = 0; index < recentFiles_.size(); ++index) {
        const QString path = recentFiles_.at(index);
        auto* action = recentFilesMenu_->addAction(
            tr("&%1 %2").arg(index + 1).arg(QFileInfo(path).fileName()));
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path] { openPath(path); });
    }
    recentFilesMenu_->addSeparator();
    auto* clear = recentFilesMenu_->addAction(tr("&Clear Recent Files"));
    connect(clear, &QAction::triggered, this, [this] {
        recentFiles_.clear();
        QSettings().remove(QStringLiteral("files/recent"));
        updateRecentFilesMenu();
    });
}

int MainWindow::restoreSession() {
    QSettings settings;
    const QStringList paths = settings.value(QStringLiteral("session/files")).toStringList();
    int restored = 0;
    for (const auto& path : paths) {
        if (QFileInfo::exists(path) && openPath(path)) ++restored;
    }
    const int requestedIndex = settings.value(QStringLiteral("session/current"), 0).toInt();
    if (tabs_->count() > 0)
        tabs_->setCurrentIndex(qBound(0, requestedIndex, tabs_->count() - 1));
    return restored;
}

void MainWindow::exportDocument(const QString& format) {
    auto* editor = currentEditor();
    if (!editor) return;
    const QString suffix = QStringLiteral(".") + format;
    const QString filter = format == QStringLiteral("pdf")
        ? tr("PDF Document (*.pdf)")
        : format == QStringLiteral("docx")
            ? tr("Microsoft Word Document (*.docx)")
            : tr("OpenDocument Text (*.odt)");
    QString suggested = editor->filePath().isEmpty()
        ? QStringLiteral("eFNote-Document") + suffix
        : QFileInfo(editor->filePath()).absolutePath() + QLatin1Char('/') +
          QFileInfo(editor->filePath()).completeBaseName() + suffix;
    QString outputPath = QFileDialog::getSaveFileName(
        this, tr("Export Document"), suggested, filter);
    if (outputPath.isEmpty()) return;
    if (!outputPath.endsWith(suffix, Qt::CaseInsensitive)) outputPath += suffix;
    if (format == QStringLiteral("pdf")) exportPdf(editor, outputPath);
    else exportWithPandoc(editor, outputPath, format);
}

void MainWindow::exportPdf(DocumentEditor* editor, const QString& outputPath) {
    QTextDocument document;
    if (editor->isMarkdown()) document.setMarkdown(editor->toPlainText());
    else if (editor->isHtml()) document.setHtml(editor->document()->toHtml());
    else document.setPlainText(editor->toPlainText());

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPath);
    printer.setDocName(QFileInfo(outputPath).completeBaseName());
    document.print(&printer);
    if (QFileInfo::exists(outputPath) && QFileInfo(outputPath).size() > 0) {
        statusBar()->showMessage(tr("Exported %1").arg(QFileInfo(outputPath).fileName()), 5000);
    } else {
        QMessageBox::critical(this, tr("PDF Export Failed"),
                              tr("eFNote could not create the PDF file."));
    }
}

void MainWindow::exportWithPandoc(DocumentEditor* editor, const QString& outputPath,
                                  const QString& format) {
    const QString pandoc = QStandardPaths::findExecutable(QStringLiteral("pandoc"));
    if (pandoc.isEmpty()) {
        QMessageBox::critical(this, tr("Pandoc Required"),
            tr("DOCX and ODT export require Pandoc. Install it with:\n\n"
               "sudo apt install pandoc"));
        return;
    }
    QString inputFormat = QStringLiteral("markdown");
    QByteArray source = editor->toPlainText().toUtf8();
    if (editor->isMarkdown()) inputFormat = QStringLiteral("gfm");
    if (editor->isHtml()) {
        inputFormat = QStringLiteral("html");
        source = editor->document()->toHtml().toUtf8();
    }

    QProcess process;
    const QString title = editor->filePath().isEmpty()
        ? tr("eFNote Document") : QFileInfo(editor->filePath()).completeBaseName();
    process.start(pandoc, {QStringLiteral("--from"), inputFormat,
                           QStringLiteral("--to"), format,
                           QStringLiteral("--standalone"),
                           QStringLiteral("--metadata"), QStringLiteral("title=") + title,
                           QStringLiteral("--output"), outputPath}, QIODevice::ReadWrite);
    if (!process.waitForStarted(5000)) {
        QMessageBox::critical(this, tr("Export Failed"), process.errorString());
        return;
    }
    process.write(source);
    process.closeWriteChannel();
    if (!process.waitForFinished(60'000) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        QMessageBox::critical(this, tr("Export Failed"),
            QString::fromUtf8(process.readAllStandardError()).trimmed());
        return;
    }
    statusBar()->showMessage(tr("Exported %1").arg(QFileInfo(outputPath).fileName()), 5000);
}

QString MainWindow::recoveryPath(DocumentEditor* editor) const {
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/recovery");
    QDir().mkpath(directory);
    return directory + QLatin1Char('/') + editor->property("recoveryId").toString()
        + QStringLiteral(".json");
}

void MainWindow::autoSaveAll() {
    int saved = 0;
    for (int index = 0; index < tabs_->count(); ++index) {
        auto* editor = qobject_cast<DocumentEditor*>(tabs_->widget(index));
        if (!editor || !editor->document()->isModified()) continue;
        QJsonObject recovery {
            {QStringLiteral("filePath"), editor->filePath()},
            {QStringLiteral("html"), editor->isHtml()},
            {QStringLiteral("encoding"), editor->textEncoding()},
            {QStringLiteral("byteOrderMark"), editor->hasByteOrderMark()},
            {QStringLiteral("lineEnding"), editor->lineEnding()},
            {QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
            {QStringLiteral("content"), editor->isHtml()
                ? editor->document()->toHtml() : editor->toPlainText()}
        };
        QSaveFile file(recoveryPath(editor));
        if (file.open(QIODevice::WriteOnly) &&
            file.write(QJsonDocument(recovery).toJson(QJsonDocument::Compact)) >= 0 &&
            file.commit()) {
            ++saved;
        }
    }
    if (saved > 0) statusBar()->showMessage(tr("Recovery snapshot saved"), 2000);
}

int MainWindow::restoreRecoveries() {
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/recovery");
    QDir recoveryDirectory(directory);
    const auto files = recoveryDirectory.entryList({QStringLiteral("*.json")}, QDir::Files);
    if (files.isEmpty()) return 0;
    const auto answer = QMessageBox::question(
        this, tr("Restore Documents"),
        tr("eFNote found %1 recovery snapshot(s).\n\n"
           "Yes: restore them\nNo: discard them\nCancel: keep them for next startup")
            .arg(files.size()),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
    if (answer == QMessageBox::Cancel) return 0;
    if (answer == QMessageBox::No) {
        for (const auto& name : files) QFile::remove(recoveryDirectory.filePath(name));
        return 0;
    }

    int restored = 0;
    for (const auto& name : files) {
        QFile file(recoveryDirectory.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        const auto object = document.object();
        if (parseError.error != QJsonParseError::NoError ||
            !object.contains(QStringLiteral("content"))) {
            file.close();
            QFile::remove(recoveryDirectory.filePath(name));
            continue;
        }
        auto* editor = addDocument(object.value(QStringLiteral("filePath")).toString());
        editor->setTextEncoding(
            object.value(QStringLiteral("encoding")).toString(QStringLiteral("UTF-8")),
            object.value(QStringLiteral("byteOrderMark")).toBool(false));
        editor->setLineEnding(
            object.value(QStringLiteral("lineEnding")).toString(QStringLiteral("\n")));
        watchFile(editor->filePath());
        if (object.value(QStringLiteral("html")).toBool())
            editor->setHtml(object.value(QStringLiteral("content")).toString());
        else
            editor->setPlainText(object.value(QStringLiteral("content")).toString());
        editor->document()->setModified(true);
        updateTabTitle(editor);
        file.close();
        QFile::remove(recoveryDirectory.filePath(name));
        ++restored;
    }
    return restored;
}

void MainWindow::removeRecovery(DocumentEditor* editor) {
    if (editor) QFile::remove(recoveryPath(editor));
}

void MainWindow::setViewMode(ViewMode mode) {
    viewMode_ = mode;
    editorOnlyAction_->setChecked(mode == ViewMode::EditorOnly);
    splitViewAction_->setChecked(mode == ViewMode::Split);
    previewOnlyAction_->setChecked(mode == ViewMode::PreviewOnly);
    applyViewMode();
}

void MainWindow::applyViewMode() {
    auto* editor = currentEditor();
    const bool markdown = editor && editor->isMarkdown();
    if (!markdown) {
        tabs_->show();
        previewDock_->hide();
        return;
    }
    tabs_->setVisible(viewMode_ != ViewMode::PreviewOnly);
    previewDock_->setVisible(previewAction_->isChecked() && viewMode_ != ViewMode::EditorOnly);
}

void MainWindow::toggleDarkMode(bool enabled) {
    if (!enabled) {
        qApp->setPalette(qApp->style()->standardPalette());
        setStyleSheet(QStringLiteral(
            "QMainWindow{background:#f5f6f8}"
            "QMenuBar,QToolBar,QStatusBar{background:#ffffff}"
            "QToolBar{border:0;border-bottom:1px solid #d8dee8;padding:5px;spacing:3px}"
            "QToolButton{border:1px solid transparent;border-radius:6px;padding:5px 8px}"
            "QToolButton:hover{background:#edf3fb;border-color:#c8d8eb}"
            "QTabWidget::pane{border:1px solid #d8dee8;background:#ffffff}"
            "QTabBar::tab{background:#e8ebf0;border:1px solid #d4d9e1;"
            "border-bottom:0;border-radius:7px 7px 0 0;padding:7px 13px;margin-right:2px}"
            "QTabBar::tab:selected{background:#ffffff;color:#1f4f82}"
            "QTextEdit,QTextBrowser{background:#ffffff;color:#20242b;border:0;padding:8px}"
            "QDockWidget::title{background:#edf1f6;padding:6px;font-weight:600}"));
        preview_->document()->setDefaultStyleSheet(QStringLiteral(
            "body{font-family:sans-serif;font-size:11pt;color:#20242b;line-height:1.55}"
            "h1{font-size:24pt;color:#173f68}h2{font-size:18pt;color:#245a8d;"
            "border-bottom:2px solid #d0d7de;padding-bottom:6px}"
            "h3{font-size:14pt;color:#2c679d}a{color:#0969da}"
            "blockquote{color:#57606a;border-left:4px solid #7da8cf;padding-left:12px}"
            "code,pre{font-family:monospace;background:#edf1f5}pre{padding:10px}"
            "table{border-collapse:collapse}th,td{border:1px solid #cbd3dd;padding:7px}"));
        updatePreview();
        return;
    }
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#282c34")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#abb2bf")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#21252b")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#abb2bf")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#3e4452")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#abb2bf")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#61afef")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#282c34")));
    qApp->setPalette(palette);
    setStyleSheet(QStringLiteral(
        "QMainWindow{background:#282c34}"
        "QMenuBar,QToolBar,QStatusBar{background:#21252b;color:#d7dae0}"
        "QMenu{background:#21252b;color:#d7dae0;border:1px solid #454b57}"
        "QMenu::item:selected{background:#3e4452}"
        "QToolBar{border:0;border-bottom:1px solid #454b57;padding:5px;spacing:3px}"
        "QToolButton{color:#d7dae0;border:1px solid transparent;border-radius:6px;padding:5px 8px}"
        "QToolButton:hover{background:#3e4452;border-color:#5b6372}"
        "QTabWidget::pane{border:1px solid #454b57;background:#21252b}"
        "QTabBar::tab{background:#353b46;color:#c8ccd4;border:1px solid #454b57;"
        "border-bottom:0;border-radius:7px 7px 0 0;padding:7px 13px;margin-right:2px}"
        "QTabBar::tab:selected{background:#21252b;color:#73b7f2}"
        "QTextEdit,QTextBrowser{background:#21252b;color:#d7dae0;border:0;padding:8px}"
        "QDockWidget{color:#d7dae0}QDockWidget::title{background:#353b46;padding:6px;font-weight:600}"));
    preview_->document()->setDefaultStyleSheet(QStringLiteral(
        "body{font-family:sans-serif;font-size:11pt;color:#d7dae0;line-height:1.55}"
        "h1{font-size:24pt;color:#7fc1ff}h2{font-size:18pt;color:#73b7f2;"
        "border-bottom:2px solid #555d6a;padding-bottom:6px}"
        "h3{font-size:14pt;color:#8ec7f5}a{color:#61afef}"
        "blockquote{color:#b5bac4;border-left:4px solid #61afef;padding-left:12px}"
        "code,pre{font-family:monospace;background:#303640;color:#d7dae0}pre{padding:10px}"
        "table{border-collapse:collapse}th,td{border:1px solid #555d6a;padding:7px}"));
    updatePreview();
}

void MainWindow::updateTabTitle(DocumentEditor* editor) {
    const int index = tabs_->indexOf(editor);
    if (index < 0) return;
    tabs_->setTabText(index, editor->displayName() +
        (editor->document()->isModified() ? QStringLiteral("*") : QString{}));
    tabs_->setTabToolTip(index, editor->filePath());
}

void MainWindow::updateCurrentUi() {
    auto* editor = currentEditor();
    const bool available = editor != nullptr;
    saveAction_->setEnabled(available);
    saveAsAction_->setEnabled(available);
    closeAction_->setEnabled(available);
    if (!editor) return;

    setWindowTitle(tr("eFNote — %1[*]").arg(editor->displayName()));
    setWindowModified(editor->document()->isModified());
    const auto cursor = editor->textCursor();
    const QString text = editor->toPlainText();
    int words = 0;
    auto matches = QRegularExpression(QStringLiteral("\\S+")).globalMatch(text);
    while (matches.hasNext()) { matches.next(); ++words; }
    const QString ending = editor->lineEnding() == QStringLiteral("\r\n")
        ? QStringLiteral("CRLF")
        : editor->lineEnding() == QStringLiteral("\r")
            ? QStringLiteral("CR") : QStringLiteral("LF");
    statusBar()->showMessage(tr("%1 | %2 | %3 | %4 | Ln %5, Col %6 | %7 lines | %8 words | %9 characters")
        .arg(editor->displayName())
        .arg(editor->languageName())
        .arg(editor->textEncoding())
        .arg(ending)
        .arg(cursor.blockNumber() + 1)
        .arg(cursor.positionInBlock() + 1)
        .arg(editor->document()->blockCount())
        .arg(words)
        .arg(text.size()));
}

void MainWindow::updatePreview() {
    auto* editor = currentEditor();
    if (editor && editor->isMarkdown()) {
        if (editor->property("largeFile").toBool())
            preview_->setPlainText(tr("Markdown preview is disabled for this large file."));
        else
            preview_->setMarkdown(editor->toPlainText());
    }
    applyViewMode();
}

void MainWindow::showHelpDialog(const QString& title, const QString& html) {
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(850, 700);
    auto* browser = new QTextBrowser(&dialog);
    browser->setOpenExternalLinks(true);
    browser->setHtml(html);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(browser);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showHowToUse() {
    showHelpDialog(tr("How to Use eFNote"), QStringLiteral(
        "<h1>How to Use eFNote 4</h1>"
        "<p>eFNote is a native C++/Qt6 text and Markdown editor. Each document"
        " opens in its own movable tab.</p>"
        "<h2>File operations</h2><ul>"
        "<li><b>New (Ctrl+N):</b> create an empty document.</li>"
        "<li><b>Open (Ctrl+O):</b> open one or more UTF-8 files.</li>"
        "<li><b>Save (Ctrl+S):</b> atomically save the active document.</li>"
        "<li><b>Save As (Ctrl+Shift+S):</b> save under a new name.</li>"
        "<li><b>Close (Ctrl+W):</b> close the active tab with unsaved-change protection.</li>"
        "</ul><h2>Editing</h2><ul>"
        "<li>Use standard Undo, Redo, Cut, Copy, and Paste shortcuts.</li>"
        "<li><b>Find (Ctrl+F)</b> searches from the cursor and wraps to the beginning.</li>"
        "<li>The status bar shows filename, line, column, and character count.</li>"
        "</ul><h2>Markdown</h2><ul>"
        "<li>Open a <code>.md</code> or <code>.markdown</code> file to activate preview.</li>"
        "<li><b>Ctrl+Shift+M</b> shows or hides the rendered preview.</li>"
        "<li>The preview updates automatically while you type.</li>"
        "<li>Use <b>Help → Markdown Cheat Sheet</b> for syntax examples.</li>"
        "</ul><h2>Appearance</h2><p>Press <b>F10</b> to toggle dark mode. Window size,"
        " dock arrangement, theme, and preview preference are remembered.</p>"
        "<h2>Command line</h2><pre>./build/efnote README.md</pre>"));
}

void MainWindow::showMarkdownCheatSheet() {
    showHelpDialog(tr("eFNote Markdown Cheat Sheet"), QStringLiteral(
        "<h1>Markdown Cheat Sheet</h1>"
        "<p>The left column describes the feature, the center shows what to type,"
        " and the right shows the published result.</p>"
        "<table border='1' cellspacing='0' cellpadding='8'>"
        "<tr><th>Purpose</th><th>Markdown source</th><th>Result</th></tr>"
        "<tr><td>Heading 1</td><td><code># Heading</code></td><td><h1>Heading</h1></td></tr>"
        "<tr><td>Heading 2</td><td><code>## Heading</code></td><td><h2>Heading</h2></td></tr>"
        "<tr><td>Bold</td><td><code>**bold**</code></td><td><b>bold</b></td></tr>"
        "<tr><td>Italic</td><td><code>*italic*</code></td><td><i>italic</i></td></tr>"
        "<tr><td>Bold italic</td><td><code>***important***</code></td><td><b><i>important</i></b></td></tr>"
        "<tr><td>Strikethrough</td><td><code>~~removed~~</code></td><td><s>removed</s></td></tr>"
        "<tr><td>List</td><td><code>- First<br>- Second</code></td><td>• First<br>• Second</td></tr>"
        "<tr><td>Numbered list</td><td><code>1. First<br>2. Second</code></td><td>1. First<br>2. Second</td></tr>"
        "<tr><td>Task list</td><td><code>- [x] Done<br>- [ ] Pending</code></td><td>☑ Done<br>☐ Pending</td></tr>"
        "<tr><td>Link</td><td><code>[OpenAI](https://openai.com)</code></td><td>OpenAI</td></tr>"
        "<tr><td>Image</td><td><code>![Description](image.png)</code></td><td>Embedded image</td></tr>"
        "<tr><td>Quote</td><td><code>&gt; Quoted text</code></td><td><blockquote>Quoted text</blockquote></td></tr>"
        "<tr><td>Inline code</td><td><code>`command`</code></td><td><code>command</code></td></tr>"
        "<tr><td>Horizontal rule</td><td><code>---</code></td><td><hr></td></tr>"
        "</table><h2>Code block</h2><pre>```cpp\nint main() {}\n```</pre>"
        "<h2>Table</h2><pre>| Instrument | Status |\n|---|---|\n| Fluke 287 | Ready |</pre>"
        "<h2>Escaping Markdown</h2><p>Use a backslash before a formatting character:"
        " <code>\\*not italic\\*</code>.</p>"
        "<p><b>Ctrl+Shift+M</b> toggles the live preview.</p>"));
}

void MainWindow::showAbout() {
    QMessageBox::about(this, tr("About eFNote"),
        tr("<h2>eFNote %1</h2><p>Native C++/Qt6 Edition</p>"
           "<p>Programmed by Dr. Eric O. Flores</p><p>August 2026</p>")
            .arg(QStringLiteral(EFNOTE_VERSION)));
}

void MainWindow::restoreSettings() {
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    previewAction_->setChecked(settings.value(QStringLiteral("view/preview"), true).toBool());
    darkModeAction_->setChecked(settings.value(QStringLiteral("view/darkMode"), false).toBool());
    wordWrapAction_->setChecked(settings.value(QStringLiteral("editor/wordWrap"), false).toBool());
    whitespaceAction_->setChecked(settings.value(QStringLiteral("editor/whitespace"), false).toBool());
    recentFiles_ = settings.value(QStringLiteral("files/recent")).toStringList();
    updateRecentFilesMenu();
    setViewMode(static_cast<ViewMode>(settings.value(QStringLiteral("view/mode"),
                                                     static_cast<int>(ViewMode::Split)).toInt()));
}

void MainWindow::saveSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("view/preview"), previewAction_->isChecked());
    settings.setValue(QStringLiteral("view/darkMode"), darkModeAction_->isChecked());
    settings.setValue(QStringLiteral("editor/wordWrap"), wordWrapAction_->isChecked());
    settings.setValue(QStringLiteral("editor/whitespace"), whitespaceAction_->isChecked());
    settings.setValue(QStringLiteral("view/mode"), static_cast<int>(viewMode_));
    QStringList sessionFiles;
    for (int index = 0; index < tabs_->count(); ++index) {
        if (auto* editor = qobject_cast<DocumentEditor*>(tabs_->widget(index));
            editor && !editor->filePath().isEmpty()) {
            sessionFiles.append(editor->filePath());
        }
    }
    settings.setValue(QStringLiteral("session/files"), sessionFiles);
    settings.setValue(QStringLiteral("session/current"), tabs_->currentIndex());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    for (int index = 0; index < tabs_->count(); ++index) {
        auto* editor = qobject_cast<DocumentEditor*>(tabs_->widget(index));
        tabs_->setCurrentIndex(index);
        if (!maybeSave(editor)) {
            event->ignore();
            return;
        }
    }
    saveSettings();
    event->accept();
}
