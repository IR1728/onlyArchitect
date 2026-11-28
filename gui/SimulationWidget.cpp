#include "SimulationWidget.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include "ea/simulation/SimulationEngine.hpp"
#include <QSignalBlocker>
#include <unordered_map>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QDateTime>
#include <cmath>
#include <numbers>

namespace ea {

SimulationNodeItem::SimulationNodeItem(const std::string& id, const QString& name, const QString& type, const QRectF& rect)
    : m_id(id), m_name(name), m_type(type), m_rect(rect) {
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QRectF SimulationNodeItem::boundingRect() const {
    return m_rect.adjusted(-10, -10, 10, 10);
}

void SimulationNodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {
    painter->setRenderHint(QPainter::Antialiasing);

    // Fondo y borde
    QColor bgColor = ea::gui::m3().surfaceContainer;
    QColor borderColor = ea::gui::m3().surfaceHighest;

    if (m_active) {
        bgColor = ea::gui::m3().successContainer;
        borderColor = ea::gui::m3().success; // Verde esmeralda simulación activo
    } else if (isSelected()) {
        borderColor = ea::gui::m3().primary; // Azul selección
    }

    if (m_type == "Initial") {
        painter->setBrush(QBrush(m_active ? ea::gui::m3().success : ea::gui::m3().primary));
        painter->setPen(QPen(ea::gui::m3().onSurface, 2));
        painter->drawEllipse(m_rect);
    } else if (m_type == "Final") {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(m_active ? ea::gui::m3().success : ea::gui::m3().error, 2));
        painter->drawEllipse(m_rect);
        painter->setBrush(QBrush(m_active ? ea::gui::m3().success : ea::gui::m3().error));
        painter->drawEllipse(m_rect.adjusted(4, 4, -4, -4));
    } else if (m_type == "Decision") {
        QPolygonF diamond;
        diamond << QPointF(m_rect.center().x(), m_rect.top())
                << QPointF(m_rect.right(), m_rect.center().y())
                << QPointF(m_rect.center().x(), m_rect.bottom())
                << QPointF(m_rect.left(), m_rect.center().y());
        painter->setBrush(QBrush(bgColor));
        painter->setPen(QPen(borderColor, m_active ? 2.5 : 1.5));
        painter->drawPolygon(diamond);

        painter->setPen(QPen(ea::gui::m3().onSurface));
        QFont font = painter->font();
        font.setBold(true);
        font.setPointSize(9);
        painter->setFont(font);
        painter->drawText(m_rect, Qt::AlignCenter, m_name);
    } else {
        // Acción o Estado
        painter->setBrush(QBrush(bgColor));
        painter->setPen(QPen(borderColor, m_active ? 2.5 : 1.5));
        painter->drawRoundedRect(m_rect, 8.0, 8.0);

        // Cabecera estereotipo o tipo
        painter->setPen(QPen(m_active ? ea::gui::m3().success : ea::gui::m3().onSurfaceMuted));
        QFont typeFont = painter->font();
        typeFont.setPointSize(8);
        painter->setFont(typeFont);
        QRectF topRect(m_rect.left(), m_rect.top() + 6, m_rect.width(), 16);
        painter->drawText(topRect, Qt::AlignCenter, QString("«%1»").arg(m_type));

        // Nombre
        painter->setPen(QPen(ea::gui::m3().onSurface));
        QFont nameFont = painter->font();
        nameFont.setBold(true);
        nameFont.setPointSize(10);
        painter->setFont(nameFont);
        QRectF nameRect(m_rect.left() + 6, m_rect.top() + 22, m_rect.width() - 12, m_rect.height() - 28);
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, m_name);
    }

    // Badge de Breakpoint
    if (m_hasBreakpoint) {
        painter->setBrush(QBrush(ea::gui::m3().error));
        painter->setPen(QPen(Qt::white, 1));
        painter->drawEllipse(m_rect.topRight() + QPointF(-4, 4), 6, 6);
    }

    // Indicador activo
    if (m_active) {
        painter->setBrush(QBrush(ea::gui::m3().success));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(m_rect.topLeft() + QPointF(6, 6), 4, 4);
    }
}

