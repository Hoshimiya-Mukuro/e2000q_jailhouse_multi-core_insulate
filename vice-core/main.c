#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include "handleJson.h"
#include "commMsg.h"

#define ERROR_T       10
#define JSON_BUF_SIZE 4096

int toStop = 0;

void sig_handler(int sig)
{
    printf("\n收到信号 %d，正在退出...\n", sig);
    toStop = 1;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("cell communication model start!\n");

    // 注册信号处理
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 初始化共享内存映射
    if (init_shm() != 0) {
        fprintf(stderr, "共享内存初始化失败，退出。\n");
        return -1;
    }

    // 分配 JSON 接收缓冲区
    char *jsonMsg = malloc(JSON_BUF_SIZE);
    if (!jsonMsg) {
        fprintf(stderr, "内存分配失败。\n");
        return -1;
    }

    toStop = ERROR_T;
    SenserData anaData;

    while (toStop)
    {
        uint32_t rx_len = 0;
        int flag_0 = recv_from_root((uint8_t*)jsonMsg, &rx_len);
        if (flag_0 != 0)
        {
            // 无数据到达
            printf("no data\n");
            toStop--;
            sleep(1);
            continue;
        }

        // 确保接收到的 JSON 字符串以 '\0' 结尾
        if (rx_len < JSON_BUF_SIZE)
            jsonMsg[rx_len] = '\0';
        else
            jsonMsg[JSON_BUF_SIZE - 1] = '\0';

        memset(&anaData, 0, sizeof(anaData));
        analysisSensorDataJson(jsonMsg, &anaData);

        int flag_1 = send_to_root((uint8_t*)&anaData, sizeof(anaData));
        if (flag_1 == -1)
        {
            printf("buffer full!\n");
            toStop--;
        }
        sleep(1);
    }

    free(jsonMsg);
    printf("exit!\n");
    return 0;
}
