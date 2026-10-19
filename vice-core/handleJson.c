#include "handleJson.h"

/////////////////////
//函数功能：创建一个json文件，按照参数需求填入对应的参数即可
//可以按照需求添加参数，仿照格式加上去就行，最后一个参数填写生成的文件的名字
/////////////////////
int createSensorDataJson(const char* device_id, double temperature, double humidity, int light, char* status, char* outJson)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) 
    {
        //fprintf(stderr, "创建JSON根对象失败\n");
        return 0;
    }
    cJSON_AddStringToObject(root, "device_id", device_id);
    time_t current_time = time(NULL);
    cJSON_AddNumberToObject(root, "timestamp", (double)current_time);
    cJSON *sensors = cJSON_CreateObject();
    if (!sensors)
    {
        //fprintf(stderr, "创建sensors对象失败\n");
        cJSON_Delete(root);
        return 0;
    }
    cJSON_AddNumberToObject(sensors, "temperature", temperature);
    cJSON_AddNumberToObject(sensors, "humidity", humidity);
    cJSON_AddNumberToObject(sensors, "light", light);
    cJSON_AddItemToObject(root, "sensors", sensors);
    cJSON_AddStringToObject(root, "status", status);
    
    char *json_str = cJSON_Print(root);
    if (!json_str)
    {
        cJSON_Delete(root);
        return 0;
    }

    // 将生成的 JSON 字符串复制到调用者提供的输出缓冲区
    strcpy(outJson, json_str);

    free(json_str);
    cJSON_Delete(root);

    return 1;
}
////////////////////////////////////
//函数功能：处理单片机上传的json字符串的解析函数，
//输入一个字符串，对应数据通过结构体获取
////////////////////////////////////////////
int analysisSensorDataJson(char* json_str, SenserData *data)
{
    char *json_buffer = malloc(strlen(json_str)+1);
    strcpy(json_buffer,json_str);

    cJSON *root = cJSON_Parse(json_buffer);
    free(json_buffer);
    if (!root)
    {
        return 0;
    }

    cJSON *device_id = cJSON_GetObjectItemCaseSensitive(root, "device_id");
    if (cJSON_IsString(device_id) && (device_id->valuestring != NULL))
    {
        strncpy(data->deviceId, device_id->valuestring, sizeof(data->deviceId) - 1);
        data->deviceId[sizeof(data->deviceId) - 1] = '\0';
    }
    cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(root, "timestamp");
    if (cJSON_IsNumber(timestamp))
    {
        data->timestamp = (long)timestamp->valuedouble;
    }

    cJSON *sensors = cJSON_GetObjectItemCaseSensitive(root, "sensors");
    if (cJSON_IsObject(sensors))
    {
        cJSON *temp = cJSON_GetObjectItemCaseSensitive(sensors, "temperature");
        if (cJSON_IsNumber(temp))
        {
            data->temperature = temp->valuedouble;
        }
        cJSON *hum = cJSON_GetObjectItemCaseSensitive(sensors, "humidity");
        if (cJSON_IsNumber(hum))
        {
            data->humidity = hum->valuedouble;
        }
        cJSON *light = cJSON_GetObjectItemCaseSensitive(sensors, "light");
        if (cJSON_IsNumber(light))
        {
            data->light = (int)light->valuedouble;
        }
    }
    cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    if (cJSON_IsString(status) && (status->valuestring != NULL))
    {
        strncpy(data->status, status->valuestring, sizeof(data->status) - 1);
        data->status[sizeof(data->status) - 1] = '\0';
    }

    cJSON_Delete(root);
    return 1;
}