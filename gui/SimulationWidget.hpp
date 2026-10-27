#pragma once

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QTimer>
#include <vector>
#include <string>
#include <memory>
#include "ea/model/Element.hpp"
#include "ea/model/Diagram.hpp"
#include "ea/repository/Repository.hpp"

namespace ea {

struct SimStep {
    std::string nodeId;
    std::string nodeName;
    std::string nodeType; // "Action", "Decision", "State", "Initial", "Final"
    std::string logMessage;
    std::vector<std::pair<std::string, std::string>> variableUpdates; // {name, value}
};

class SimulationNodeItem : public QGraphicsItem {
public:
    SimulationNodeItem(const std::string& id, const QString& name, const QString& type, const QRectF& rect);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void setActive(bool active);
    bool isActive() const { return m_active; }

    void toggleBreakpoint();
    bool hasBreakpoint() const { return m_hasBreakpoint; }

    std::string getId() const { return m_id; }
    QString getName() const { return m_name; }
    QString getType() const { return m_type; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    std::string m_id;
    QString m_name;
    QString m_type;
    QRectF m_rect;
    bool m_active{false};
    bool m_hasBreakpoint{false};
};

class SimulationWidget : public QWidget {
    Q_OBJECT

public:
    explicit SimulationWidget(ea::Repository& repo, QWidget* parent = nullptr);
    ~SimulationWidget() override = default;

    void loadDiagramForSimulation(int diagramId);
    void resetSimulation();

public slots:
    void play();
    void pause();
    void stepForward();
    void stop();
    void setSpeed(int value);
    void onTimerTick();

private:
    void setupUI();
    void buildSimulationFlow();
    void updateVisuals();
    void logEvent(const QString& level, const QString& msg);
    void updateVariablesTable(const std::vector<std::pair<std::string, std::string>>& updates);

    ea::Repository& m_repo;
    QTimer* m_timer{nullptr};
    int m_currentStepIndex{-1};
    bool m_isRunning{false};

    // UI elements
    QPushButton* m_btnPlay{nullptr};
    QPushButton* m_btnPause{nullptr};
    QPushButton* m_btnStep{nullptr};
    QPushButton* m_btnStop{nullptr};
    QPushButton* m_btnReset{nullptr};
    QSlider* m_speedSlider{nullptr};
    QLabel* m_statusLabel{nullptr};
    QComboBox* m_flowSelector{nullptr};

    QGraphicsView* m_graphicsView{nullptr};
    QGraphicsScene* m_scene{nullptr};
    QTableWidget* m_varsTable{nullptr};
    QTextEdit* m_logConsole{nullptr};

    std::vector<SimStep> m_steps;
    std::vector<SimulationNodeItem*> m_nodeItems;
};

} // namespace ea
