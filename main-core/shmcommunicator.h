#ifndef SHMCOMMUNICATOR_H
#define SHMCOMMUNICATOR_H

#include <QObject>
#include <stdint.h>

// 共享内存布局（与副核 commMsg.h 一致）
#define SHM_BASE         0xb1004000
#define SHM_SIZE         0x4000
#define CH_A2B_OFFSET    0x0000   // 副核→主核 (Qt 从此通道读取)
#define CH_B2A_OFFSET    0x2000   // 主核→副核 (Qt 向此通道写入)
#define CHANNEL_SIZE     0x2000
#define DATA_AREA_SIZE   (CHANNEL_SIZE - 8)

// 环形缓冲区结构体（必须与副核 ringBuf.h 完全一致）
#pragma pack(push, 1)
struct RingBuffer
{
    volatile uint32_t write_ptr;
    volatile uint32_t read_ptr;
    uint8_t data[DATA_AREA_SIZE];
};
#pragma pack(pop)

class ShmCommunicator : public QObject
{
    Q_OBJECT
public:
    explicit ShmCommunicator(QObject *parent = nullptr);
    ~ShmCommunicator();

    bool init();
    bool isInitialized() const { return m_initialized; }

    // 向副核发送数据（写入 B→A 通道）
    bool sendToSecondary(const uint8_t *msg, uint32_t len);

    // 从副核接收数据（读取 A→B 通道）
    // 返回 true 表示有新数据，buf 需预分配，len 返回实际长度
    bool recvFromSecondary(uint8_t *buf, uint32_t *len);

    // 获取通信状态信息
    QString statusInfo() const;

private:
    bool m_initialized;
    int m_fd;
    void *m_base;
    RingBuffer *m_rx_rb;  // 读取通道 (A→B, 副核写/主核读)
    RingBuffer *m_tx_rb;  // 发送通道 (B→A, 主核写/副核读)

    bool ringbufEmpty(const RingBuffer *rb) const;
    bool ringbufFull(const RingBuffer *rb) const;
};

#endif // SHMCOMMUNICATOR_H