void SimulationNodeItem::setActive(bool active) {
    m_active = active;
    update();
}

void SimulationNodeItem::toggleBreakpoint() {
    m_hasBreakpoint = !m_hasBreakpoint;
    update();
}

void SimulationNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        toggleBreakpoint();
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent(event);
}

// ---------------------------------------------------------------------------
// SimulationWidget
// ---------------------------------------------------------------------------

SimulationWidget::SimulationWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    setupUI();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SimulationWidget::onTimerTick);
    buildSimulationFlow();
}

void SimulationWidget::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Barra de herramientas superior estilo panel de control
    auto* toolbarWidget = new QWidget(this);
    toolbarWidget->setObjectName("SimToolbar");
    ea::gui::setThemedStyleSheet(toolbarWidget,
        "#SimToolbar { background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 8px; padding: 6px; }"
    );
    auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(8, 4, 8, 4);
    toolbarLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("<b>⏯️ SIMULADOR Y DEPURADOR</b>"), toolbarWidget);
    ea::gui::setThemedStyleSheet(titleLabel, "color: @primary; font-size: 13px;");
    toolbarLayout->addWidget(titleLabel);

    toolbarLayout->addSpacing(12);

    m_flowSelector = new QComboBox(toolbarWidget);
    ea::gui::setThemedStyleSheet(m_flowSelector, "background-color: @surfaceContainer; color: @onSurface; border: 1px solid @outline; border-radius: 4px; padding: 4px 8px; min-width: 280px;");
    toolbarLayout->addWidget(m_flowSelector);

    toolbarLayout->addStretch();

    // Controles Play / Pause / Step / Stop / Reset
    m_btnPlay = new QPushButton(QString::fromUtf8("▶️ Ejecutar"), toolbarWidget);
    ea::gui::setThemedStyleSheet(m_btnPlay, "background-color: @success; color: white; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    toolbarLayout->addWidget(m_btnPlay);

    m_btnPause = new QPushButton(QString::fromUtf8("⏸️ Pausa"), toolbarWidget);
    ea::gui::setThemedStyleSheet(m_btnPause, "background-color: @surfaceHighest; color: @onSurface; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnPause);

    m_btnStep = new QPushButton(QString::fromUtf8("⏭️ Paso a Paso"), toolbarWidget);
    ea::gui::setThemedStyleSheet(m_btnStep, "background-color: @surfaceContainer; color: @onSurface; border: 1px solid @outline; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnStep);

    m_btnStop = new QPushButton(QString::fromUtf8("⏹️ Detener"), toolbarWidget);
    ea::gui::setThemedStyleSheet(m_btnStop, "background-color: @surfaceContainer; color: @error; border: 1px solid @outline; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnStop);

    m_btnReset = new QPushButton(QString::fromUtf8("Reiniciar"), toolbarWidget);
    ea::gui::M3Icons::decorate(m_btnReset, "sync");
    ea::gui::setThemedStyleSheet(m_btnReset, "background-color: @surfaceContainer; color: @onSurfaceMuted; border: 1px solid @outline; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnReset);

    toolbarLayout->addSpacing(16);

    auto* speedLabel = new QLabel(QString::fromUtf8("Velocidad:"), toolbarWidget);
    ea::gui::setThemedStyleSheet(speedLabel, "color: @onSurfaceMuted;");
    toolbarLayout->addWidget(speedLabel);

    m_speedSlider = new QSlider(Qt::Horizontal, toolbarWidget);
    m_speedSlider->setRange(200, 2000);
    m_speedSlider->setValue(800);
    m_speedSlider->setFixedWidth(100);
    toolbarLayout->addWidget(m_speedSlider);

    m_statusLabel = new QLabel(QString::fromUtf8("⏸️ En Espera"), toolbarWidget);
    ea::gui::setThemedStyleSheet(m_statusLabel, "color: @onSurfaceMuted; font-weight: bold; min-width: 90px;");
    toolbarLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(toolbarWidget);

    // Splitter Vertical principal (Arriba: Escena gráfica, Abajo: Variables y Logs)
    auto* vSplitter = new QSplitter(Qt::Vertical, this);

    // Escena gráfica
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QBrush(ea::gui::m3().surface));
    m_graphicsView = new QGraphicsView(m_scene, vSplitter);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    ea::gui::setThemedStyleSheet(m_graphicsView, "background-color: @surface; border: 1px solid @outlineVariant; border-radius: 8px;");
    vSplitter->addWidget(m_graphicsView);

    // Panel inferior dividido en dos columnas: Izquierda Variables, Derecha Registro de trazas
    auto* bottomWidget = new QWidget(vSplitter);
    auto* bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 4, 0, 0);
    bottomLayout->setSpacing(10);

    // Panel Variables en tiempo real
    auto* varsBox = new QWidget(bottomWidget);
    ea::gui::setThemedStyleSheet(varsBox, "background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 8px; padding: 4px;");
    auto* varsLayout = new QVBoxLayout(varsBox);
    varsLayout->setContentsMargins(8, 6, 8, 6);
    varsLayout->setSpacing(6);

    auto* varsTitle = new QLabel(QString::fromUtf8("<b>VARIABLES EN TIEMPO REAL</b>"), varsBox);
    ea::gui::setThemedStyleSheet(varsTitle, "color: @success; font-size: 11px;");
    varsLayout->addWidget(varsTitle);

    m_varsTable = new QTableWidget(0, 3, varsBox);
    m_varsTable->setHorizontalHeaderLabels({QString::fromUtf8("Variable"), QString::fromUtf8("Tipo"), QString::fromUtf8("Valor Actual")});
    m_varsTable->horizontalHeader()->setStretchLastSection(true);
    m_varsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_varsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ea::gui::setThemedStyleSheet(m_varsTable,
        "QTableWidget { background-color: @surface; color: @onSurface; gridline-color: @outlineVariant; border: none; }"
        "QHeaderView::section { background-color: @surfaceContainer; color: @onSurfaceMuted; border: 1px solid @outlineVariant; padding: 4px; font-weight: bold; }"
    );
    varsLayout->addWidget(m_varsTable);
    bottomLayout->addWidget(varsBox, 45);

    // Panel Registro de Eventos
    auto* logBox = new QWidget(bottomWidget);
    ea::gui::setThemedStyleSheet(logBox, "background-color: @surfaceLow; border: 1px solid @outlineVariant; border-radius: 8px; padding: 4px;");
    auto* logLayout = new QVBoxLayout(logBox);
    logLayout->setContentsMargins(8, 6, 8, 6);
    logLayout->setSpacing(6);

    auto* logTitle = new QLabel(QString::fromUtf8("<b>REGISTRO DE EVENTOS Y TRAZAS DE SIMULACIÓN</b>"), logBox);
    ea::gui::setThemedStyleSheet(logTitle, "color: @primary; font-size: 11px;");
    logLayout->addWidget(logTitle);

    m_logConsole = new QTextEdit(logBox);
    m_logConsole->setReadOnly(true);
    ea::gui::setThemedStyleSheet(m_logConsole, "background-color: @surface; color: @onSurfaceMuted; font-family: monospace; font-size: 11px; border: none;");
    logLayout->addWidget(m_logConsole);
    bottomLayout->addWidget(logBox, 55);

    vSplitter->addWidget(bottomWidget);
    vSplitter->setSizes({460, 220});

    mainLayout->addWidget(vSplitter);

    // Conexiones de control
    connect(m_btnPlay, &QPushButton::clicked, this, &SimulationWidget::play);
    connect(m_btnPause, &QPushButton::clicked, this, &SimulationWidget::pause);
    connect(m_btnStep, &QPushButton::clicked, this, &SimulationWidget::stepForward);
    connect(m_btnStop, &QPushButton::clicked, this, &SimulationWidget::stop);
    connect(m_btnReset, &QPushButton::clicked, this, &SimulationWidget::resetSimulation);
    connect(m_speedSlider, &QSlider::valueChanged, this, &SimulationWidget::setSpeed);

    connect(m_flowSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index >= 0) {
            m_activeDiagramId = m_flowSelector->itemData(index).toInt();
        }
        buildSimulationFlow();
    });

    logEvent("INFO", QString::fromUtf8("Simulador inicializado. Selecciona un diagrama del repositorio y haz clic derecho en un nodo para añadir un Breakpoint."));
    refreshDiagramList();
}

