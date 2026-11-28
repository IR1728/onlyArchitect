#include "ServerConnectionDialog.hpp"
#include "DarkTheme.hpp"
#include "M3Icons.hpp"
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QSettings>
#include <QVBoxLayout>

namespace ea::gui {

QString ServerConnectionDialog::detectedLocalIps() {
    QStringList ips;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                ips << QString("%1 (%2)").arg(addr.toString(), iface.humanReadableName());
            }
        }
    }
    return ips.isEmpty() ? QStringLiteral("127.0.0.1 (solo local)") : ips.join("  ·  ");
}

ServerConnectionDialog::ServerConnectionDialog(const QString& currentFilePath,
                                               bool isHosting,
                                               bool isConnected,
                                               quint16 activePort,
                                               const QStringList& peers,
                                               QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Colaboración en Red Local / VPN (Team Server)");
    resize(540, 340);

    QSettings settings;
    const QString savedHost = settings.value("teamSync/lastHost", "127.0.0.1").toString();
    const int savedPort = settings.value("teamSync/lastPort", 7777).toInt();
    const QString savedUser = settings.value("teamSync/username", qEnvironmentVariable("USER", "Arquitecto")).toString();

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto* statusBanner = new QLabel(this);
    statusBanner->setWordWrap(true);
    if (isHosting || isConnected) {
        statusBanner->setText(QString("<b>Estado:</b> Conectado (%1) · <b>En línea (%2):</b> %3")
                                  .arg(isHosting ? QString("Hospedando en puerto %1").arg(activePort) : QStringLiteral("Cliente"))
                                  .arg(peers.size())
                                  .arg(peers.isEmpty() ? QStringLiteral("-") : peers.join(", ")));
        setThemedStyleSheet(statusBanner, "background-color: @surfaceContainer; color: @success; padding: 8px; border-radius: 6px;");
    } else {
        statusBanner->setText("<b>Sin conexión de equipo.</b> Conéctate por IP local / VPN (Tailscale, ZeroTier, LAN) o hospeda tu proyecto .qea actual.");
        setThemedStyleSheet(statusBanner, "background-color: @surfaceLow; color: @onSurfaceVariant; padding: 8px; border-radius: 6px;");
    }
    root->addWidget(statusBanner);

    m_tabs = new QTabWidget(this);

    // Tab 1: Conectar como Cliente
    auto* clientPage = new QWidget(this);
    auto* clientForm = new QFormLayout(clientPage);
    clientForm->setSpacing(10);

    m_hostEdit = new QLineEdit(savedHost, clientPage);
    m_hostEdit->setPlaceholderText("Ej. 192.168.1.50 o 100.119.92.93");
    m_clientPortSpin = new QSpinBox(clientPage);
    m_clientPortSpin->setRange(1024, 65535);
    m_clientPortSpin->setValue(savedPort);
    m_userEdit = new QLineEdit(savedUser, clientPage);
    m_clientTokenEdit = new QLineEdit(clientPage);
    m_clientTokenEdit->setEchoMode(QLineEdit::Password);
    m_clientTokenEdit->setPlaceholderText("Opcional (si el anfitrión configuró PIN)");

    clientForm->addRow("IP / Host del Servidor:", m_hostEdit);
    clientForm->addRow("Puerto TCP:", m_clientPortSpin);
    clientForm->addRow("Tu Nombre:", m_userEdit);
    clientForm->addRow("PIN / Clave de Sala:", m_clientTokenEdit);

    auto* btnConnect = new QPushButton("Conectar al Equipo", clientPage);
    M3Icons::decorate(btnConnect, "cloud");
    setThemedStyleSheet(btnConnect, "QPushButton { background-color: @primaryStrong; color: white; font-weight: bold; padding: 7px 16px; border-radius: 6px; }");
    connect(btnConnect, &QPushButton::clicked, this, [this]() {
        QSettings s;
        s.setValue("teamSync/lastHost", m_hostEdit->text().trimmed());
        s.setValue("teamSync/lastPort", m_clientPortSpin->value());
        s.setValue("teamSync/username", m_userEdit->text().trimmed());
        m_action = Action::ConnectClient;
        accept();
    });
    clientForm->addRow("", btnConnect);
    m_tabs->addTab(clientPage, "Conectar a Servidor");

    // Tab 2: Hospedar Servidor LAN/VPN
    auto* hostPage = new QWidget(this);
    auto* hostForm = new QFormLayout(hostPage);
    hostForm->setSpacing(10);

    auto* fileRow = new QHBoxLayout();
    m_hostFileEdit = new QLineEdit(currentFilePath, hostPage);
    m_hostFileEdit->setPlaceholderText("Selecciona el archivo maestro .qea...");
    auto* browseBtn = new QPushButton("Examinar...", hostPage);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString p = QFileDialog::getOpenFileName(this, "Seleccionar Repositorio Maestro", m_hostFileEdit->text(), "Repositorios SQLite (*.qea)");
        if (!p.isEmpty()) m_hostFileEdit->setText(p);
    });
    fileRow->addWidget(m_hostFileEdit, 1);
    fileRow->addWidget(browseBtn);

    m_hostPortSpin = new QSpinBox(hostPage);
    m_hostPortSpin->setRange(1024, 65535);
    m_hostPortSpin->setValue(savedPort);

    m_hostUserEdit = new QLineEdit(savedUser, hostPage);
    m_hostTokenEdit = new QLineEdit(hostPage);
    m_hostTokenEdit->setEchoMode(QLineEdit::Password);
    m_hostTokenEdit->setPlaceholderText("Opcional — protege tu sala en Wi-Fi compartido");

    auto* ipsLabel = new QLabel(detectedLocalIps(), hostPage);
    ipsLabel->setWordWrap(true);
    ipsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    setThemedStyleSheet(ipsLabel, "color: @primary; font-family: monospace; font-size: 11px;");

    hostForm->addRow("Repositorio (.qea):", fileRow);
    hostForm->addRow("Puerto TCP:", m_hostPortSpin);
    hostForm->addRow("Tu Nombre:", m_hostUserEdit);
    hostForm->addRow("PIN / Clave de Sala:", m_hostTokenEdit);
    hostForm->addRow("Tus IPs LAN / VPN:", ipsLabel);

    auto* btnHost = new QPushButton("Iniciar Servidor y Conectar", hostPage);
    M3Icons::decorate(btnHost, "dns");
    setThemedStyleSheet(btnHost, "QPushButton { background-color: @success; color: white; font-weight: bold; padding: 7px 16px; border-radius: 6px; }");
    connect(btnHost, &QPushButton::clicked, this, [this]() {
        QSettings s;
        s.setValue("teamSync/lastPort", m_hostPortSpin->value());
        s.setValue("teamSync/username", m_hostUserEdit->text().trimmed());
        m_userEdit->setText(m_hostUserEdit->text().trimmed());
        m_action = Action::HostServer;
        accept();
    });
    hostForm->addRow("", btnHost);
    m_tabs->addTab(hostPage, "Hospedar Proyecto (LAN/VPN)");

    root->addWidget(m_tabs, 1);

    auto* bottomRow = new QHBoxLayout();
    if (isHosting || isConnected) {
        auto* btnDisc = new QPushButton("Desconectar / Detener", this);
        setThemedStyleSheet(btnDisc, "color: @error;");
        connect(btnDisc, &QPushButton::clicked, this, [this]() {
            m_action = Action::Disconnect;
            accept();
        });
        bottomRow->addWidget(btnDisc);
    }
    bottomRow->addStretch();
    auto* btnClose = new QPushButton("Cerrar", this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    bottomRow->addWidget(btnClose);
    root->addLayout(bottomRow);
}

quint16 ServerConnectionDialog::port() const {
    if (m_action == Action::HostServer && m_hostPortSpin) {
        return static_cast<quint16>(m_hostPortSpin->value());
    }
    return m_clientPortSpin ? static_cast<quint16>(m_clientPortSpin->value()) : 7777;
}

QString ServerConnectionDialog::roomToken() const {
    if (m_action == Action::HostServer && m_hostTokenEdit) {
        return m_hostTokenEdit->text().trimmed();
    }
    return m_clientTokenEdit ? m_clientTokenEdit->text().trimmed() : QString();
}

} // namespace ea::gui
