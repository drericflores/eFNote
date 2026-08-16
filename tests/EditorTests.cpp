#include "DocumentEditor.hpp"

#include <QTextCursor>
#include <QTextDocument>
#include <QtTest/QTest>

class EditorTests final : public QObject {
    Q_OBJECT
private slots:
    void recognizesLanguages();
    void preservesDocumentMetadata();
    void controlsEditorPresentation();
    void performsAutomaticIndentation();
};

void EditorTests::recognizesLanguages() {
    DocumentEditor editor;
    editor.setFilePath(QStringLiteral("/tmp/example.cpp"));
    QCOMPARE(editor.languageName(), QStringLiteral("C/C++"));
    editor.setFilePath(QStringLiteral("/tmp/example.py"));
    QCOMPARE(editor.languageName(), QStringLiteral("Python"));
    editor.setFilePath(QStringLiteral("/tmp/example.md"));
    QCOMPARE(editor.languageName(), QStringLiteral("Markdown"));
    editor.setFilePath(QStringLiteral("/tmp/example.json"));
    QCOMPARE(editor.languageName(), QStringLiteral("JSON"));
    editor.setFilePath(QStringLiteral("/tmp/example.html"));
    QCOMPARE(editor.languageName(), QStringLiteral("HTML"));
    editor.setFilePath(QStringLiteral("/tmp/example.unknown"));
    QCOMPARE(editor.languageName(), QStringLiteral("Plain Text"));
}

void EditorTests::preservesDocumentMetadata() {
    DocumentEditor editor;
    editor.setTextEncoding(QStringLiteral("UTF-16LE"), true);
    editor.setLineEnding(QStringLiteral("\r\n"));
    QCOMPARE(editor.textEncoding(), QStringLiteral("UTF-16LE"));
    QVERIFY(editor.hasByteOrderMark());
    QCOMPARE(editor.lineEnding(), QStringLiteral("\r\n"));
}

void EditorTests::controlsEditorPresentation() {
    DocumentEditor editor;
    editor.setWordWrapEnabled(true);
    QVERIFY(editor.wordWrapEnabled());
    editor.setWordWrapEnabled(false);
    QVERIFY(!editor.wordWrapEnabled());
    editor.setWhitespaceVisible(true);
    QVERIFY(editor.whitespaceVisible());
    editor.setWhitespaceVisible(false);
    QVERIFY(!editor.whitespaceVisible());
}

void EditorTests::performsAutomaticIndentation() {
    DocumentEditor editor;
    editor.setPlainText(QStringLiteral("if (ready) {"));
    auto cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::End);
    editor.setTextCursor(cursor);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QStringLiteral("if (ready) {\n    "));
}

QTEST_MAIN(EditorTests)
#include "EditorTests.moc"