void SimulationWidget::refreshDiagramList(int selectDiagramId) {
    const QSignalBlocker block(m_flowSelector);
    m_flowSelector->clear();

    if (m_repo.IsOpen()) {
        std::function<void(const ea::Package&)> collect = [&](const ea::Package& pkg) {
            for (size_t i = 0; i < pkg.GetDiagrams().size(); ++i) {
                if (auto d = pkg.GetDiagrams().GetAt(static_cast<short>(i))) {
                    const QString label = QString("%1 (%2)")
                        .arg(QString::fromStdString(d->GetName()), QString::fromStdString(to_string(d->GetType())));
                    m_flowSelector->addItem(label, d->GetDiagramID());
                }
            }
            for (size_t i = 0; i < pkg.GetPackages().size(); ++i) {
                if (auto sub = pkg.GetPackages().GetAt(static_cast<short>(i))) collect(*sub);
            }
        };
        for (size_t i = 0; i < m_repo.GetModels().size(); ++i) {
            if (auto m = m_repo.GetModels().GetAt(static_cast<short>(i))) collect(*m);
        }
    }

    if (m_flowSelector->count() == 0) {
        m_flowSelector->addItem(QString::fromUtf8("(Sin diagramas en el repositorio)"), 0);
        m_activeDiagramId = 0;
        return;
    }

    int targetIdx = 0;
    if (selectDiagramId > 0) {
        int found = m_flowSelector->findData(selectDiagramId);
        if (found >= 0) targetIdx = found;
    }
    m_flowSelector->setCurrentIndex(targetIdx);
    m_activeDiagramId = m_flowSelector->itemData(targetIdx).toInt();
}

