#pragma once

#include <QDialog>
#include <QTextDocument>

class QCheckBox;
class QLineEdit;
class QTextEdit;

class SearchReplaceDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SearchReplaceDialog(QWidget* parent = nullptr);
    void setEditor(QTextEdit* editor);

private:
    QTextDocument::FindFlags flags() const;
    bool findNext(bool backwards = false);
    void replaceCurrent();
    void replaceAll();

    QTextEdit* editor_{};
    QLineEdit* findEdit_{};
    QLineEdit* replaceEdit_{};
    QCheckBox* caseSensitive_{};
    QCheckBox* wholeWords_{};
};
