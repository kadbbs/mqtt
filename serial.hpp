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
#include "ElegantLog.hpp"
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

        front_ %= capacity_;

        if (size_ < capacity_)
        {

            printf("size_ %d \n", size_);
            return data[size_ - 1];
        }
        else
        {
            int x = front_;
            x += capacity_ - 1;
            x %= capacity_;
            return data[x];

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
        config.data_bits = bits;
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
    void start()
    {
        // 打开串口
        fd = open(config.com.data(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0)
        {
            LOG_ERROR("Failed to open serial port: {}{}", config.com,strerror(errno));
            exit(EXIT_FAILURE);
        }
        // 配置串口
        if (configure_serial() < 0)
        {
            close(fd);
            LOG_ERROR("Failed to configure serial port: {}{}", config.com, strerror(errno));
            exit(EXIT_FAILURE);
        }
        running = true;
        work_ = std::thread(&meteserial::fromreg, this);

    }

    void fromreg()
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
                        LOG_ERROR("Failed to write to serial port: {}{}", config.com, strerror(errno));
                        close(fd);
                        exit(-1);
                    }
                    else if (n == 0)
                    {
                        break;
                    }
                    cnt += n;
                    // printf("Sent %zd bytes: ", n);
                }
                for (size_t i = 0; i < tx_len; i++)
                {
                    printf("%02X ", tx_cmd[i]);
                }
                printf("\n");
                // 清空读串口缓冲区
                if (tcflush(fd, TCIFLUSH) == -1)
                {
                    LOG_ERROR("Failed to flush read buffer: {}{}", config.com, strerror(errno));
                    close(fd);
                    exit(-1);
                    // 错误处理
                }
                n = 0;
                cnt = 0;
                while (1)
                {
                    n = read(fd, rx_buf + cnt, sizeof(rx_buf));
                    if (n < 0)
                    {
                        LOG_ERROR("Failed to read from serial port: {}{}", config.com, strerror(errno));
                        close(fd);
                        exit(-1);
                    }
                    else if (n == 0)
                    {
                        printf("Timeout reached!\n");
                        LOG_WARN("Timeout reached when reading from serial port: {}", config.com);
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
                    LOG_INFO("Received {} bytes from serial port: {}", cnt, config.com);

                    LOG_INFO("{}", ElegantLog::formathex(rx_buf, 5 + data.regcnt_l * 2));

                    std::vector<uint8_t> tempdata(rx_buf + 3, rx_buf + 3 + data.regcnt_l * 2);
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

public:

    std::vector<float> getFloatData(){
        std::lock_guard<std::mutex> lock(mutex_);
        auto &lastData = buff.back();
        std::vector<float> floatValue(4);
        memcpy(floatValue.data(), lastData.data(), 4*sizeof(float)); // 避免类型双关（type-punning）问题
        std::cout << "\nx_Float Value: " << floatValue[0] << std::endl;
        std::cout << "\ny_Float Value: " << floatValue[1] << std::endl;
        std::cout << "\nz_Float Value: " << floatValue[2] << std::endl;
        std::cout << "\nt_Float Value: " << floatValue[3] << std::endl;
        return floatValue;
    }

private:
    int configure_serial()
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



};