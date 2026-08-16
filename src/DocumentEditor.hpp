#pragma once

#include <QTextEdit>

class DocumentEditor final : public QTextEdit {
    Q_OBJECT

public:
    explicit DocumentEditor(QWidget* parent = nullptr);

    [[nodiscard]] QString filePath() const;
    void setFilePath(const QString& path);
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool isMarkdown() const;
    [[nodiscard]] bool isHtml() const;

private:
    QString filePath_;
};
