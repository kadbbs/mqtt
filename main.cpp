#include <string>
#include <csignal>
#include <iostream>
#include "publisher.hpp"
#include "subscriber.hpp"
#include "serial.hpp"
#include "ElegantLog.hpp"
int main(int argc, char const *argv[])
{

    // 初始化日志系统
    ElegantLog::initDefaultLogger(true, true, "log/myapp.log");

    Publisher publisher("broker.emqx.io:1883", "cpp_publisher", "yun/topic");
    Subscriber subscriber("broker.emqx.io:1883", "cpp_subscriber", "yun/topic");
    auto &serial = meteserial::instance();
    serial.start();
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
        LOG_ERROR("Error: {}", exc.what());
        return 1;
    }

    return 0;
}
