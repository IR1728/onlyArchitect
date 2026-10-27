#include "NotesWidget.hpp"
#include <QTextCursor>
#include <QTextList>

namespace ea::gui {

NotesWidget::NotesWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUi();
}

void NotesWidget::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Barra de herramientas de formato de texto WYSIWYG
    auto tbWidget = new QWidget(this);
    tbWidget->setStyleSheet("background-color: #1a1f2b; border: 1px solid #283042; border-radius: 4px; padding: 2px;");
    auto tbLayout = new QHBoxLayout(tbWidget);
    tbLayout->setContentsMargins(4, 2, 4, 2);
    tbLayout->setSpacing(4);

    auto createFormatBtn = [this, tbWidget](const QString& text, const QString& tooltip) -> QToolButton* {
        auto btn = new QToolButton(tbWidget);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setStyleSheet(
            "QToolButton { background: transparent; color: #cbd5e1; border: 1px solid transparent; border-radius: 3px; font-weight: bold; padding: 2px 6px; min-width: 18px; }"
            "QToolButton:hover { background-color: #242c3d; color: #ffffff; border-color: #38455e; }"
            "QToolButton:checked { background-color: #007acc; color: #ffffff; border-color: #38bdf8; }"
        );
        return btn;
    };

    m_btnBold = createFormatBtn("B", "Negrita");
    connect(m_btnBold, &QToolButton::clicked, this, &NotesWidget::toggleBold);
    tbLayout->addWidget(m_btnBold);

    m_btnItalic = createFormatBtn("I", "Cursiva");
    connect(m_btnItalic, &QToolButton::clicked, this, &NotesWidget::toggleItalic);
    tbLayout->addWidget(m_btnItalic);

    m_btnUnderline = createFormatBtn("U", "Subrayado");
    connect(m_btnUnderline, &QToolButton::clicked, this, &NotesWidget::toggleUnderline);
    tbLayout->addWidget(m_btnUnderline);

    tbLayout->addSpacing(6);

    m_btnBullets = createFormatBtn("• ≡", "Lista con viñetas");
    connect(m_btnBullets, &QToolButton::clicked, this, &NotesWidget::toggleBulletList);
    tbLayout->addWidget(m_btnBullets);

    m_btnNumbered = createFormatBtn("1. ≡", "Lista numerada");
    connect(m_btnNumbered, &QToolButton::clicked, this, &NotesWidget::toggleNumberedList);
    tbLayout->addWidget(m_btnNumbered);

    tbLayout->addStretch();
    mainLayout->addWidget(tbWidget);

    // Editor de texto
    m_textEdit = new QTextEdit(this);
    m_textEdit->setPlaceholderText("Escribe notas o especificaciones del elemento...");
    m_textEdit->setStyleSheet(
        "QTextEdit { background-color: #171c26; color: #f1f5f9; border: 1px solid #283042; border-radius: 4px; padding: 6px; font-size: 11.5px; font-family: 'Segoe UI', 'Inter', sans-serif; }"
        "QTextEdit:focus { border-color: #38bdf8; }"
    );
    mainLayout->addWidget(m_textEdit);

    connect(m_textEdit, &QTextEdit::textChanged, this, &NotesWidget::onNotesChanged);
}

void NotesWidget::setElement(std::shared_ptr<ea::Element> elem) {
    m_updating = true;
    m_currentElem = elem;
    if (m_currentElem) {
        m_textEdit->setPlainText(QString::fromStdString(m_currentElem->GetNotes()));
        m_textEdit->setEnabled(true);
    } else {
        m_textEdit->clear();
        m_textEdit->setEnabled(false);
    }
    m_updating = false;
}

void NotesWidget::clear() {
    m_updating = true;
    m_currentElem.reset();
    m_textEdit->clear();
    m_textEdit->setEnabled(false);
    m_updating = false;
}

void NotesWidget::onNotesChanged() {
    if (m_updating || !m_currentElem) return;
    std::string text = m_textEdit->toPlainText().toStdString();
    m_currentElem->SetNotes(text);
    if (m_repo.GetStorageEngine()) {
        m_repo.GetStorageEngine()->SaveElement(*m_currentElem);
    }
}

void NotesWidget::toggleBold() {
    QTextCharFormat fmt;
    fmt.setFontWeight(m_btnBold->isChecked() ? QFont::Bold : QFont::Normal);
    m_textEdit->mergeCurrentCharFormat(fmt);
}

void NotesWidget::toggleItalic() {
    QTextCharFormat fmt;
    fmt.setFontItalic(m_btnItalic->isChecked());
    m_textEdit->mergeCurrentCharFormat(fmt);
}

void NotesWidget::toggleUnderline() {
    QTextCharFormat fmt;
    fmt.setFontUnderline(m_btnUnderline->isChecked());
    m_textEdit->mergeCurrentCharFormat(fmt);
}

void NotesWidget::toggleBulletList() {
    auto cursor = m_textEdit->textCursor();
    cursor.createList(QTextListFormat::ListDisc);
}

void NotesWidget::toggleNumberedList() {
    auto cursor = m_textEdit->textCursor();
    cursor.createList(QTextListFormat::ListDecimal);
}

} // namespace ea::gui
