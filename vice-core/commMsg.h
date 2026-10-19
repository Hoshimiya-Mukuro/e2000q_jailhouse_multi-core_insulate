#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ringBuf.h"

#define SHM_BASE         0xb1004000
#define SHM_SIZE         0x4000
#define CH_A2B_OFFSET    0x0000   // 副核 → 主核
#define CH_B2A_OFFSET    0x2000   // 主核 → 副核

// 初始化共享内存映射
int init_shm(); 

// 向主核发送一条消息
// 返回值：0 成功， -1 缓冲区满
int send_to_root(const uint8_t *msg, uint32_t len); 

// 从主核接收一条消息
// 缓冲区：副核提供的预分配内存，len 返回实际长度
// 返回值：0 成功， -1 无数据
int recv_from_root(uint8_t *buf, uint32_t *len); 