void SimulationWidget::buildSimulationFlow() {
    m_scene->clear();
    m_nodeItems.clear();
    m_steps.clear();
    m_varsTable->setRowCount(0);
    m_currentStepIndex = -1;

    const auto plan = ea::simulation::SimulationEngine::buildFromDiagram(m_repo, m_activeDiagramId);
    if (plan.empty()) {
        auto* hint = m_scene->addText(QString::fromUtf8("Abre un repositorio y selecciona un diagrama con elementos para simular su flujo."));
        hint->setDefaultTextColor(ea::gui::m3().onSurfaceMuted);
        hint->setPos(40, 40);
        m_statusLabel->setText(QString::fromUtf8("Sin diagrama"));
        return;
    }

    std::unordered_map<std::string, SimulationNodeItem*> itemById;
    for (const auto& node : plan.nodes) {
        auto* item = new SimulationNodeItem(
            node.id,
            QString::fromStdString(node.name),
            QString::fromStdString(node.kind),
            QRectF(node.x, node.y, node.width, node.height));
        m_scene->addItem(item);
        m_nodeItems.push_back(item);
        itemById[node.id] = item;
    }

    auto drawArrow = [this](QPointF from, QPointF to, const QString& label = "") {
        QPen pen(ea::gui::m3().onSurfaceDisabled, 2);
        m_scene->addLine(QLineF(from, to), pen);

        const qreal angle = std::atan2(to.y() - from.y(), to.x() - from.x());
        constexpr qreal arrowSize = 8.0;
        const QPointF p1 = to - QPointF(std::cos(angle - std::numbers::pi / 6.0) * arrowSize, std::sin(angle - std::numbers::pi / 6.0) * arrowSize);
        const QPointF p2 = to - QPointF(std::cos(angle + std::numbers::pi / 6.0) * arrowSize, std::sin(angle + std::numbers::pi / 6.0) * arrowSize);
        QPolygonF head;
        head << to << p1 << p2;
        m_scene->addPolygon(head, pen, QBrush(ea::gui::m3().onSurfaceDisabled));

        if (!label.isEmpty()) {
            auto* text = m_scene->addText(label);
            text->setDefaultTextColor(ea::gui::m3().onSurfaceMuted);
            text->setFont(ea::gui::M3Typography::labelMedium());
            text->setPos((from.x() + to.x()) / 2.0 - 20.0, (from.y() + to.y()) / 2.0 - 18.0);
        }
    };

    for (const auto& edge : plan.edges) {
        auto itFrom = itemById.find(edge.fromNodeId);
        auto itTo = itemById.find(edge.toNodeId);
        if (itFrom == itemById.end() || itTo == itemById.end()) continue;
        const QRectF rFrom = itFrom->second->boundingRect();
        const QRectF rTo = itTo->second->boundingRect();
        const QPointF pFrom(rFrom.right() - 8.0, rFrom.center().y());
        const QPointF pTo(rTo.left() + 8.0, rTo.center().y());
        drawArrow(pFrom, pTo, QString::fromStdString(edge.label));
    }

    for (const auto& s : plan.steps) {
        m_steps.push_back({s.nodeId, s.nodeName, s.nodeKind, s.logMessage, s.variables});
    }

    const QRectF bounds = m_scene->itemsBoundingRect().adjusted(-60, -60, 60, 60);
    m_scene->setSceneRect(bounds);
    if (m_graphicsView && !bounds.isEmpty()) {
        m_graphicsView->fitInView(bounds, Qt::KeepAspectRatio);
    }

    updateVariablesTable({
        {"diagram", plan.diagramName},
        {"diagram_type", to_string(plan.diagramType)},
        {"nodes", std::to_string(plan.nodes.size())},
        {"transitions", std::to_string(plan.edges.size())},
        {"steps", std::to_string(plan.steps.size())}
    });
    logEvent("INFO", QString::fromUtf8("Diagrama \"%1\" cargado: %2 nodos, %3 conexiones, %4 pasos.")
                         .arg(QString::fromStdString(plan.diagramName))
                         .arg(plan.nodes.size())
                         .arg(plan.edges.size())
                         .arg(plan.steps.size()));
}

