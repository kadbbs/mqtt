// #define SPDLOG_NO_TLS  // 禁用 TLS
// #include "spdlog/spdlog.h"

#include <string>
#include <csignal>
#include <iostream>
#include "publisher.hpp"
#include "subscriber.hpp"
#include "serial.hpp"
#include "ElegantLog.hpp"

// void signal_handler(int signal) {
//     LOG_WARN("收到中断信号: {}", signal);
//     LOG_INFO("日志系统已停止");
//     // 显式停止日志系统
//     ElegantLog::Logger::instance().flushSinks();
//     ElegantLog::Logger::instance().setAsync(false); // stop async engine
//     exit(0);
// }
int main(int argc, char const *argv[])
{
    // std::signal(SIGINT, signal_handler);

    // 初始化日志系统
    auto &logger = ElegantLog::Logger::instance();

    // 添加控制台输出
    logger.addSink(std::make_shared<ElegantLog::ConsoleSink>());

    // 添加文件输出 (10MB轮转，保留3个文件，每5秒刷新)
    logger.addSink(std::make_shared<ElegantLog::FileSink>(
        "myapp.log",
        10 * 1024 * 1024, // 10MB
        3,                // 保留3个文件
        5                 // 每5秒刷新
        ));

    // 设置日志级别
    logger.setLevel(ElegantLog::LogLevel::DEBUG);

    Publisher publisher("broker.emqx.io:1883", "cpp_publisher", "yun/topic");
    Subscriber subscriber("broker.emqx.io:1883", "cpp_subscriber", "yun/topic");
    auto &serial = meteserial::instance();
    serial.start();
    // 测试日志写入
    for (int i = 0; i < 1000; ++i)
    {
        LOG_INFO("This is log message {}, testing file rotation", i);
    }
    // 显式刷新日志或关闭异步

    std::this_thread::sleep_for(std::chrono::seconds(13)); // 简单休眠
    try
    {
        // Subscribe to the topic and wait for messages
        subscriber.subtopic();

        while (1)
        {
            auto ret = serial.getFloatData();
            std::string payload = "x: " + std::to_string(ret[0]) +
                                  ",y: " + std::to_string(ret[1]) +
                                  ", z: " + std::to_string(ret[2]) +
                                  ", t: " + std::to_string(ret[3]);

            publisher.publish(payload);

            // Publish a message
            std::this_thread::sleep_for(std::chrono::seconds(6)); // 简单休眠
        }
    }
    catch (const mqtt::exception &exc)
    {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}
