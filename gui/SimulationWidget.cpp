#include "SimulationWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QDateTime>
#include <cmath>

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
    QColor bgColor(34, 39, 51);
    QColor borderColor(56, 66, 82);

    if (m_active) {
        bgColor = QColor(16, 55, 38);
        borderColor = QColor(16, 185, 129); // Verde esmeralda simulación activo
    } else if (isSelected()) {
        borderColor = QColor(56, 189, 248); // Azul selección
    }

    if (m_type == "Initial") {
        painter->setBrush(QBrush(m_active ? QColor(16, 185, 129) : QColor(56, 189, 248)));
        painter->setPen(QPen(QColor(255, 255, 255), 2));
        painter->drawEllipse(m_rect);
    } else if (m_type == "Final") {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(m_active ? QColor(16, 185, 129) : QColor(244, 63, 94), 2));
        painter->drawEllipse(m_rect);
        painter->setBrush(QBrush(m_active ? QColor(16, 185, 129) : QColor(244, 63, 94)));
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

        painter->setPen(QPen(QColor(241, 245, 249)));
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
        painter->setPen(QPen(m_active ? QColor(110, 231, 183) : QColor(148, 163, 184)));
        QFont typeFont = painter->font();
        typeFont.setPointSize(8);
        painter->setFont(typeFont);
        QRectF topRect(m_rect.left(), m_rect.top() + 6, m_rect.width(), 16);
        painter->drawText(topRect, Qt::AlignCenter, QString("«%1»").arg(m_type));

        // Nombre
        painter->setPen(QPen(QColor(241, 245, 249)));
        QFont nameFont = painter->font();
        nameFont.setBold(true);
        nameFont.setPointSize(10);
        painter->setFont(nameFont);
        QRectF nameRect(m_rect.left() + 6, m_rect.top() + 22, m_rect.width() - 12, m_rect.height() - 28);
        painter->drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, m_name);
    }

    // Badge de Breakpoint 🔴
    if (m_hasBreakpoint) {
        painter->setBrush(QBrush(QColor(239, 68, 68)));
        painter->setPen(QPen(Qt::white, 1));
        painter->drawEllipse(m_rect.topRight() + QPointF(-4, 4), 6, 6);
    }

    // Indicador activo
    if (m_active) {
        painter->setBrush(QBrush(QColor(16, 185, 129)));
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
    toolbarWidget->setStyleSheet(
        "#SimToolbar { background-color: #1e222d; border: 1px solid #2b3140; border-radius: 8px; padding: 6px; }"
    );
    auto* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(8, 4, 8, 4);
    toolbarLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QString::fromUtf8("<b>⏯️ SIMULADOR Y DEPURADOR</b>"), toolbarWidget);
    titleLabel->setStyleSheet("color: #38bdf8; font-size: 13px;");
    toolbarLayout->addWidget(titleLabel);

    toolbarLayout->addSpacing(12);

    m_flowSelector = new QComboBox(toolbarWidget);
    m_flowSelector->addItem(QString::fromUtf8("Flujo: Procesamiento y Validación de Pedido (Actividad)"));
    m_flowSelector->addItem(QString::fromUtf8("Flujo: Máquina de Estados de Facturación"));
    m_flowSelector->addItem(QString::fromUtf8("Flujo: Secuencia Autenticación y Autorización"));
    m_flowSelector->setStyleSheet("background-color: #242936; color: #f1f5f9; border: 1px solid #384252; border-radius: 4px; padding: 4px 8px; min-width: 280px;");
    toolbarLayout->addWidget(m_flowSelector);

    toolbarLayout->addStretch();

    // Controles Play / Pause / Step / Stop / Reset
    m_btnPlay = new QPushButton(QString::fromUtf8("▶️ Ejecutar"), toolbarWidget);
    m_btnPlay->setStyleSheet("background-color: #10b981; color: white; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    toolbarLayout->addWidget(m_btnPlay);

    m_btnPause = new QPushButton(QString::fromUtf8("⏸️ Pausa"), toolbarWidget);
    m_btnPause->setStyleSheet("background-color: #384252; color: #f1f5f9; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnPause);

    m_btnStep = new QPushButton(QString::fromUtf8("⏭️ Paso a Paso"), toolbarWidget);
    m_btnStep->setStyleSheet("background-color: #242936; color: #f1f5f9; border: 1px solid #384252; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnStep);

    m_btnStop = new QPushButton(QString::fromUtf8("⏹️ Detener"), toolbarWidget);
    m_btnStop->setStyleSheet("background-color: #242936; color: #f43f5e; border: 1px solid #384252; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnStop);

    m_btnReset = new QPushButton(QString::fromUtf8("🔄 Reiniciar"), toolbarWidget);
    m_btnReset->setStyleSheet("background-color: #242936; color: #94a3b8; border: 1px solid #384252; border-radius: 4px; padding: 6px 10px;");
    toolbarLayout->addWidget(m_btnReset);

    toolbarLayout->addSpacing(16);

    auto* speedLabel = new QLabel(QString::fromUtf8("Velocidad:"), toolbarWidget);
    speedLabel->setStyleSheet("color: #94a3b8;");
    toolbarLayout->addWidget(speedLabel);

    m_speedSlider = new QSlider(Qt::Horizontal, toolbarWidget);
    m_speedSlider->setRange(200, 2000);
    m_speedSlider->setValue(800);
    m_speedSlider->setFixedWidth(100);
    toolbarLayout->addWidget(m_speedSlider);

    m_statusLabel = new QLabel(QString::fromUtf8("⏸️ En Espera"), toolbarWidget);
    m_statusLabel->setStyleSheet("color: #94a3b8; font-weight: bold; min-width: 90px;");
    toolbarLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(toolbarWidget);

    // Splitter Vertical principal (Arriba: Escena gráfica, Abajo: Variables y Logs)
    auto* vSplitter = new QSplitter(Qt::Vertical, this);

    // Escena gráfica
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QBrush(QColor(22, 25, 34)));
    m_graphicsView = new QGraphicsView(m_scene, vSplitter);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setStyleSheet("background-color: #161922; border: 1px solid #2b3140; border-radius: 8px;");
    vSplitter->addWidget(m_graphicsView);

    // Panel inferior dividido en dos columnas: Izquierda Variables, Derecha Registro de trazas
    auto* bottomWidget = new QWidget(vSplitter);
    auto* bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 4, 0, 0);
    bottomLayout->setSpacing(10);

    // Panel Variables en tiempo real
    auto* varsBox = new QWidget(bottomWidget);
    varsBox->setStyleSheet("background-color: #1e222d; border: 1px solid #2b3140; border-radius: 8px; padding: 4px;");
    auto* varsLayout = new QVBoxLayout(varsBox);
    varsLayout->setContentsMargins(8, 6, 8, 6);
    varsLayout->setSpacing(6);

    auto* varsTitle = new QLabel(QString::fromUtf8("<b>📊 VARIABLES EN TIEMPO REAL</b>"), varsBox);
    varsTitle->setStyleSheet("color: #10b981; font-size: 11px;");
    varsLayout->addWidget(varsTitle);

    m_varsTable = new QTableWidget(0, 3, varsBox);
    m_varsTable->setHorizontalHeaderLabels({QString::fromUtf8("Variable"), QString::fromUtf8("Tipo"), QString::fromUtf8("Valor Actual")});
    m_varsTable->horizontalHeader()->setStretchLastSection(true);
    m_varsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_varsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_varsTable->setStyleSheet(
        "QTableWidget { background-color: #161922; color: #f1f5f9; gridline-color: #2b3140; border: none; }"
        "QHeaderView::section { background-color: #242936; color: #94a3b8; border: 1px solid #2b3140; padding: 4px; font-weight: bold; }"
    );
    varsLayout->addWidget(m_varsTable);
    bottomLayout->addWidget(varsBox, 45);

    // Panel Registro de Eventos
    auto* logBox = new QWidget(bottomWidget);
    logBox->setStyleSheet("background-color: #1e222d; border: 1px solid #2b3140; border-radius: 8px; padding: 4px;");
    auto* logLayout = new QVBoxLayout(logBox);
    logLayout->setContentsMargins(8, 6, 8, 6);
    logLayout->setSpacing(6);

    auto* logTitle = new QLabel(QString::fromUtf8("<b>📜 REGISTRO DE EVENTOS Y TRAZAS DE SIMULACIÓN</b>"), logBox);
    logTitle->setStyleSheet("color: #38bdf8; font-size: 11px;");
    logLayout->addWidget(logTitle);

    m_logConsole = new QTextEdit(logBox);
    m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet("background-color: #161922; color: #94a3b8; font-family: monospace; font-size: 11px; border: none;");
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

    connect(m_flowSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        buildSimulationFlow();
    });

    logEvent("INFO", QString::fromUtf8("Simulador inicializado. Haz clic derecho sobre cualquier nodo para colocar un Breakpoint 🔴."));
}

