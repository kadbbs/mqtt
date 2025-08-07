#include <bits/stdc++.h>
#include "../ElegantLog.hpp"
int main() {
    // 初始化日志系统
    ElegantLog::initDefaultLogger(true, false, "myapp.log");
    
    // 或者手动创建文件日志（带轮转功能）
    // auto file_sink = std::make_shared<ElegantLog::FileSink>("myapp.log", 5*1024*1024, 3);
    // ElegantLog::Logger::instance().addSink(file_sink);
    
    // 设置日志级别
    ElegantLog::Logger::instance().setLevel(ElegantLog::LogLevel::DEBUG);
    
    // 测试日志写入
    for (int i = 0; i < 10; ++i) {
        LOG_INFO("This is log message , testing file rotation", i);
    }
    
    return 0;
}
