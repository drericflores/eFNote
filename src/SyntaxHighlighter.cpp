#include "SyntaxHighlighter.hpp"

#include <QFileInfo>
#include <QFont>
#include <QSet>
#include <QTextDocument>

namespace {
QTextCharFormat makeFormat(const char* color, QFont::Weight weight = QFont::Normal,
                           bool italic = false) {
    QTextCharFormat value;
    value.setForeground(QColor(QString::fromLatin1(color)));
    value.setFontWeight(weight);
    value.setFontItalic(italic);
    return value;
}
QString keywords(const QStringList& words) {
    return QStringLiteral("\\b(?:") + words.join(QLatin1Char('|')) + QStringLiteral(")\\b");
}
}

SyntaxHighlighter::SyntaxHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document) {
    rebuildRules();
}

void SyntaxHighlighter::setFilePath(const QString& path) {
    const auto detected = detectLanguage(path);
    if (detected == language_) return;
    language_ = detected;
    rebuildRules();
    rehighlight();
}

QString SyntaxHighlighter::languageName() const {
    switch (language_) {
    case Language::Markdown: return tr("Markdown");
    case Language::Cpp: return tr("C/C++");
    case Language::Python: return tr("Python");
    case Language::Json: return tr("JSON");
    case Language::Html: return tr("HTML");
    case Language::PlainText: return tr("Plain Text");
    }
    return tr("Plain Text");
}

SyntaxHighlighter::Language SyntaxHighlighter::detectLanguage(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown"))
        return Language::Markdown;
    if (QSet<QString>{QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cpp"),
                      QStringLiteral("cxx"), QStringLiteral("h"), QStringLiteral("hh"),
                      QStringLiteral("hpp"), QStringLiteral("hxx")}.contains(suffix))
        return Language::Cpp;
    if (suffix == QStringLiteral("py") || suffix == QStringLiteral("pyw"))
        return Language::Python;
    if (suffix == QStringLiteral("json")) return Language::Json;
    if (suffix == QStringLiteral("html") || suffix == QStringLiteral("htm"))
        return Language::Html;
    return Language::PlainText;
}

void SyntaxHighlighter::addRule(const QString& pattern, const QTextCharFormat& value) {
    rules_.append({QRegularExpression(pattern), value});
}

void SyntaxHighlighter::rebuildRules() {
    rules_.clear();
    commentStart_ = {};
    commentEnd_ = {};
    const auto keyword = makeFormat("#7c4dff", QFont::Bold);
    const auto type = makeFormat("#00796b", QFont::DemiBold);
    const auto string = makeFormat("#c62828");
    const auto number = makeFormat("#1565c0");
    const auto comment = makeFormat("#607d8b", QFont::Normal, true);
    const auto heading = makeFormat("#0d47a1", QFont::Bold);
    const auto emphasis = makeFormat("#8e24aa", QFont::DemiBold);
    const auto tag = makeFormat("#1565c0", QFont::DemiBold);
    commentFormat_ = comment;

    if (language_ == Language::Cpp) {
        addRule(keywords({QStringLiteral("auto"), QStringLiteral("bool"), QStringLiteral("break"),
            QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("char"),
            QStringLiteral("class"), QStringLiteral("const"), QStringLiteral("constexpr"),
            QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("delete"),
            QStringLiteral("do"), QStringLiteral("double"), QStringLiteral("else"),
            QStringLiteral("enum"), QStringLiteral("explicit"), QStringLiteral("false"),
            QStringLiteral("float"), QStringLiteral("for"), QStringLiteral("if"),
            QStringLiteral("inline"), QStringLiteral("int"), QStringLiteral("namespace"),
            QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("nullptr"),
            QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
            QStringLiteral("return"), QStringLiteral("static"), QStringLiteral("struct"),
            QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
            QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
            QStringLiteral("typename"), QStringLiteral("using"), QStringLiteral("virtual"),
            QStringLiteral("void"), QStringLiteral("while")}), keyword);
        addRule(QStringLiteral("\\b(?:QString|QStringList|QWidget|QObject|QTextDocument)\\b"), type);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""), string);
        addRule(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b"), number);
        addRule(QStringLiteral("//[^\\n]*"), comment);
        addRule(QStringLiteral("^\\s*#\\s*(?:include|define|if|ifdef|ifndef|endif|pragma).*"), emphasis);
        commentStart_ = QRegularExpression(QStringLiteral("/\\*"));
        commentEnd_ = QRegularExpression(QStringLiteral("\\*/"));
    } else if (language_ == Language::Python) {
        addRule(keywords({QStringLiteral("and"), QStringLiteral("as"), QStringLiteral("assert"),
            QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
            QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("def"),
            QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("except"),
            QStringLiteral("False"), QStringLiteral("finally"), QStringLiteral("for"),
            QStringLiteral("from"), QStringLiteral("if"), QStringLiteral("import"),
            QStringLiteral("in"), QStringLiteral("is"), QStringLiteral("lambda"),
            QStringLiteral("None"), QStringLiteral("not"), QStringLiteral("or"),
            QStringLiteral("pass"), QStringLiteral("raise"), QStringLiteral("return"),
            QStringLiteral("True"), QStringLiteral("try"), QStringLiteral("while"),
            QStringLiteral("with"), QStringLiteral("yield")}), keyword);
        addRule(QStringLiteral("@[A-Za-z_][A-Za-z0-9_.]*"), emphasis);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'"), string);
        addRule(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), number);
        addRule(QStringLiteral("#[^\\n]*"), comment);
    } else if (language_ == Language::Json) {
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"(?=\\s*:)"), keyword);
        addRule(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""), string);
        addRule(QStringLiteral("\\b(?:true|false|null)\\b"), emphasis);
        addRule(QStringLiteral("-?\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"), number);
    } else if (language_ == Language::Html) {
        addRule(QStringLiteral("</?[A-Za-z][^>]*>"), tag);
        addRule(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'"), string);
        commentStart_ = QRegularExpression(QStringLiteral("<!--"));
        commentEnd_ = QRegularExpression(QStringLiteral("-->"));
    } else if (language_ == Language::Markdown) {
        addRule(QStringLiteral("^#{1,6}\\s+.*$"), heading);
        addRule(QStringLiteral("^\\s*>.*$"), comment);
        addRule(QStringLiteral("^\\s*(?:[-+*]|\\d+\\.)\\s+"), keyword);
        addRule(QStringLiteral("\x60[^\x60]+\x60"), string);
        addRule(QStringLiteral("\\*\\*[^*]+\\*\\*|__[^_]+__"), emphasis);
        addRule(QStringLiteral("\\[[^]]+\\]\\([^)]+\\)"), tag);
    }
}

void SyntaxHighlighter::highlightMultilineComments(const QString& text) {
    if (commentStart_.pattern().isEmpty()) return;
    setCurrentBlockState(0);
    qsizetype start = previousBlockState() == 1 ? 0 : text.indexOf(commentStart_);
    while (start >= 0) {
        const auto endMatch = commentEnd_.match(text, start);
        const qsizetype end = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
        const qsizetype length = end < 0 ? text.size() - start
            : end - start + endMatch.capturedLength();
        if (end < 0) setCurrentBlockState(1);
        setFormat(static_cast<int>(start), static_cast<int>(length), commentFormat_);
        start = end < 0 ? -1 : text.indexOf(commentStart_, start + length);
    }
}

void SyntaxHighlighter::highlightBlock(const QString& text) {
    for (const auto& rule : rules_) {
        auto matches = rule.expression.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    highlightMultilineComments(text);
}
