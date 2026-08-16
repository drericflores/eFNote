#include "DocumentEditor.hpp"

#include "SyntaxHighlighter.hpp"

#include <QAbstractTextDocumentLayout>
#include <QKeyEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextOption>

class LineNumberArea final : public QWidget {
public:
    explicit LineNumberArea(DocumentEditor* editor) : QWidget(editor), editor_(editor) {}
    QSize sizeHint() const override { return {editor_->lineNumberAreaWidth(), 0}; }
protected:
    void paintEvent(QPaintEvent* event) override { editor_->paintLineNumberArea(event); }
private:
    DocumentEditor* editor_;
};

DocumentEditor::DocumentEditor(QWidget* parent)
    : QTextEdit(parent),
      highlighter_(new SyntaxHighlighter(document())),
      lineNumberArea_(new LineNumberArea(this)) {
    setAcceptRichText(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    setLineWrapMode(QTextEdit::NoWrap);
    connect(document(), &QTextDocument::blockCountChanged,
            this, &DocumentEditor::updateLineNumberArea);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &DocumentEditor::updateLineNumberArea);
    connect(this, &QTextEdit::cursorPositionChanged,
            this, &DocumentEditor::updateEditorSelections);
    updateLineNumberArea();
    updateEditorSelections();
    document()->setModified(false);
}

QString DocumentEditor::filePath() const {
    return filePath_;
}

void DocumentEditor::setFilePath(const QString& path) {
    filePath_ = path;
    highlighter_->setFilePath(path);
}

QString DocumentEditor::displayName() const {
    return filePath_.isEmpty() ? tr("Untitled") : QFileInfo(filePath_).fileName();
}

bool DocumentEditor::isMarkdown() const {
    const auto suffix = QFileInfo(filePath_).suffix().toLower();
    return suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown");
}

bool DocumentEditor::isHtml() const {
    const auto suffix = QFileInfo(filePath_).suffix().toLower();
    return suffix == QStringLiteral("html") || suffix == QStringLiteral("htm");
}

QString DocumentEditor::languageName() const {
    return highlighter_->languageName();
}

void DocumentEditor::setTextEncoding(const QString& name, bool byteOrderMark) {
    textEncoding_ = name;
    byteOrderMark_ = byteOrderMark;
}

QString DocumentEditor::textEncoding() const {
    return textEncoding_;
}

bool DocumentEditor::hasByteOrderMark() const {
    return byteOrderMark_;
}

void DocumentEditor::setLineEnding(const QString& ending) {
    lineEnding_ = ending;
}

QString DocumentEditor::lineEnding() const {
    return lineEnding_;
}

void DocumentEditor::setWordWrapEnabled(bool enabled) {
    setLineWrapMode(enabled ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    updateLineNumberArea();
}

void DocumentEditor::setWhitespaceVisible(bool visible) {
    whitespaceVisible_ = visible;
    auto option = document()->defaultTextOption();
    auto flags = option.flags();
    flags.setFlag(QTextOption::ShowTabsAndSpaces, visible);
    flags.setFlag(QTextOption::ShowLineAndParagraphSeparators, visible);
    option.setFlags(flags);
    document()->setDefaultTextOption(option);
    viewport()->update();
}

bool DocumentEditor::wordWrapEnabled() const {
    return lineWrapMode() != QTextEdit::NoWrap;
}

bool DocumentEditor::whitespaceVisible() const {
    return whitespaceVisible_;
}

int DocumentEditor::lineNumberAreaWidth() const {
    int digits = 1;
    for (int lines = qMax(1, document()->blockCount()); lines >= 10; lines /= 10) ++digits;
    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void DocumentEditor::updateLineNumberArea() {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    const QRect area = contentsRect();
    lineNumberArea_->setGeometry(area.left(), area.top(), lineNumberAreaWidth(), area.height());
    lineNumberArea_->update();
}

void DocumentEditor::resizeEvent(QResizeEvent* event) {
    QTextEdit::resizeEvent(event);
    updateLineNumberArea();
}

void DocumentEditor::scrollContentsBy(int dx, int dy) {
    QTextEdit::scrollContentsBy(dx, dy);
    lineNumberArea_->scroll(0, dy);
    lineNumberArea_->update();
}

void DocumentEditor::paintLineNumberArea(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), palette().alternateBase());
    painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));

    const qreal margin = document()->documentMargin();
    const int scroll = verticalScrollBar()->value();
    auto* layout = document()->documentLayout();
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const QRectF bounds = layout->blockBoundingRect(block);
        const int top = qRound(bounds.top() + margin) - scroll;
        const int height = qRound(bounds.height());
        if (top > event->rect().bottom()) break;
        if (top + height >= event->rect().top()) {
            painter.drawText(0, top, lineNumberArea_->width() - 6, height,
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(block.blockNumber() + 1));
        }
    }
}

