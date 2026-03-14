//
// Created by admin on 2026/3/2.
//

// You may need to build the project (run Qt uic code generator) to get "ui_MainWindow.h" resolved

#include "ui_MainWindow.h"
#include "Mainwindow.h"
#include <QPushButton>
#include <QIntValidator>
#include <QMessageBox>
#include <QHostAddress>
#include <QMenu>
#include <QScreen>

#include "Connector.h"

MainWindow::MainWindow(QWidget *parent) : QWidget(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setWindowFlags(Qt::WindowMinMaxButtonsHint);

    auto *validator = new QIntValidator(0, 20, this);
    auto *portValidator = new QIntValidator(1024, 65535, this);
    ui->lineEdit_port->setValidator(portValidator);
    ui->lineEdit_num->setValidator(validator);

    ui->lineEdit_ip->setText("192.168.56.103");
    ui->lineEdit_port->setText("8192");
    ui->lineEdit_num->setText("1");

    ui->comboBox_proto->clear();
    ui->comboBox_proto->addItem("tcp", static_cast<int>(ConnectorType::TCP));
    ui->comboBox_proto->addItem("websocket", static_cast<int>(ConnectorType::WEBSOCKET));
    ui->comboBox_proto->setCurrentIndex(1);

    setAttribute(Qt::WA_DeleteOnClose);

    initTrayIcon();
}

MainWindow::~MainWindow() {
    for (auto &conn: connections_) {
        conn->close();
        // delete conn;
    }

    delete ui;

    delete trayIcon;
}

void MainWindow::buttonClicked(QAbstractButton *button) {
    if (button == ui->buttonBox->button(QDialogButtonBox::Close)) {
        close();
    } else if (button == ui->buttonBox->button(QDialogButtonBox::Ok)) {
        createConnections();
    }
}

void MainWindow::trayIconActive(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
            if (connections_.isEmpty()) {
                showNormal();
                activateWindow();
            } else {
                for (auto &conn: connections_) {
                    conn->show();
                }
            }

            break;
        default: ;
    }
}

bool MainWindow::paramsValidator(MainWindowForm *form) {
    bool ok;
    auto port = ui->lineEdit_port->text().trimmed().toUInt(&ok);
    if (!ok) {
        QMessageBox::critical(this, tr("错误"), tr("端口号 %1 不是整数").arg(port));
        return ok;
    }

    auto num = ui->lineEdit_num->text().trimmed().toUInt(&ok);
    if (!ok) {
        QMessageBox::critical(this, tr("错误"), tr("开启窗口 %1 不是整数").arg(num));
        return ok;
    }

    auto ip = ui->lineEdit_ip->text().trimmed();
    QHostAddress host;
    ok = host.setAddress(ip);
    if (!ok) {
        QMessageBox::critical(this, tr("错误"), tr("%1 不是正确的ip").arg(ip));
        return ok;
    }
    form->ip = ip;
    form->port = port;
    form->num = num;
    form->connectorType = static_cast<ConnectorType>(ui->comboBox_proto->currentData().toInt());
    return true;
}

void MainWindow::createConnections() {
    MainWindowForm form;
    bool ok = paramsValidator(&form);
    if (!ok) return;

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    for (uint i = 0; i < form.num; i++) {
        constexpr int offset = 20;
        auto *connector = new Connector(nullptr, form.ip, form.port, form.connectorType);
        connections_.insert(connector);

        connect(connector, &Connector::windowClosed, this, [this, connector]() {
            connections_.remove(connector);
            connector->deleteLater();
        });
        int centerX = screenGeometry.x() +
                  (screenGeometry.width() - connector->width()) / 2;

        int centerY = screenGeometry.y() +
                      (screenGeometry.height() - connector->height()) / 2;

        int x = centerX + i * offset;
        int y = centerY - i * offset;
        connector->move(x, y);
        connector->show();
        connector->raise();
    }

    showMinimized();
}

void MainWindow::initTrayIcon() {
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/Filosofem.png"));

    trayIcon->setToolTip(windowTitle());
    trayIcon->show();

    auto *quitAction = new QAction("退出", this);
    auto *restoreAction = new QAction("最大化", this);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);
    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);

    auto *menu = new QMenu(this);
    menu->addAction(restoreAction);
    menu->addSeparator();
    menu->addAction(quitAction);
    trayIcon->setContextMenu(menu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActive);
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowStateChange && windowState() == Qt::WindowMinimized) {
        trayIcon->showMessage("托盘", "dsadas");
    }
}
