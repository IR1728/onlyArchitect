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
    enum { Type = UserType + 4 };
    int type() const override { return Type; }

    StickyNoteItem(const QString& title, const QString& content, const QString& author, NoteColor color,
                   const QPointF& pos, int elementId = 0,
                   std::function<void(int, const QString&, const QPointF&)> onChanged = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    int elementId() const { return m_elementId; }
    QString getTitle() const { return m_title; }
    QString getContent() const { return m_content; }
    void setContent(const QString& content) { m_content = content; update(); }

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    int m_elementId{0};
    QString m_title;
    QString m_content;
    QString m_author;
    NoteColor m_color;
    QRectF m_rect;
    std::function<void(int, const QString&, const QPointF&)> m_onChanged;
};

class WhiteboardConnectionItem : public QGraphicsItem {
public:
    enum { Type = UserType + 5 };
    int type() const override { return Type; }

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
    void refreshFromRepository();
    void addStickyNote(NoteColor color);
    void addIdeaCard();
    void clearWhiteboard();
    void exportToImage();

private:
    void setupUI();
    std::shared_ptr<ea::Diagram> ensureWhiteboardDiagram(int& outPackageId);
    void persistNoteChange(int elementId, const QString& content, const QPointF& pos);

    ea::Repository& m_repo;
    int m_whiteboardDiagramId{0};
    QGraphicsView* m_graphicsView{nullptr};
    QGraphicsScene* m_scene{nullptr};
};

} // namespace ea
