#ifndef READINGDATATHREAD_H
#define READINGDATATHREAD_H

#include <QThread>
#include <QTimer>
#include "sensordata.h"

class ShmCommunicator;

class readingDataThread : public QThread
{
    Q_OBJECT
public:
    explicit readingDataThread(ShmCommunicator *shm, QObject *parent = nullptr);
    ~readingDataThread();

signals:
    // 接收到副核解析完成的传感器数据
    void dataReady(const SensorData &data);
    // 通信错误
    void commError(const QString &errorMsg);

public slots:
    void stop();

protected:
    void run() override;

private:
    bool m_isRunning;
    ShmCommunicator *m_shm;
};

#endif // READINGDATATHREAD_H