void SimulationWidget::play() {
    if (m_steps.empty()) return;
    m_isRunning = true;
    m_btnPlay->setEnabled(false);
    m_btnPause->setEnabled(true);
    m_statusLabel->setText(QString::fromUtf8("Ejecutando..."));
    ea::gui::setThemedStyleSheet(m_statusLabel, "color: @success; font-weight: bold;");

    int interval = m_speedSlider->value();
    m_timer->start(interval);
}

void SimulationWidget::pause() {
    m_isRunning = false;
    m_timer->stop();
    m_btnPlay->setEnabled(true);
    m_btnPause->setEnabled(false);
    m_statusLabel->setText(QString::fromUtf8("⏸️ En Pausa"));
    ea::gui::setThemedStyleSheet(m_statusLabel, "color: @warning; font-weight: bold;");
}

void SimulationWidget::stepForward() {
    if (m_currentStepIndex + 1 >= static_cast<int>(m_steps.size())) {
        stop();
        logEvent("SUCCESS", QString::fromUtf8("Simulación completada con éxito. Todos los pasos fueron ejecutados."));
        return;
    }

    m_currentStepIndex++;
    updateVisuals();

    const auto& step = m_steps[m_currentStepIndex];
    logEvent("STEP", QString("[%1] %2 — %3").arg(m_currentStepIndex + 1).arg(QString::fromStdString(step.nodeName)).arg(QString::fromStdString(step.logMessage)));
    updateVariablesTable(step.variableUpdates);

    // Revisar si el nodo activo tiene breakpoint
    for (auto* item : m_nodeItems) {
        if (item->getId() == step.nodeId && item->hasBreakpoint()) {
            pause();
            logEvent("BREAKPOINT", QString::fromUtf8("Breakpoint alcanzado en: %1. Simulación pausada.").arg(QString::fromStdString(step.nodeName)));
            break;
        }
    }
}

