#pragma once

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QColor>
#include <memory>
#include "ea/repository/Repository.hpp"

namespace ea {

enum class NoteColor {
    Yellow,
    Pink,
    Green,
    Cyan,
    Purple
};

class StickyNoteItem : public QGraphicsItem {
public:
    StickyNoteItem(const QString& title, const QString& content, const QString& author, NoteColor color, const QPointF& pos);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QString getTitle() const { return m_title; }
    QString getContent() const { return m_content; }
    void setContent(const QString& content) { m_content = content; update(); }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QString m_title;
    QString m_content;
    QString m_author;
    NoteColor m_color;
    QRectF m_rect;
};

class WhiteboardConnectionItem : public QGraphicsItem {
public:
    WhiteboardConnectionItem(QGraphicsItem* startItem, QGraphicsItem* endItem, const QString& label = "");

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QGraphicsItem* m_start;
    QGraphicsItem* m_end;
    QString m_label;
};

class WhiteboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit WhiteboardWidget(ea::Repository& repo, QWidget* parent = nullptr);
    ~WhiteboardWidget() override = default;

public slots:
    void addStickyNote(NoteColor color);
    void addIdeaCard();
    void clearWhiteboard();
    void exportToImage();

private:
    void setupUI();
    void createSampleBoard();

    ea::Repository& m_repo;
    QGraphicsView* m_graphicsView{nullptr};
    QGraphicsScene* m_scene{nullptr};
};

} // namespace ea
