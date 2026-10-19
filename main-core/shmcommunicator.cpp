#include "shmcommunicator.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <QString>

ShmCommunicator::ShmCommunicator(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_fd(-1)
    , m_base(nullptr)
    , m_rx_rb(nullptr)
    , m_tx_rb(nullptr)
{
}

ShmCommunicator::~ShmCommunicator()
{
    if (m_base && m_base != MAP_FAILED) {
        munmap(m_base, SHM_SIZE);
    }
    if (m_fd >= 0) {
        close(m_fd);
    }
}

bool ShmCommunicator::init()
{
    m_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (m_fd < 0) {
        perror("ShmCommunicator: open /dev/mem");
        return false;
    }

    m_base = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SHARED, m_fd, SHM_BASE);
    if (m_base == MAP_FAILED) {
        perror("ShmCommunicator: mmap");
        close(m_fd);
        m_fd = -1;
        return false;
    }

    // A→B 通道 (offset 0x0000): 副核写，主核读
    m_rx_rb = reinterpret_cast<RingBuffer *>(
        static_cast<char *>(m_base) + CH_A2B_OFFSET);
    // B→A 通道 (offset 0x2000): 主核写，副核读
    m_tx_rb = reinterpret_cast<RingBuffer *>(
        static_cast<char *>(m_base) + CH_B2A_OFFSET);

    m_initialized = true;
    return true;
}

bool ShmCommunicator::ringbufEmpty(const RingBuffer *rb) const
{
    return rb->write_ptr == rb->read_ptr;
}

bool ShmCommunicator::ringbufFull(const RingBuffer *rb) const
{
    return ((rb->write_ptr + 1) % DATA_AREA_SIZE) == rb->read_ptr;
}

bool ShmCommunicator::sendToSecondary(const uint8_t *msg, uint32_t len)
{
    if (!m_initialized || !msg) return false;
    // 总长=2+len，缓冲区最多容纳 D-1 字节
    if (len > DATA_AREA_SIZE - 3) return false;
    if (ringbufFull(m_tx_rb)) return false;

    uint32_t w = m_tx_rb->write_ptr;

    // 写入 2 字节长度头（小端）
    m_tx_rb->data[w] = static_cast<uint8_t>(len & 0xFF);
    m_tx_rb->data[(w + 1) % DATA_AREA_SIZE] =
        static_cast<uint8_t>((len >> 8) & 0xFF);

    // 写入数据
    for (uint32_t i = 0; i < len; i++) {
        m_tx_rb->data[(w + 2 + i) % DATA_AREA_SIZE] = msg[i];
    }

    // 更新写指针（内存屏障确保数据写入后再更新指针）
    uint32_t new_w = (w + 2 + len) % DATA_AREA_SIZE;
    __sync_synchronize();
    m_tx_rb->write_ptr = new_w;
    return true;
}

bool ShmCommunicator::recvFromSecondary(uint8_t *buf, uint32_t *len)
{
    if (!m_initialized || !buf || !len) return false;
    if (ringbufEmpty(m_rx_rb)) return false;

    uint32_t r = m_rx_rb->read_ptr;

    // 读取 2 字节长度头（小端）
    uint16_t msg_len = m_rx_rb->data[r]
        | (static_cast<uint16_t>(m_rx_rb->data[(r + 1) % DATA_AREA_SIZE]) << 8);

    // 读取数据
    for (uint16_t i = 0; i < msg_len; i++) {
        buf[i] = m_rx_rb->data[(r + 2 + i) % DATA_AREA_SIZE];
    }

    // 更新读指针（内存屏障确保数据读取完成后再更新指针）
    uint32_t new_r = (r + 2 + msg_len) % DATA_AREA_SIZE;
    __sync_synchronize();
    m_rx_rb->read_ptr = new_r;

    *len = msg_len;
    return true;
}

QString ShmCommunicator::statusInfo() const
{
    if (!m_initialized) {
        return QStringLiteral("共享内存: 未初始化\n/dev/mem 映射失败，请检查设备节点和权限");
    }

    QString info;
    info += QStringLiteral("共享内存基址: 0x%1\n")
        .arg(SHM_BASE, 8, 16, QChar('0'));
    info += QStringLiteral("映射大小: %1 KB\n").arg(SHM_SIZE / 1024);

    uint32_t rx_avail = 0;
    if (m_rx_rb) {
        uint32_t w = m_rx_rb->write_ptr;
        uint32_t r = m_rx_rb->read_ptr;
        rx_avail = (w >= r) ? (w - r) : (DATA_AREA_SIZE - r + w);
    }
    info += QStringLiteral("A→B 通道 (副核→主核) 可读字节: %1\n").arg(rx_avail);

    uint32_t tx_space = 0;
    if (m_tx_rb) {
        uint32_t w = m_tx_rb->write_ptr;
        uint32_t r = m_tx_rb->read_ptr;
        uint32_t used = (w >= r) ? (w - r) : (DATA_AREA_SIZE - r + w);
        tx_space = DATA_AREA_SIZE - 1 - used;
    }
    info += QStringLiteral("B→A 通道 (主核→副核) 可用空间: %1 字节\n").arg(tx_space);

    return info;
}
