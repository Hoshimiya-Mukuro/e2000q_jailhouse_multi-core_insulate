#ifndef SENSORDATA_H
#define SENSORDATA_H

// 必须与副核 handleJson.h 中的 SenserData 结构体完全一致
// 副核使用 #pragma pack(1)，此处同样处理以确保内存布局匹配
#pragma pack(push, 1)
struct SensorData
{
    char deviceId[64];
    double temperature;
    double humidity;
    int light;
    char status[16];
    long timestamp;
};
#pragma pack(pop)

#endif // SENSORDATA_H
