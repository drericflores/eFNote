#include "DocumentEditor.hpp"

#include <QFileInfo>
#include <QFontDatabase>

DocumentEditor::DocumentEditor(QWidget* parent)
    : QTextEdit(parent) {
    setAcceptRichText(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    document()->setModified(false);
}

QString DocumentEditor::filePath() const {
    return filePath_;
}

void DocumentEditor::setFilePath(const QString& path) {
    filePath_ = path;
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