void SimulationWidget::buildSimulationFlow() {
    m_scene->clear();
    m_nodeItems.clear();
    m_steps.clear();
    m_varsTable->setRowCount(0);
    m_currentStepIndex = -1;

    // Configurar flujo ilustrativo completo de actividad/estado
    // Pasos preconfigurados:
    // 1. Inicio
    // 2. Recibir Pedido
    // 3. Validar Stock y Crédito (Decisión)
    // 4. Preparar Despacho
    // 5. Emitir Factura Fiscal
    // 6. Notificar al Cliente
    // 7. Fin

    auto* initItem = new SimulationNodeItem("n_init", "", "Initial", QRectF(40, 160, 32, 32));
    auto* n1 = new SimulationNodeItem("n_recibir", QString::fromUtf8("Recibir Solicitud de Pedido"), "Action", QRectF(110, 140, 160, 70));
    auto* n2 = new SimulationNodeItem("n_validar", QString::fromUtf8("¿Stock y Crédito Válidos?"), "Decision", QRectF(320, 140, 160, 70));
    auto* n3 = new SimulationNodeItem("n_despacho", QString::fromUtf8("Preparar Despacho en Bodega"), "Action", QRectF(530, 80, 170, 70));
    auto* n4 = new SimulationNodeItem("n_rechazo", QString::fromUtf8("Notificar Rechazo de Orden"), "Action", QRectF(530, 220, 170, 70));
    auto* n5 = new SimulationNodeItem("n_factura", QString::fromUtf8("Emitir Factura Fiscal y Timbrado"), "Action", QRectF(750, 80, 170, 70));
    auto* n6 = new SimulationNodeItem("n_notificar", QString::fromUtf8("Enviar Correo con Guía de Rastreo"), "Action", QRectF(970, 140, 170, 70));
    auto* endItem = new SimulationNodeItem("n_end", "", "Final", QRectF(1190, 160, 32, 32));

    m_scene->addItem(initItem);
    m_scene->addItem(n1);
    m_scene->addItem(n2);
    m_scene->addItem(n3);
    m_scene->addItem(n4);
    m_scene->addItem(n5);
    m_scene->addItem(n6);
    m_scene->addItem(endItem);

    m_nodeItems = {initItem, n1, n2, n3, n5, n6, endItem};

    // Dibujar líneas y flechas entre nodos
    auto drawArrow = [this](QPointF from, QPointF to, const QString& label = "") {
        QPen pen(QColor(71, 85, 105), 2);
        m_scene->addLine(QLineF(from, to), pen);

        // Flecha punta
        qreal angle = std::atan2(to.y() - from.y(), to.x() - from.x());
        qreal arrowSize = 8;
        QPointF p1 = to - QPointF(sin(angle + M_PI / 3) * arrowSize, cos(angle + M_PI / 3) * arrowSize);
        QPointF p2 = to - QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize, cos(angle + M_PI - M_PI / 3) * arrowSize);
        QPolygonF head;
        head << to << p1 << p2;
        m_scene->addPolygon(head, pen, QBrush(QColor(71, 85, 105)));

        if (!label.isEmpty()) {
            auto* text = m_scene->addText(label);
            text->setDefaultTextColor(QColor(148, 163, 184));
            text->setFont(QFont("sans-serif", 8));
            text->setPos((from.x() + to.x()) / 2.0 - 15, (from.y() + to.y()) / 2.0 - 18);
        }
    };

    drawArrow(QPointF(72, 176), QPointF(110, 176));
    drawArrow(QPointF(270, 176), QPointF(320, 176));
    drawArrow(QPointF(400, 140), QPointF(530, 115), "[stock > 0]");
    drawArrow(QPointF(400, 210), QPointF(530, 255), "[sin stock]");
    drawArrow(QPointF(700, 115), QPointF(750, 115));
    drawArrow(QPointF(920, 115), QPointF(970, 160));
    drawArrow(QPointF(700, 255), QPointF(970, 185));
    drawArrow(QPointF(1140, 176), QPointF(1190, 176));

    // Definir secuencia de simulación con variables asociadas
    m_steps = {
        {
            "n_init", "Inicio", "Initial",
            QString::fromUtf8("Evento disparador recibido: Nueva orden de compra #8921 recibida en el bus de mensajes.").toStdString(),
            {{"idOrden", "8921"}, {"estado", "\"RECIBIDA\""}, {"montoTotal", "$4,850.00"}, {"intentos", "1"}}
        },
        {
            "n_recibir", "Recibir Solicitud", "Action",
            QString::fromUtf8("Decodificando payload JSON y verificando firma HMAC del cliente.").toStdString(),
            {{"idCliente", "CUST-3042"}, {"canal", "\"API_GATEWAY\""}, {"articulosCount", "6"}, {"estado", "\"EN_PROCESAMIENTO\""}}
        },
        {
            "n_validar", "Validar Stock y Crédito", "Decision",
            QString::fromUtf8("Evaluando reglas de negocio: stockDisponible >= 6 (True) && limiteCredito >= 4850.00 (True).").toStdString(),
            {{"stockDisponible", "42 unidades"}, {"limiteCredito", "$15,000.00"}, {"decision", "APROBADO"}}
        },
        {
            "n_despacho", "Preparar Despacho", "Action",
            QString::fromUtf8("Apartando 6 unidades en bodega central A-14. Notificando a sistema WMS.").toStdString(),
            {{"bodegaAsignada", "\"Bodega Central A14\""}, {"stockRestante", "36 unidades"}, {"wmsTicket", "\"WMS-9941\""}}
        },
        {
            "n_factura", "Emitir Factura", "Action",
            QString::fromUtf8("Generando comprobante fiscal CFDI / DDL timbrado por el PAC. UUID generado con éxito.").toStdString(),
            {{"uuidFiscal", "\"9F3B1A2C-E4D8-4A12-B671-80C51FA24C33\""}, {"impuestos", "$776.00"}, {"estado", "\"TIMBRADO_OK\""}}
        },
        {
            "n_notificar", "Enviar Correo", "Action",
            QString::fromUtf8("Enviando correo electrónico al cliente con PDF y guía de rastreo DHL #92837482.").toStdString(),
            {{"destinatario", "\"cliente@dominio.com\""}, {"proveedorEnvio", "\"DHL Express\""}, {"guiaRastreo", "\"DHL-92837482\""}}
        },
        {
            "n_end", "Fin", "Final",
            QString::fromUtf8("Flujo completado exitosamente. Estado de orden: DESPACHADA.").toStdString(),
            {{"tiempoTotal", "342 ms"}, {"estadoFinal", "\"COMPLETADO_EXITOSO\""}}
        }
    };

    // Variables iniciales
    updateVariablesTable({
        {"idOrden", "8921"},
        {"estado", "\"PENDIENTE\""},
        {"stockDisponible", "42"},
        {"limiteCredito", "$15,000.00"}
    });
}

