//
// Created by 115282 on 2023/8/21.
//

#ifndef QT_PROJECT__MYFORM_H_
#define QT_PROJECT__MYFORM_H_

#include <QAbstractButton>
#include <QAbstractSocket>
QT_BEGIN_NAMESPACE

namespace Ui
{
    class Connector;
}

QT_END_NAMESPACE

class QTcpSocket;
class QWebSocket;
enum class ConnectorType {
    TCP,
    WEBSOCKET,
};
class Connector final : public QWidget
{
    Q_OBJECT

public:
    Connector(QWidget* parent, const QString &ip, int port, const QString &path, ConnectorType type);
    ~Connector() override;

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void windowClosed();

private slots:
    void clickHandler();

    void onMessageReceived();
    void onError(QAbstractSocket::SocketError);
    void onConnected();
    void onMessageReceivedByWebSocket(const QString &);
    void onDisconnected();

private:
    void initSocket();
    void initTcpSocket();
    void initWebSocket();



    Ui::Connector* ui;

    union {
        QTcpSocket *tcpSocket_;
        QWebSocket *webSocket_;
    } socket_{};
    QString ip_;
    int port_;
    QString path_;
    QString title_;
    ConnectorType type_;
    QString protocol_;

    static constexpr char HEARTBEAT[] = "HEARTBEAT";

    // constexpr static auto APP_TITLE = "windowsLockScreen By burzum";
};

#endif //QT_PROJECT__MYFORM_H_