int DocumentEditor::matchingBracketPosition(int position, QChar bracket) const {
    const QString text = toPlainText();
    const QString opens = QStringLiteral("([{");
    const QString closes = QStringLiteral(")]}");
    const bool forward = opens.contains(bracket);
    const int pairIndex = forward ? opens.indexOf(bracket) : closes.indexOf(bracket);
    if (pairIndex < 0) return -1;
    const QChar open = opens.at(pairIndex);
    const QChar close = closes.at(pairIndex);
    int depth = 0;
    for (int index = position; forward ? index < text.size() : index >= 0;
         index += forward ? 1 : -1) {
        if (text.at(index) == open) ++depth;
        if (text.at(index) == close) --depth;
        if (depth == 0) return index;
    }
    return -1;
}

void DocumentEditor::addBracketSelections(QList<QTextEdit::ExtraSelection>& selections) const {
    const QString text = toPlainText();
    const int cursorPosition = textCursor().position();
    int bracketPosition = -1;
    if (cursorPosition > 0 && QStringLiteral("()[]{}").contains(text.at(cursorPosition - 1)))
        bracketPosition = cursorPosition - 1;
    else if (cursorPosition < text.size() &&
             QStringLiteral("()[]{}").contains(text.at(cursorPosition)))
        bracketPosition = cursorPosition;
    if (bracketPosition < 0) return;

    const int match = matchingBracketPosition(bracketPosition, text.at(bracketPosition));
    const QColor color = match >= 0 ? QColor(QStringLiteral("#a5d6a7"))
                                    : QColor(QStringLiteral("#ef9a9a"));
    for (const int position : {bracketPosition, match}) {
        if (position < 0) continue;
        QTextEdit::ExtraSelection selection;
        selection.cursor = textCursor();
        selection.cursor.setPosition(position);
        selection.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        selection.format.setBackground(color);
        selection.format.setForeground(QColor(QStringLiteral("#202124")));
        selections.append(selection);
    }
}

void DocumentEditor::updateEditorSelections() {
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection line;
        line.cursor = textCursor();
        line.cursor.clearSelection();
        line.format.setBackground(palette().alternateBase());
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        selections.append(line);
    }
    addBracketSelections(selections);
    setExtraSelections(selections);
}

void DocumentEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
        QTextEdit::keyPressEvent(event);
        return;
    }

    const QString currentLine = textCursor().block().text();
    QString indentation;
    while (indentation.size() < currentLine.size() &&
           currentLine.at(indentation.size()).isSpace()) {
        indentation.append(currentLine.at(indentation.size()));
    }
    const QString trimmed = currentLine.trimmed();
    if (trimmed.endsWith(QLatin1Char('{')) || trimmed.endsWith(QLatin1Char('[')) ||
        trimmed.endsWith(QLatin1Char('(')) || trimmed.endsWith(QLatin1Char(':'))) {
        indentation += QStringLiteral("    ");
    }
    QTextEdit::keyPressEvent(event);
    auto cursor = textCursor();
    cursor.insertText(indentation);
    setTextCursor(cursor);
}
