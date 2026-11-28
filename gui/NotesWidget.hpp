#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <memory>
#include "ea/model/Element.hpp"
#include "ea/repository/Repository.hpp"

namespace ea::gui {

class NotesWidget : public QWidget {
    Q_OBJECT
public:
    explicit NotesWidget(ea::Repository& repo, QWidget* parent = nullptr);

    void setElement(std::shared_ptr<ea::Element> elem);
    void clear();

private slots:
    void onNotesChanged();
    void toggleBold();
    void toggleItalic();
    void toggleUnderline();
    void toggleBulletList();
    void toggleNumberedList();

private:
    void setupUi();

    ea::Repository& m_repo;
    std::shared_ptr<ea::Element> m_currentElem;

    QTextEdit* m_textEdit{nullptr};
    QToolButton* m_btnBold{nullptr};
    QToolButton* m_btnItalic{nullptr};
    QToolButton* m_btnUnderline{nullptr};
    QToolButton* m_btnBullets{nullptr};
    QToolButton* m_btnNumbered{nullptr};
    bool m_updating{false};
};

} // namespace ea::gui
