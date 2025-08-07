#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdint.h>
#include <fstream>
#include "calculate.hpp"
#include <vector>
#include <thread>
#include <mutex>
#include <iostream>
#include "frame_comm.hpp"
template <typename T>
class ringbuff
{

public:
    ringbuff(int size) : data(size), capacity_(size)
    {
        size_ = 0;
        front_ = 0;
        end_ = 0;
    }
    int size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }
    int capacity()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return capacity_;
    }
    void push(T t)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ < capacity_)
        {
            data[size_] = T(t);
            end_++;
            end_ %= capacity_;
            size_++;
        }
        else
        {
            front_ %= capacity_;
            data[front_] = T(t);
            front_++;
            front_ %= capacity_;

            end_++;
            end_ %= capacity_;
        }
    }
    T &back()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        front_ %= capacity_;
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        if (size_ < capacity_)
        {
            printf("当前行号: %d\n", __LINE__); // 输出此行行号
            printf("size_ %d \n", size_);
            return data[size_ - 1];
        }
        else
        {
            int x = front_;
            x += capacity_ - 1;
            x %= capacity_;
            return data[x];
            printf("当前行号: %d\n", __LINE__); // 输出此行行号
        }
    }
    T &operator[](int index)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // 0123456789
        // front=4 4567890123
        index += front_;
        index %= capacity_;

        return data[index];
    }

private:
    std::vector<T> data;
    int size_;
    int capacity_;
    int front_;
    int end_;
    std::mutex mutex_;
};

class meteserial
{
#pragma pack(push, 1)
    struct metemodbus
    {
        uint8_t sysaddr = 0x00; // 系统地址
        uint8_t fun = 0x03;
        uint8_t regaddr_h = 0x00;
        uint8_t regaddr_l;
        uint8_t regcnt_h = 0x00;
        uint8_t regcnt_l;
        uint16_t crc16;
    };

#pragma pack(pop)

private:
    meteserial(std::string com = "/dev/ttysWK2", int baud_rate = 9600, int bits = 8,
               std::string parity = "n", int stop_bits = 1)
    {

        config.com = com;
        config.baud_rate = baud_rate;
        config.parity = parity;
        config.stop_bits = stop_bits;

        data.regaddr_l = 0x00;
        data.regcnt_l = 0x08;
        tx_cmd = (uint8_t *)&data;
        tx_len = sizeof(data);
        data.crc16 = modbus_crc16(tx_cmd, tx_len - 2);
    }

public:
    static meteserial &instance()
    {
        static meteserial instance;
        return instance;
    }

