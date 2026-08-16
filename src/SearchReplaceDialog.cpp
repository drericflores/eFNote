#include "SearchReplaceDialog.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

SearchReplaceDialog::SearchReplaceDialog(QWidget* parent)
    : QDialog(parent),
      findEdit_(new QLineEdit(this)),
      replaceEdit_(new QLineEdit(this)),
      caseSensitive_(new QCheckBox(tr("Match case"), this)),
      wholeWords_(new QCheckBox(tr("Whole words"), this)) {
    setWindowTitle(tr("Find and Replace"));
    setModal(false);
    resize(520, 190);

    auto* form = new QFormLayout;
    form->addRow(tr("Find:"), findEdit_);
    form->addRow(tr("Replace with:"), replaceEdit_);

    auto* options = new QHBoxLayout;
    options->addWidget(caseSensitive_);
    options->addWidget(wholeWords_);
    options->addStretch();

    auto* findNextButton = new QPushButton(tr("Find Next"), this);
    auto* findPrevious = new QPushButton(tr("Find Previous"), this);
    auto* replace = new QPushButton(tr("Replace"), this);
    auto* replaceAllButton = new QPushButton(tr("Replace All"), this);
    auto* close = new QPushButton(tr("Close"), this);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(findPrevious);
    buttons->addWidget(findNextButton);
    buttons->addWidget(replace);
    buttons->addWidget(replaceAllButton);
    buttons->addStretch();
    buttons->addWidget(close);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(options);
    layout->addLayout(buttons);

    connect(findNextButton, &QPushButton::clicked, this, [this] { findNext(false); });
    connect(findPrevious, &QPushButton::clicked, this, [this] { findNext(true); });
    connect(replace, &QPushButton::clicked, this, &SearchReplaceDialog::replaceCurrent);
    connect(replaceAllButton, &QPushButton::clicked, this, &SearchReplaceDialog::replaceAll);
    connect(close, &QPushButton::clicked, this, &QDialog::hide);
    connect(findEdit_, &QLineEdit::returnPressed, this, [this] { findNext(false); });
}

void SearchReplaceDialog::setEditor(QTextEdit* editor) {
    editor_ = editor;
    if (editor_ && editor_->textCursor().hasSelection()) {
        findEdit_->setText(editor_->textCursor().selectedText());
    }
    findEdit_->setFocus();
    findEdit_->selectAll();
}

QTextDocument::FindFlags SearchReplaceDialog::flags() const {
    QTextDocument::FindFlags result;
    if (caseSensitive_->isChecked()) result |= QTextDocument::FindCaseSensitively;
    if (wholeWords_->isChecked()) result |= QTextDocument::FindWholeWords;
    return result;
}

bool SearchReplaceDialog::findNext(bool backwards) {
    if (!editor_ || findEdit_->text().isEmpty()) return false;
    auto searchFlags = flags();
    if (backwards) searchFlags |= QTextDocument::FindBackward;
    if (editor_->find(findEdit_->text(), searchFlags)) return true;

    auto cursor = editor_->textCursor();
    cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    editor_->setTextCursor(cursor);
    if (editor_->find(findEdit_->text(), searchFlags)) return true;
    QMessageBox::information(this, tr("Find"), tr("No match was found."));
    return false;
}

void SearchReplaceDialog::replaceCurrent() {
    if (!editor_) return;
    auto cursor = editor_->textCursor();
    if (cursor.hasSelection()) cursor.insertText(replaceEdit_->text());
    findNext(false);
}

void SearchReplaceDialog::replaceAll() {
    if (!editor_ || findEdit_->text().isEmpty()) return;
    auto cursor = editor_->textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    editor_->setTextCursor(cursor);
    int replacements = 0;
    while (editor_->find(findEdit_->text(), flags())) {
        auto match = editor_->textCursor();
        match.insertText(replaceEdit_->text());
        ++replacements;
    }
    cursor = editor_->textCursor();
    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
    QMessageBox::information(this, tr("Replace All"),
                             tr("Replaced %1 occurrence(s).").arg(replacements));
}