void SimulationWidget::stop() {
    m_isRunning = false;
    m_timer->stop();
    m_btnPlay->setEnabled(true);
    m_btnPause->setEnabled(false);
    m_statusLabel->setText(QString::fromUtf8("⏹️ Detenido"));
    ea::gui::setThemedStyleSheet(m_statusLabel, "color: @error; font-weight: bold;");
}

void SimulationWidget::resetSimulation() {
    stop();
    m_currentStepIndex = -1;
    for (auto* item : m_nodeItems) {
        item->setActive(false);
    }
    m_statusLabel->setText(QString::fromUtf8("Reiniciado"));
    ea::gui::setThemedStyleSheet(m_statusLabel, "color: @onSurfaceMuted; font-weight: bold;");
    logEvent("INFO", QString::fromUtf8("Simulación reiniciada al estado de inicio."));
    buildSimulationFlow();
}

void SimulationWidget::setSpeed(int value) {
    if (m_isRunning) {
        m_timer->setInterval(value);
    }
}

void SimulationWidget::onTimerTick() {
    stepForward();
}

void SimulationWidget::updateVisuals() {
    if (m_currentStepIndex < 0 || m_currentStepIndex >= static_cast<int>(m_steps.size())) return;
    const std::string& activeId = m_steps[m_currentStepIndex].nodeId;

    for (auto* item : m_nodeItems) {
        item->setActive(item->getId() == activeId);
    }
}

void SimulationWidget::logEvent(const QString& level, const QString& msg) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString color = ea::gui::themed("@onSurfaceMuted");

    if (level == "SUCCESS") color = ea::gui::themed("@success");
    else if (level == "STEP") color = ea::gui::themed("@primary");
    else if (level == "BREAKPOINT") color = ea::gui::themed("@error");
    else if (level == "WARN") color = ea::gui::themed("@warning");

    m_logConsole->append(ea::gui::themed("<span style='color: @onSurfaceDisabled;'>[%1]</span> <b style='color: %2;'>[%3]</b> %4")
                         .arg(timeStr)
                         .arg(color)
                         .arg(level)
                         .arg(msg));
}

void SimulationWidget::updateVariablesTable(const std::vector<std::pair<std::string, std::string>>& updates) {
    for (const auto& [varName, varVal] : updates) {
        bool found = false;
        for (int r = 0; r < m_varsTable->rowCount(); ++r) {
            if (m_varsTable->item(r, 0) && m_varsTable->item(r, 0)->text() == QString::fromStdString(varName)) {
                auto* valItem = m_varsTable->item(r, 2);
                if (valItem) {
                    valItem->setText(QString::fromStdString(varVal));
                    valItem->setForeground(QBrush(ea::gui::m3().primary)); // Resaltar cambio
                }
                found = true;
                break;
            }
        }

        if (!found) {
            int newRow = m_varsTable->rowCount();
            m_varsTable->insertRow(newRow);

            auto* item0 = new QTableWidgetItem(QString::fromStdString(varName));
            item0->setForeground(QBrush(ea::gui::m3().onSurface));

            QString inferredType = "String";
            if (varVal.find('$') != std::string::npos) inferredType = "Currency";
            else if (varVal == "True" || varVal == "False" || varVal == "APROBADO") inferredType = "Boolean";
            else if (varVal.find("ms") != std::string::npos || varVal.find("unidades") != std::string::npos) inferredType = "Quantity";
            else if (std::all_of(varVal.begin(), varVal.end(), ::isdigit)) inferredType = "Integer";

            auto* item1 = new QTableWidgetItem(inferredType);
            item1->setForeground(QBrush(ea::gui::m3().onSurfaceMuted));

            auto* item2 = new QTableWidgetItem(QString::fromStdString(varVal));
            item2->setForeground(QBrush(ea::gui::m3().success));

            m_varsTable->setItem(newRow, 0, item0);
            m_varsTable->setItem(newRow, 1, item1);
            m_varsTable->setItem(newRow, 2, item2);
        }
    }
}

void SimulationWidget::loadDiagramForSimulation(int diagramId) {
    refreshDiagramList(diagramId);
    buildSimulationFlow();
}

} // namespace ea