void SimulationWidget::play() {
    if (m_steps.empty()) return;
    m_isRunning = true;
    m_btnPlay->setEnabled(false);
    m_btnPause->setEnabled(true);
    m_statusLabel->setText(QString::fromUtf8("⚡ Ejecutando..."));
    m_statusLabel->setStyleSheet("color: #10b981; font-weight: bold;");

    int interval = m_speedSlider->value();
    m_timer->start(interval);
}

void SimulationWidget::pause() {
    m_isRunning = false;
    m_timer->stop();
    m_btnPlay->setEnabled(true);
    m_btnPause->setEnabled(false);
    m_statusLabel->setText(QString::fromUtf8("⏸️ En Pausa"));
    m_statusLabel->setStyleSheet("color: #f59e0b; font-weight: bold;");
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
            logEvent("BREAKPOINT", QString::fromUtf8("🔴 Breakpoint alcanzado en: %1. Simulación pausada.").arg(QString::fromStdString(step.nodeName)));
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
    m_statusLabel->setStyleSheet("color: #f43f5e; font-weight: bold;");
}

void SimulationWidget::resetSimulation() {
    stop();
    m_currentStepIndex = -1;
    for (auto* item : m_nodeItems) {
        item->setActive(false);
    }
    m_statusLabel->setText(QString::fromUtf8("🔄 Reiniciado"));
    m_statusLabel->setStyleSheet("color: #94a3b8; font-weight: bold;");
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
    QString color = "#94a3b8";

    if (level == "SUCCESS") color = "#10b981";
    else if (level == "STEP") color = "#38bdf8";
    else if (level == "BREAKPOINT") color = "#f43f5e";
    else if (level == "WARN") color = "#f59e0b";

    m_logConsole->append(QString("<span style='color: #64748b;'>[%1]</span> <b style='color: %2;'>[%3]</b> %4")
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
                    valItem->setForeground(QBrush(QColor(56, 189, 248))); // Resaltar cambio
                }
                found = true;
                break;
            }
        }

        if (!found) {
            int newRow = m_varsTable->rowCount();
            m_varsTable->insertRow(newRow);

            auto* item0 = new QTableWidgetItem(QString::fromStdString(varName));
            item0->setForeground(QBrush(QColor(241, 245, 249)));

            QString inferredType = "String";
            if (varVal.find('$') != std::string::npos) inferredType = "Currency";
            else if (varVal == "True" || varVal == "False" || varVal == "APROBADO") inferredType = "Boolean";
            else if (varVal.find("ms") != std::string::npos || varVal.find("unidades") != std::string::npos) inferredType = "Quantity";
            else if (std::all_of(varVal.begin(), varVal.end(), ::isdigit)) inferredType = "Integer";

            auto* item1 = new QTableWidgetItem(inferredType);
            item1->setForeground(QBrush(QColor(148, 163, 184)));

            auto* item2 = new QTableWidgetItem(QString::fromStdString(varVal));
            item2->setForeground(QBrush(QColor(16, 185, 129)));

            m_varsTable->setItem(newRow, 0, item0);
            m_varsTable->setItem(newRow, 1, item1);
            m_varsTable->setItem(newRow, 2, item2);
        }
    }
}

void SimulationWidget::loadDiagramForSimulation(int /*diagramId*/) {
    // Permite enlazar cualquier diagrama del repositorio
    buildSimulationFlow();
}

} // namespace ea
