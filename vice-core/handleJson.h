#ifndef __HANDLEJSON_H__
#define __HANDLEJSON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"

extern int toStop;
#pragma pack(1) 
typedef struct 
{
    char deviceId[64];
    double temperature;
    double humidity;
    int light;
    char status[16];
    long timestamp;
}SenserData;
#pragma pack()

int createSensorDataJson(const char* device_id, double temperature, double humidity, int light, char* status, char* outJson);

int analysisSensorDataJson(char* jsonStr, SenserData *data);
#endif //__HANDLEJSON_H__
