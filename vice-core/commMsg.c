#include "commMsg.h"

// 静态指针（仅本文件可见）
static RingBuffer *tx_rb = NULL;   // 发送通道（副核写）
static RingBuffer *rx_rb = NULL;   // 接收通道（副核读）

// 初始化共享内存映射
int init_shm() 
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return -1; }

    void *base = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, SHM_BASE);
    close(fd);
    if (base == MAP_FAILED) { perror("mmap"); return -1; }

    tx_rb = (RingBuffer *)((char *)base + CH_A2B_OFFSET);
    rx_rb = (RingBuffer *)((char *)base + CH_B2A_OFFSET);
    return 0;
}

// 向主核发送一条消息
// 返回值：0 成功， -1 缓冲区满
int send_to_root(const uint8_t *msg, uint32_t len) 
{
    if (len > DATA_AREA_SIZE - 3) return -1; // 消息太长（总长=2+len，缓冲区最多容纳D-1字节）

    if (ringbuf_full(tx_rb)) return -1;       // 缓冲区满

    uint32_t w = tx_rb->write_ptr;
    // 写入长度（2 字节）
    tx_rb->data[w] = (uint8_t)(len & 0xFF);
    tx_rb->data[(w + 1) % DATA_AREA_SIZE] = (uint8_t)((len >> 8) & 0xFF);
    // 写入数据
    for (uint32_t i = 0; i < len; i++) {
        tx_rb->data[(w + 2 + i) % DATA_AREA_SIZE] = msg[i];
    }
    // 更新写指针
    uint32_t new_w = (w + 2 + len) % DATA_AREA_SIZE;
    __sync_synchronize();  // 内存屏障
    tx_rb->write_ptr = new_w;
    return 0;
}

// 从主核接收一条消息
// 缓冲区：副核提供的预分配内存，len 返回实际长度
// 返回值：0 成功， -1 无数据
int recv_from_root(uint8_t *buf, uint32_t *len) 
{
    if (ringbuf_empty(rx_rb)) return -1;

    uint32_t r = rx_rb->read_ptr;
    // 读取长度
    uint16_t msg_len = rx_rb->data[r] | (rx_rb->data[(r + 1) % DATA_AREA_SIZE] << 8);
    // 读取数据
    for (uint16_t i = 0; i < msg_len; i++) {
        buf[i] = rx_rb->data[(r + 2 + i) % DATA_AREA_SIZE];
    }
    // 更新读指针
    uint32_t new_r = (r + 2 + msg_len) % DATA_AREA_SIZE;
    __sync_synchronize();
    rx_rb->read_ptr = new_r;
    *len = msg_len;
    return 0;
}
