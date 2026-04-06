//
// Created by admin on 2026/4/4.
//
#ifndef TCPCONNECTIONSBYVARIAN_EVENTBUS_H
#define TCPCONNECTIONSBYVARIAN_EVENTBUS_H

#include <QObject>

class EventBus : public QObject {
    Q_OBJECT

public:
    static EventBus &instance() {
        static EventBus instance;
        return instance;
    }

signals:
    void closeAllErrBox();

private:
    EventBus() = default;
};
#endif //TCPCONNECTIONSBYVARIAN_EVENTBUS_H
