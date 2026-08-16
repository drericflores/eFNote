#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class SyntaxHighlighter final : public QSyntaxHighlighter {
    Q_OBJECT
public:
    enum class Language { PlainText, Markdown, Cpp, Python, Json, Html };
    explicit SyntaxHighlighter(QTextDocument* document);
    void setFilePath(const QString& path);
    [[nodiscard]] QString languageName() const;

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression expression;
        QTextCharFormat format;
    };
    static Language detectLanguage(const QString& path);
    void rebuildRules();
    void addRule(const QString& pattern, const QTextCharFormat& format);
    void highlightMultilineComments(const QString& text);

    Language language_{Language::PlainText};
    QVector<Rule> rules_;
    QRegularExpression commentStart_;
    QRegularExpression commentEnd_;
    QTextCharFormat commentFormat_;
};
