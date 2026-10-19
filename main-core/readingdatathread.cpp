#include "readingdatathread.h"
#include "shmcommunicator.h"
#include <QDebug>
#include <string.h>

readingDataThread::readingDataThread(ShmCommunicator *shm, QObject *parent)
    : QThread(parent)
    , m_isRunning(false)
    , m_shm(shm)
{
}

readingDataThread::~readingDataThread()
{
    stop();
    quit();
    wait(2000);
}

void readingDataThread::run()
{
    if (!m_shm || !m_shm->isInitialized()) {
        emit commError(QStringLiteral("共享内存未初始化，无法启动数据读取"));
        return;
    }

    m_isRunning = true;

    QTimer processTimer;
    processTimer.setInterval(500);  // 每 500ms 轮询一次共享内存
    processTimer.start();

    connect(&processTimer, &QTimer::timeout, this, [this]() {
        if (!m_isRunning) return;

        // 预分配缓冲区，sizeof(SensorData) = 108 字节（pack(1)）
        uint8_t buf[256];
        uint32_t len = 0;

        if (m_shm->recvFromSecondary(buf, &len)) {
            // 验证数据长度是否匹配结构体大小
            if (len != sizeof(SensorData)) {
                emit commError(QStringLiteral(
                    "收到的数据结构大小不匹配: 期望 %1 字节，实际 %2 字节")
                    .arg(sizeof(SensorData)).arg(len));
                return;
            }

            // 将原始字节复制到结构体
            SensorData data;
            memcpy(&data, buf, sizeof(SensorData));

            emit dataReady(data);
        }
    });

    exec();  // 进入事件循环
}

void readingDataThread::stop()
{
    m_isRunning = false;
}
