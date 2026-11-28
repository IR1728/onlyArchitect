#include "SqlConsoleWidget.hpp"
#include "DarkTheme.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QXmlStreamReader>

SqlConsoleWidget::SqlConsoleWidget(ea::Repository& repo, QWidget* parent)
    : QWidget(parent), m_repo(repo) {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    auto topLayout = new QHBoxLayout();
    m_queryEdit = new QTextEdit(this);
    m_queryEdit->setPlaceholderText("Enter SQL query (e.g. SELECT Diagram_ID, Name, Diagram_Type FROM t_diagram LIMIT 10)");
    m_queryEdit->setMaximumHeight(70);
    m_queryEdit->setFont(ea::gui::M3Typography::bodySmallMono());
    m_queryEdit->setText("SELECT Object_ID, Name, Object_Type, Status FROM t_object LIMIT 20;");
    topLayout->addWidget(m_queryEdit);

    m_execButton = new QPushButton("▶ Execute", this);
    ea::gui::setThemedStyleSheet(m_execButton, "font-weight: bold; background-color: @primaryStrong; color: white; padding: 10px 16px; border-radius: 4px;");
    topLayout->addWidget(m_execButton);
    mainLayout->addLayout(topLayout);

    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(m_resultsTable);

    m_statusLabel = new QLabel("Ready.", this);
    mainLayout->addWidget(m_statusLabel);

    connect(m_execButton, &QPushButton::clicked, this, &SqlConsoleWidget::onExecuteClicked);
}

void SqlConsoleWidget::onExecuteClicked() {
    if (!m_repo.IsOpen()) {
        m_statusLabel->setText("No hay ningún repositorio abierto.");
        return;
    }
    QString sql = m_queryEdit->toPlainText().trimmed();
    if (sql.isEmpty()) return;

    // Guard against accidental destructive SQL statements on important repositories
    static const QRegularExpression destructiveRe(
        QStringLiteral(R"(\b(DROP|DELETE|UPDATE|ALTER|TRUNCATE|INSERT|REPLACE|ATTACH|DETACH|VACUUM)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    if (destructiveRe.match(sql).hasMatch()) {
        const auto reply = QMessageBox::warning(
            this, "Confirmar sentencia SQL que modifica datos",
            "Esta consulta SQL puede modificar o eliminar datos permanentes de tu repositorio .qea.\n\n"
            "¿Seguro que deseas ejecutarla?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            m_statusLabel->setText("Ejecución cancelada por seguridad.");
            return;
        }
    }

    std::string xmlStr = m_repo.SQLQuery(sql.toStdString());
    QByteArray xmlData(xmlStr.c_str(), static_cast<int>(xmlStr.size()));

    QXmlStreamReader xml(xmlData);

    QStringList headers;
    QList<QStringList> rows;
    QStringList currentRow;
    QString currentField;

    bool inRow = false;
    while (!xml.atEnd() && !xml.hasError()) {
        auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            if (name == "Row") {
                inRow = true;
                currentRow.clear();
            } else if (inRow) {
                currentField = name;
                if (!headers.contains(name) && rows.isEmpty()) {
                    headers.append(name);
                }
            }
        } else if (token == QXmlStreamReader::Characters && inRow) {
            if (!xml.isWhitespace()) {
                currentRow.append(xml.text().toString());
            }
        } else if (token == QXmlStreamReader::EndElement) {
            QString name = xml.name().toString();
            if (name == "Row") {
                inRow = false;
                rows.append(currentRow);
            }
        }
    }

    m_resultsTable->clear();
    m_resultsTable->setColumnCount(headers.size());
    m_resultsTable->setHorizontalHeaderLabels(headers);
    m_resultsTable->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < rows[r].size() && c < headers.size(); ++c) {
            m_resultsTable->setItem(r, c, new QTableWidgetItem(rows[r][c]));
        }
    }

    m_statusLabel->setText(QString("Query executed: %1 rows returned.").arg(rows.size()));
}
