#pragma once

#include <QTextEdit>

class SyntaxHighlighter;

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

private:
    QString filePath_;
    SyntaxHighlighter* highlighter_{};
};
