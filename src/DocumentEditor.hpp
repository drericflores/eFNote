#pragma once

#include <QTextEdit>

class QKeyEvent;
class QPaintEvent;
class QResizeEvent;
class SyntaxHighlighter;
class LineNumberArea;

class DocumentEditor final : public QTextEdit {
    Q_OBJECT

public:
    explicit DocumentEditor(QWidget* parent = nullptr);

    [[nodiscard]] QString filePath() const;
    void setFilePath(const QString& path);
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool isMarkdown() const;
    [[nodiscard]] bool isHtml() const;
    [[nodiscard]] QString languageName() const;
    void setTextEncoding(const QString& name, bool byteOrderMark);
    [[nodiscard]] QString textEncoding() const;
    [[nodiscard]] bool hasByteOrderMark() const;
    void setLineEnding(const QString& ending);
    [[nodiscard]] QString lineEnding() const;
    void setWordWrapEnabled(bool enabled);
    void setWhitespaceVisible(bool visible);
    [[nodiscard]] bool wordWrapEnabled() const;
    [[nodiscard]] bool whitespaceVisible() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    friend class LineNumberArea;
    [[nodiscard]] int lineNumberAreaWidth() const;
    void updateLineNumberArea();
    void paintLineNumberArea(QPaintEvent* event);
    void updateEditorSelections();
    void addBracketSelections(QList<QTextEdit::ExtraSelection>& selections) const;
    [[nodiscard]] int matchingBracketPosition(int position, QChar bracket) const;

    QString filePath_;
    SyntaxHighlighter* highlighter_{};
    LineNumberArea* lineNumberArea_{};
    bool whitespaceVisible_{false};
    QString textEncoding_{QStringLiteral("UTF-8")};
    QString lineEnding_{QStringLiteral("\n")};
    bool byteOrderMark_{false};
};
