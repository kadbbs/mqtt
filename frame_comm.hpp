#pragma once
// frame_comm.hpp
#include <atomic>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iomanip>
#include <chrono>
#include <ctime>
#include "calculate.hpp"
#include <condition_variable>

// 定义串口配置结构体
struct SerialConfig
{
    std::string com;
    int baud_rate;
    int data_bits;
    std::string parity;
    int stop_bits;
};

#pragma pack(push, 1)
struct WeatherData
{
    // 30秒取样一次，只记录10分钟的
    float avgWindSpeed10min;     // 10分钟平均风速（m/s，精度0.1）
    uint16_t avgWindDir10min;    // 10分钟平均风向（0-359°）
    float maxWindSpeed;          // 最大风速（m/s，精度0.1）取10分钟三次数据的最大值
    float extremeWindSpeed;      // 极大风速（m/s，精度0.1）取最后三次数据的最大值
    float stdWindSpeed;          // 10m高度标准风速（m/s，精度0.1）
    float airTemperature;        // 气温（℃，精度0.1）
    uint16_t humidity;           // 湿度（%RH，0-100）
    float airPressure;           // 气压（hPa，精度0.1）
    float precipitation;         // 10分钟降雨量（mm，精度0.1）
    float precipIntensity;       // 雨强（mm/min，精度0.1）
    uint16_t radiationIntensity; // 光辐射强度（W/m²）
};

#pragma pack(pop)

// 帧结构定义
#pragma pack(push, 1)
struct FrameHeader
{
    uint16_t head = 0x5AA5;
    uint16_t length;
    char cmdID[17];
    uint8_t frameType;
    uint8_t packetType;
    uint8_t frameNo;
};
struct FrameTail
{
    uint16_t crc;
    uint8_t tail = 0x96;
};
#pragma pack(pop)
class RawFrame
{
public:
    FrameHeader header;
    std::vector<uint8_t> data;
    FrameTail tail;
    uint16_t MakeProtocolKey()
    {
        return (static_cast<uint16_t>(header.frameType) << 8) | header.packetType;
    }
    // 只有Rawframe解帧之后可以进行获取size的操作，不然结果是不可预测的
    size_t size() const
    {
        return sizeof(FrameHeader) + sizeof(FrameTail) + data.size();
    }
};