    ~meteserial()
    {

        if (work_.joinable())
        {
            running = false;
            work_.join();
        }
        // 1. 关闭文件描述符（如果有效）
        if (fd != -1)
        {
            close(fd); // Linux 系统调用，Windows 用 CloseHandle()
            fd = -1;   // 标记为已关闭
        }
    }

private:
    int fd;
    int n = 0;
    int cnt = 0;
    SerialConfig config;
    metemodbus data;
    uint8_t *tx_cmd;
    size_t tx_len;
    uint8_t rx_buf[128] = {0};
    ringbuff<std::vector<uint8_t>> buff{30};
    std::mutex mutex_;
    std::atomic<bool> running{true};
    std::thread work_;

public:
    int start()
    {
        // 打开串口
        fd = open(config.com.data(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0)
        {
            perror("open");
            return -1;
        }
        // 配置串口
        if (configure_serial(fd) < 0)
        {
            close(fd);
            return -1;
        }
        running = true;
        work_ = std::thread(&meteserial::fromreg, this);
        // // 等待线程完成
        // if (work_.joinable())
        // {
        //     work_.join(); // 确保线程完成后再退出
        // }
    }

    int fromreg()
    {
        while (running)
        {

            {
                std::lock_guard<std::mutex> lock(mutex_);
                n = 0;
                cnt = 0;
                memset(rx_buf, 0, sizeof(rx_buf));
                // 发送命令
                while (1)
                {
                    n = write(fd, tx_cmd + cnt, sizeof(data) - cnt);
                    if (n < 0)
                    {
                        perror("write");
                        close(fd);
                        return -1;
                    }
                    else if (n == 0)
                    {
                        break;
                    }
                    cnt += n;
                    // printf("Sent %zd bytes: ", n);
                }
                for (int i = 0; i < tx_len; i++)
                {
                    printf("%02X ", tx_cmd[i]);
                }
                printf("\n");
                // 清空读串口缓冲区
                if (tcflush(fd, TCIFLUSH) == -1)
                {
                    perror("Failed to flush read buffer");
                    // 错误处理
                }
                n = 0;
                cnt = 0;
                while (1)
                {
                    n = read(fd, rx_buf + cnt, sizeof(rx_buf));
                    if (n < 0)
                    {
                        perror("read");
                        close(fd);
                        exit(-1);
                    }
                    else if (n == 0)
                    {
                        printf("Timeout reached!\n");
                        break;
                    }
                    cnt += n;
                    if (cnt == 5 + data.regcnt_l * 2)
                    {
                        break;
                    }
                }
                if (0x0000 == modbus_crc16(rx_buf, 5 + data.regcnt_l * 2))
                {
                    // 打印接收到的原始数据
                    printf("Received %d bytes: ", cnt);
                    for (int i = 0; i < 5 + data.regcnt_l * 2; i++)
                    {
                        printf("%02X ", rx_buf[i]);
                    }
                    printf("\n");
                    static int k = 0;
                    std::vector<uint8_t> tempdata(rx_buf + 3, rx_buf + 3 + data.regcnt_l * 2);
                    for (auto i : tempdata)
                    {
                        printf("%02X ", i);
                    }
                    printf("copy tempdata\n");
                    // float floatValue[4];
                    // memcpy(floatValue.data(), tempdata.data(), 4*sizeof(float)); // 避免类型双关（type-punning）问题

                    // std::cout << "\nx_Float Value: " << floatValue[0] << std::endl;
                    // std::cout << "\ny_Float Value: " << floatValue[1] << std::endl;
                    // std::cout << "\nz_Float Value: " << floatValue[2] << std::endl;
                    // std::cout << "\nt_Float Value: " << floatValue[3] << std::endl;
                    buff.push(tempdata);
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 简单休眠
        }
    }

    int getdata()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int allsize = buff.capacity();
        int datasize = buff.size();
        if (datasize == 0)
        {
            return 0;
        }
        WeatherData w_data;
    }

private:
    int configure_serial(int fd)
    {
        struct termios tty;
        memset(&tty, 0, sizeof(tty));

        if (tcgetattr(fd, &tty) != 0)
        {
            perror("tcgetattr");
            return -1;
        }

        switch (config.baud_rate)
        {
        case 9600:
        {
            // 设置波特率 9600
            cfsetospeed(&tty, B9600);
            cfsetispeed(&tty, B9600);
        }
        break;

        default:
        {
            // 设置波特率 9600
            cfsetospeed(&tty, B9600);
            cfsetispeed(&tty, B9600);
        }
        }

        // 8位数据位，无校验，1位停止位
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;

        // 禁用流控
        tty.c_cflag &= ~CRTSCTS;

        // 启用接收
        tty.c_cflag |= CREAD | CLOCAL;

        // 禁用规范模式（原始数据）
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

        // 原始输出
        tty.c_oflag &= ~OPOST;

        // 设置超时：1500ms
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 15;

        if (tcsetattr(fd, TCSANOW, &tty) != 0)
        {
            perror("tcsetattr");
            return -1;
        }

        return 0;
    }

public:

    std::vector<float> getFloatData(){
        std::lock_guard<std::mutex> lock(mutex_);
        auto &lastData = buff.back();
        printf("当前行号: %d\n", __LINE__);
        std::vector<float> floatValue(4);
        memcpy(floatValue.data(), lastData.data(), 4*sizeof(float)); // 避免类型双关（type-punning）问题
        std::cout << "\nx_Float Value: " << floatValue[0] << std::endl;
        std::cout << "\ny_Float Value: " << floatValue[1] << std::endl;
        std::cout << "\nz_Float Value: " << floatValue[2] << std::endl;
        std::cout << "\nt_Float Value: " << floatValue[3] << std::endl;
        return floatValue;
    }

    WeatherData processWeatherData()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        WeatherData result = {};
        int count = buff.size();
        std::cout << "count is " << count << std::endl;

        if (count == 0)
        {
            std::cerr << "Error: No data available!" << std::endl;
            return result; // 直接返回一个空的结构体
        }
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        float totalWindSpeed = 0;
        float totalPrecipitation = 0;
        float totalAirTemperature = 0;
        float totalAirPressure = 0;
        uint16_t totalHumidity = 0;
        uint16_t totalWindDirection = 0;
        uint16_t totalRadiationIntensity = 0;
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        // 解析所有数据项
        for (int i = 0; i < count; ++i)
        {
            const auto &rawData = buff[i];

            // 假设每个 rawData 是从传感器读取到的一组数据
            totalWindSpeed += parseWindSpeed(rawData);
            totalPrecipitation += parsePrecipitation(rawData);
            totalAirTemperature += parseAirTemperature(rawData);
            totalAirPressure += parseAirPressure(rawData);
            totalHumidity += parseHumidity(rawData);
            totalWindDirection += parseWindDirection(rawData);
            totalRadiationIntensity += parseRadiationIntensity(rawData);
        }
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        // 计算平均值（对于需要平均的字段）
        result.avgWindSpeed10min = totalWindSpeed / count;
        result.avgWindDir10min = totalWindDirection / count;
        result.precipitation = totalPrecipitation / count;
        result.airTemperature = totalAirTemperature / count;
        result.airPressure = totalAirPressure / count;
        result.humidity = totalHumidity / count;
        result.radiationIntensity = totalRadiationIntensity / count;
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        // 对于一些单独的字段直接取最后一个数据
        const auto &lastData = buff.back();

        for (auto i : lastData)
        {
            printf("%02X ", i);
        }
        printf("\n");

        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        result.maxWindSpeed = parseMaxWindSpeed(lastData);
        // result.extremeWindSpeed = parseExtremeWindSpeed(lastData);
        // result.stdWindSpeed = parseStdWindSpeed(lastData);
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        return result;
    }

    void printWeatherData(const WeatherData &data)
    {
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        std::cout << "Weather Data:" << std::endl;
        std::cout << "-----------------------------" << std::endl;

        std::cout << "Average Wind Speed (10 min): " << std::fixed << std::setprecision(2) << data.avgWindSpeed10min << " m/s" << std::endl;
        std::cout << "Average Wind Direction (10 min): " << data.avgWindDir10min << "°" << std::endl;
        std::cout << "Max Wind Speed: " << std::fixed << std::setprecision(2) << data.maxWindSpeed << " m/s" << std::endl;
        std::cout << "Extreme Wind Speed: " << std::fixed << std::setprecision(2) << data.extremeWindSpeed << " m/s" << std::endl;
        std::cout << "Standard Wind Speed (10m): " << std::fixed << std::setprecision(2) << data.stdWindSpeed << " m/s" << std::endl;
        std::cout << "Air Temperature: " << std::fixed << std::setprecision(2) << data.airTemperature << " °C" << std::endl;
        std::cout << "Humidity: " << data.humidity << "%" << std::endl;
        std::cout << "Air Pressure: " << std::fixed << std::setprecision(2) << data.airPressure << " hPa" << std::endl;
        std::cout << "Precipitation (10 min): " << std::fixed << std::setprecision(2) << data.precipitation << " mm" << std::endl;
        std::cout << "Precipitation Intensity: " << std::fixed << std::setprecision(2) << data.precipIntensity << " mm/min" << std::endl;
        std::cout << "Radiation Intensity: " << data.radiationIntensity << " W/m²" << std::endl;

        std::cout << "-----------------------------" << std::endl;
    }

public:
    void test()
    {
        auto ret = processWeatherData();
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
        printWeatherData(ret);
        printf("当前行号: %d\n", __LINE__); // 输出此行行号
    }

private:
    // 解析各个字段
    float parseAirTemperature(const std::vector<uint8_t> &rawData)
    {
        uint16_t tempRaw = (rawData[0] << 8) | rawData[1];
        return (tempRaw / 100.0f) - 40.0f;
    }

    uint16_t parseHumidity(const std::vector<uint8_t> &rawData)
    {
        uint16_t humidityRaw = (rawData[2] << 8) | rawData[3];
        return humidityRaw / 100;
    }

    float parseAirPressure(const std::vector<uint8_t> &rawData)
    {
        uint16_t pressureRaw = (rawData[4] << 8) | rawData[5];
        return pressureRaw / 10.0f;
    }

    float parseWindSpeed(const std::vector<uint8_t> &rawData)
    {
        uint16_t windSpeedRaw = (rawData[6] << 8) | rawData[7];
        return windSpeedRaw / 100.0f;
    }

    uint16_t parseWindDirection(const std::vector<uint8_t> &rawData)
    {
        uint16_t windDirectionRaw = (rawData[8] << 8) | rawData[9];
        return windDirectionRaw / 10;
    }

    float parsePrecipitation(const std::vector<uint8_t> &rawData)
    {
        uint16_t precipitationRaw = (rawData[10] << 8) | rawData[11];
        return precipitationRaw / 10.0f;
    }

    uint16_t parseRadiationIntensity(const std::vector<uint8_t> &rawData)
    {
        uint16_t radiationRaw = (rawData[12] << 8) | rawData[13];
        return radiationRaw;
    }

    float parseMaxWindSpeed(const std::vector<uint8_t> &rawData)
    {
        printf("%d line: rawdata size is%d", __LINE__, rawData.size());
        uint16_t maxWindSpeedRaw = (rawData[14] << 8) | rawData[15];
        return maxWindSpeedRaw / 100.0f;
    }

    // float parseExtremeWindSpeed(const std::vector<uint8_t>& rawData)
    // {
    //     uint16_t extremeWindSpeedRaw = (rawData[16] << 8) | rawData[17];
    //     return extremeWindSpeedRaw / 100.0f;
    // }

    // float parseStdWindSpeed(const std::vector<uint8_t>& rawData)
    // {
    //     uint16_t stdWindSpeedRaw = (rawData[18] << 8) | rawData[19];
    //     return stdWindSpeedRaw / 100.0f;
    // }
};