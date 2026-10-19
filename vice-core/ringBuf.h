#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stddef.h>

// 每个通道的大小
#define CHANNEL_SIZE    0x2000   // 8KB

// 环形缓冲区结构体（必须与另一侧保持一致）
typedef struct 
{
    volatile uint32_t write_ptr;  // 写指针（生产者更新）
    volatile uint32_t read_ptr;   // 读指针（消费者更新）
    uint8_t data[CHANNEL_SIZE - 8]; // 数据区
} RingBuffer;

// 计算数据区最大有效长度
#define DATA_AREA_SIZE  (CHANNEL_SIZE - 8)

// 判断缓冲区是否为空
static inline int ringbuf_empty(const RingBuffer *rb) 
{
    return rb->write_ptr == rb->read_ptr;
}

// 判断缓冲区是否已满
static inline int ringbuf_full(const RingBuffer *rb) 
{
    uint32_t w = rb->write_ptr;
    uint32_t r = rb->read_ptr;
    return ((w + 1) % DATA_AREA_SIZE) == r;
}

// 获取可读字节数
static inline uint32_t ringbuf_available(const RingBuffer *rb) 
{
    uint32_t w = rb->write_ptr;
    uint32_t r = rb->read_ptr;
    return (w >= r) ? (w - r) : (DATA_AREA_SIZE - r + w);
}

#endif