//
// Created by Kantu004 on 2024/11/11.
//

#ifndef WINDOWLOCKSCREENFORM__SINGLEAPPLICATION_H_
#define WINDOWLOCKSCREENFORM__SINGLEAPPLICATION_H_

#include <QApplication>
#include <QtNetwork/QLocalServer>
#include <QLockFile>
#include <QtNetwork/QLocalSocket>
#include <QMessageBox>
#include <utility>

class SingleApplication : public QApplication
{
Q_OBJECT
public:
    enum class SINGLE_MODE
    {
        BY_LOCK_FILE,  // 使用文件锁，文件锁不支持前台激活widget，只可判断是否存在当前程序的进程
        BY_LOCAL_SOCKET  // 使用本地socket通信
    };

    SingleApplication(int &argc, char **argv, QString appName, SINGLE_MODE mode = SINGLE_MODE::BY_LOCAL_SOCKET, int f = ApplicationFlags)
        : QApplication(argc, argv, f), mode_(mode), NAME(std::move(appName)) {
        if (mode_ == SINGLE_MODE::BY_LOCAL_SOCKET) initLocalServer();
        else initLockFile();
    }
    ~SingleApplication() noexcept override {
        if (mode_ == SINGLE_MODE::BY_LOCAL_SOCKET) {
            // 如果有localServer，则当前实例关闭后就不存在程序实例了，关闭server
            if (m_.localServer_) m_.localServer_->close();
            delete m_.localServer_;
        } else {
            if (m_.lockFile_->isLocked()) m_.lockFile_->unlock();
            delete m_.lockFile_;
        }
    }

    inline void setWidget(QWidget *widget);
    /*
     * 返回当前是否有实例在运行，如果有，返回true，没有则返回false
     * */
    [[nodiscard]] inline bool isRunning() const { return isRunning_; }

private Q_SLOT:
    inline void newLocalConnect() const;

private:
    inline void initLocalServer();
    inline void initLockFile();
    inline void newLocalServer();

private:
    bool isRunning_ = false;
    SINGLE_MODE mode_;
    QWidget *w_ {};

    union
    {
        QLocalServer *localServer_;
        QLockFile *lockFile_;
    } m_ {};

    QString NAME;
};

void SingleApplication::initLocalServer() {
    QLocalSocket socket;
    socket.connectToServer(NAME, QIODevice::ReadOnly);
    if (socket.waitForConnected(100)) {
        // 有实例已经创建了socketserver
        isRunning_ = true;
        return;
    }

    newLocalServer();
}

void SingleApplication::initLockFile() {
    m_.lockFile_ = new QLockFile { NAME };
    if (m_.lockFile_->tryLock()) isRunning_ = false;
    else isRunning_ = true;
}

void SingleApplication::setWidget(QWidget *widget) {
    w_ = widget;
    if (!isRunning_) w_->show();
}

void SingleApplication::newLocalServer() {
    m_.localServer_ = new QLocalServer;
    connect(m_.localServer_, &QLocalServer::newConnection, this, &SingleApplication::newLocalConnect);
    if (!m_.localServer_->listen(NAME)) {
        // 此时监听失败,可能是程序崩溃时,残留进程服务导致的,移除,重新监听
        if (m_.localServer_->serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(NAME);
            m_.localServer_->listen(NAME);
            return;
        }

        QMessageBox::critical(nullptr, tr("错误"), tr("本地socket服务创建错误，请联系管理员"));
    }
}

void SingleApplication::newLocalConnect() const {
    if (auto *socket = m_.localServer_->nextPendingConnection()) {
        delete socket;

        if (w_) {
            w_->raise();
            w_->showNormal();
            w_->activateWindow();
        }
    }
}
#endif //WINDOWLOCKSCREENFORM__SINGLEAPPLICATION_H_
