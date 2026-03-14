//
// Created by 115282 on 2023/8/21.
//

// You may need to build the project (run Qt uic code generator) to get "ui_myform.h" resolved

#include "Connector.h"

#include "ui_Connector.h"
#include <QMenu>
#include <QMessageBox>
#include <utility>
#include <QTcpSocket>
#include <QDateTime>

#include <QDebug>

Connector::Connector(QWidget *parent, QString ip, int port, ConnectorType type)
    : QWidget(parent), ui(new Ui::Connector), ip_(std::move(ip)), port_(port), type_(type) {
    ui->setupUi(this);

    setWindowFlag(Qt::Window);
    title_ = tr("%1:%2").arg(ip_).arg(port_);

    switch (type_) {
        case ConnectorType::TCP:
            protocol_ = "tcp";
            initTcpSocket();
            break;
        case ConnectorType::WEBSOCKET:
            protocol_ = "websocket";
            initWebSocket();
            break;
    }
}

Connector::~Connector() {
    delete ui;
    if (type_ == ConnectorType::TCP) {
        delete socket_.tcpSocket_;
    } else {
        delete socket_.webSocket_;
    }
}

void Connector::closeEvent(QCloseEvent *event) {
    emit windowClosed();
    QWidget::closeEvent(event);
}

void Connector::clickHandler() {
    auto msg = ui->data->text().trimmed();
    if (msg.isEmpty()) return;

    qint64 bytes = 0;
    switch (type_) {
        case ConnectorType::TCP:
            bytes = socket_.tcpSocket_->write(msg.toUtf8());
            break;
        case ConnectorType::WEBSOCKET:
            bytes = socket_.webSocket_->sendTextMessage(msg.toUtf8());
            break;
    }
    if (bytes == -1) {
        QString err;
        switch (type_) {
            case ConnectorType::TCP:
                err = socket_.tcpSocket_->errorString();
                break;
            case ConnectorType::WEBSOCKET:
                err = socket_.webSocket_->errorString();
                break;
        }
        auto box = new QMessageBox(QMessageBox::Critical,
                                   tr("提示"),
                                   tr("%1发送消息错误: %2")
                                   .arg(protocol_)
                                   .arg(err),
                                   QMessageBox::Ok,
                                   this);

        box->setAttribute(Qt::WA_DeleteOnClose);
        box->show();
    }

    ui->data->clear();
}

void Connector::initSocket() {
    switch (type_) {
        case ConnectorType::TCP:
            initTcpSocket();
            break;
        case ConnectorType::WEBSOCKET:
            initWebSocket();
            break;
    }
}

void Connector::initTcpSocket() {
    socket_.tcpSocket_ = new QTcpSocket(this);

    // 连接成功
    connect(socket_.tcpSocket_, &QTcpSocket::connected, this, &Connector::onConnected);
    // 收到数据
    connect(socket_.tcpSocket_, &QTcpSocket::readyRead, this, &Connector::onMessageReceived);
    // 错误
    connect(socket_.tcpSocket_, &QTcpSocket::errorOccurred, this, &Connector::onError);
    // 关闭
    connect(socket_.tcpSocket_, &QTcpSocket::disconnected, this, &Connector::onDisconnected);
    socket_.tcpSocket_->connectToHost(ip_, port_);
}

void Connector::initWebSocket() {
    socket_.webSocket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    QUrl url;
    url.setScheme("ws");
    url.setHost(ip_);
    url.setPort(port_);

    connect(socket_.webSocket_, &QWebSocket::connected, this, &Connector::onConnected);
    connect(socket_.webSocket_, &QWebSocket::textMessageReceived, this, &Connector::onMessageReceivedByWebSocket);
    connect(socket_.webSocket_, &QWebSocket::errorOccurred, this, &Connector::onError);
    connect(socket_.webSocket_, &QWebSocket::disconnected, this, &Connector::onDisconnected);

    socket_.webSocket_->open(url);
}

void Connector::onMessageReceived() {
    QByteArray data = socket_.tcpSocket_->readAll();
    if (data == HEARTBEAT) {
        qDebug() << "收到心跳检测";

        socket_.tcpSocket_->write(HEARTBEAT);
        return;
    }
    ui->plainTextEdit->appendPlainText(data);
}

void Connector::onError(QAbstractSocket::SocketError) {
    QString err;
    switch (type_) {
        case ConnectorType::TCP:
            err = socket_.tcpSocket_->errorString();
            break;
        case ConnectorType::WEBSOCKET:
            err = socket_.webSocket_->errorString();
            break;
    }
    setWindowTitle("连接失败");
    ui->label_title->setText("连接失败");
    auto box = new QMessageBox(QMessageBox::Critical,
                               tr("提示"),
                               tr("%1连接发生错误: %2")
                               .arg(protocol_)
                               .arg(err),
                               QMessageBox::Ok,
                               this);
    connect(box, &QMessageBox::accepted, this, &QWidget::close);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->show();
}

void Connector::onConnected() {
    QString timeStr = QDateTime::currentDateTime()
            .toString("yyyy-MM-dd HH:mm:ss");
    ui->plainTextEdit->appendPlainText(tr("[%1] %2:%3连接成功").arg(timeStr, protocol_, title_));

    setWindowTitle(title_);
    ui->label_title->setText(tr("连接成功 %1").arg(title_));
}

void Connector::onMessageReceivedByWebSocket(const QString &msg) {
    if (msg == HEARTBEAT) {
        qDebug() << "收到心跳检测";

        socket_.webSocket_->sendTextMessage(HEARTBEAT);
        return;
    }
    ui->plainTextEdit->appendPlainText(msg);
}

void Connector::onDisconnected() {
    setWindowTitle("连接关闭");
    ui->label_title->setText("连接关闭");
    auto box = new QMessageBox(QMessageBox::Information,
                               tr("提示"),
                               tr("websocket关闭"),
                               QMessageBox::Ok,
                               this);
    connect(box, &QMessageBox::accepted, this, &Connector::close);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->show();
}
