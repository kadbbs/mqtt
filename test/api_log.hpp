// ElegantLog.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <condition_variable>

namespace ElegantLog
{
    // ==================== 核心枚举和工具 ====================
    /// @brief 日志级别枚举
    /// @note 从TRACE(最详细)到FATAL(最严重)共6个级别
    enum class LogLevel
    {
        TRACE,  ///< 跟踪信息，最详细的日志级别
        DEBUG,  ///< 调试信息，开发阶段使用
        INFO,   ///< 重要运行时事件
        WARN,   ///< 警告事件，需要注意但非错误
        ERROR,  ///< 错误事件，但应用仍可运行
        FATAL   ///< 严重错误，可能导致应用终止
    };

    /// @brief 将日志级别转换为字符串
    /// @param level 日志级别枚举值
    /// @return 对应的级别名称字符串
    inline const char *levelToString(LogLevel level)
    {
        static const char *levels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        auto index = static_cast<size_t>(level);
        return (index < sizeof(levels) / sizeof(levels[0])) ? levels[index] : "UNKNOWN";
    }

    // ==================== 格式化工具 ====================
    namespace detail
    {
        /// @brief 格式化辅助函数基础情况（无参数时）
        /// @param oss 输出字符串流
        /// @param fmt 格式字符串
        /// @param pos 当前处理位置
        inline void formatHelper(std::ostringstream &oss, const std::string &fmt, size_t pos = 0)
        {
            oss << fmt.substr(pos);
        }

        /// @brief 格式化辅助函数递归情况（处理单个参数）
        /// @tparam T 参数类型
        /// @param oss 输出字符串流
        /// @param fmt 格式字符串
        /// @param pos 当前处理位置
        /// @param arg 要格式化的参数
        template <typename T>
        void formatHelper(std::ostringstream &oss, const std::string &fmt,
                          size_t pos, T &&arg)
        {
            pos = fmt.find("{}", pos);
            if (pos == std::string::npos)
            {
                oss << fmt.substr(0, pos);
                return;
            }
            oss << fmt.substr(0, pos);
            oss << std::forward<T>(arg);
            formatHelper(oss, fmt, pos + 2);
        }

        /// @brief 格式化辅助函数递归情况（处理多个参数）
        /// @tparam T 第一个参数类型
        /// @tparam Args 剩余参数类型包
        /// @param oss 输出字符串流
        /// @param fmt 格式字符串
        /// @param pos 当前处理位置
        /// @param arg 第一个参数
        /// @param args 剩余参数包
        template <typename T, typename... Args>
        void formatHelper(std::ostringstream &oss, const std::string &fmt,
                          size_t pos, T &&arg, Args &&...args)
        {
            pos = fmt.find("{}", pos);
            if (pos == std::string::npos)
            {
                oss << fmt.substr(0, pos);
                return;
            }
            oss << fmt.substr(0, pos);
            oss << std::forward<T>(arg);
            formatHelper(oss, fmt, pos + 2, std::forward<Args>(args)...);
        }
    }

    /// @brief 格式化字符串函数
    /// @tparam Args 可变参数类型
    /// @param fmt 格式字符串（使用{}作为占位符）
    /// @param args 要格式化的参数
    /// @return 格式化后的字符串
    template <typename... Args>
    std::string format(const std::string &fmt, Args &&...args)
    {
        std::ostringstream oss;
        detail::formatHelper(oss, fmt, 0, std::forward<Args>(args)...);
        return oss.str();
    }

    // ==================== 日志输出目标 ====================
    /// @brief 日志输出目标抽象基类
    class Sink
    {
    public:
        virtual ~Sink() = default;
        
        /// @brief 写入日志
        /// @param level 日志级别
        /// @param message 日志消息
        virtual void log(LogLevel level, const std::string &message) = 0;
        
        /// @brief 刷新缓冲区
        virtual void flush() = 0;
    };

    /// @brief 控制台日志输出目标
    class ConsoleSink : public Sink
    {
    public:
        /// @brief 构造函数
        /// @param use_color 是否使用颜色输出
        explicit ConsoleSink(bool use_color = true) : m_use_color(use_color) {}

        void log(LogLevel level, const std::string &message) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_use_color)
            {
                std::clog << getColor(level) << message << "\033[0m\n";
            }
            else
            {
                std::clog << message << '\n';
            }
        }

        void flush() override { std::clog.flush(); }

    private:
        /// @brief 获取日志级别对应的颜色代码
        /// @param level 日志级别
        /// @return ANSI颜色代码字符串
        std::string getColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::TRACE: return "\033[37m"; // White
            case LogLevel::DEBUG: return "\033[36m"; // Cyan
            case LogLevel::INFO:  return "\033[32m"; // Green
            case LogLevel::WARN:  return "\033[33m"; // Yellow
            case LogLevel::ERROR: return "\033[31m"; // Red
            case LogLevel::FATAL: return "\033[35m"; // Magenta
            default: return "";
            }
        }

        std::mutex m_mutex;      ///< 互斥锁保证线程安全
        bool m_use_color;        ///< 是否使用颜色输出
    };

    /// @brief 文件日志输出目标（支持日志轮转）
    class FileSink : public Sink 
    {
    public:
        /**
         * @brief 构造函数
         * @param base_filename  基础文件名 (如 "app.log")
         * @param max_size       单个日志文件最大字节数 (默认10MB)
         * @param max_files      保留的日志文件个数 (默认5个)
         * @param flush_interval 自动刷新间隔(秒) (默认3秒)
         */
        explicit FileSink(const std::string& base_filename,
                        size_t max_size = 10 * 1024 * 1024,
                        uint8_t max_files = 5,
                        int flush_interval = 3)
            : m_base_filename(base_filename),
              m_max_size(max_size),
              m_max_files(max_files),
              m_flush_interval(flush_interval) {
            openFile();
            startFlushThread();
        }

        ~FileSink() {
            stopFlushThread();
            if (m_file.is_open()) {
                m_file.flush();
                m_file.close();
            }
        }

        void log(LogLevel level, const std::string& message) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // 检查是否需要轮转
            if (m_current_size + message.size() > m_max_size) {
                rotateFile();
            }
            
            // 写入日志 (包含时间戳)
            auto now = std::chrono::system_clock::now();
            m_file << formatTime(now) << " [" << levelToString(level) << "] "
                   << message << '\n';
            m_current_size += message.size();
            
            // 标记需要刷新
            m_needs_flush = true;
        }

        void flush() override {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open()) {
                m_file.flush();
                m_needs_flush = false;
            }
        }

    private:
        /// @brief 打开日志文件
        void openFile() {
            m_file.open(m_base_filename, std::ios::app);
            if (!m_file.is_open()) {
                throw std::runtime_error("无法打开日志文件: " + m_base_filename);
            }
            m_file.seekp(0, std::ios::end);
            m_current_size = m_file.tellp();
        }

        /// @brief 日志文件轮转
        void rotateFile() {
            m_file.close();
            
            // 轮转旧文件 (app.log.1 → app.log.2, etc.)
            for (int i = m_max_files - 1; i > 0; --i) {
                std::string old_name = m_base_filename + "." + std::to_string(i);
                std::string new_name = m_base_filename + "." + std::to_string(i + 1);
                std::rename(old_name.c_str(), new_name.c_str());
            }
            
            // 当前文件 → app.log.1
            std::rename(m_base_filename.c_str(), (m_base_filename + ".1").c_str());
            
            // 创建新文件
            openFile();
        }

        /// @brief 格式化时间戳
        /// @param tp 时间点
        /// @return 格式化后的时间字符串
        std::string formatTime(const std::chrono::system_clock::time_point& tp) {
            auto in_time_t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm;
            localtime_r(&in_time_t, &tm);  // 线程安全版本
            
            char buffer[80];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
            return buffer;
        }

        /// @brief 启动刷新线程
        void startFlushThread() {
            m_flush_thread = std::thread([this] {
                while (!m_stop_flush_thread) {
                    std::this_thread::sleep_for(std::chrono::seconds(m_flush_interval));
                    if (m_needs_flush) {
                        flush();
                    }
                }
            });
        }

        /// @brief 停止刷新线程
        void stopFlushThread() {
            m_stop_flush_thread = true;
            if (m_flush_thread.joinable()) {
                m_flush_thread.join();
            }
        }

        std::string m_base_filename;      ///< 基础文件名
        std::ofstream m_file;             ///< 文件输出流
        size_t m_current_size = 0;        ///< 当前文件大小
        const size_t m_max_size;          ///< 单个文件最大大小
        const uint8_t m_max_files;        ///< 最大保留文件数
        const int m_flush_interval;       ///< 自动刷新间隔(秒)
        std::mutex m_mutex;               ///< 互斥锁保证线程安全
        std::thread m_flush_thread;       ///< 自动刷新线程
        std::atomic<bool> m_stop_flush_thread{false}; ///< 停止线程标志
        std::atomic<bool> m_needs_flush{false};       ///< 需要刷新标志
    };

    // ==================== 异步日志引擎 ====================
    /// @brief 异步日志处理引擎
    class AsyncLogEngine
    {
    public:
        AsyncLogEngine() : m_running(true)
        {
            m_worker = std::thread([this] { work(); });
        }

        ~AsyncLogEngine()
        {
            stop();
        }

        /// @brief 停止异步引擎
        void stop()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_running = false;
            }
            m_cv.notify_one();
            if (m_worker.joinable())
            {
                m_worker.join();
            }
        }

        /// @brief 提交日志任务
        /// @tparam F 可调用对象类型
        /// @param task 要执行的任务
        template <typename F>
        void submit(F &&task)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue.emplace(std::forward<F>(task));
            }
            m_cv.notify_one();
        }

    private:
        /// @brief 工作线程函数
        void work()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] { return !m_queue.empty() || !m_running; });

                    if (!m_running && m_queue.empty())
                        break;

                    if (!m_queue.empty())
                    {
                        task = std::move(m_queue.front());
                        m_queue.pop();
                    }
                }
                if (task)
                    task();
            }
        }

        std::queue<std::function<void()>> m_queue; ///< 任务队列
        std::mutex m_mutex;                       ///< 保护队列的互斥锁
        std::condition_variable m_cv;             ///< 条件变量
        bool m_running;                           ///< 运行标志
        std::thread m_worker;                     ///< 工作线程
    };

    // ==================== 核心日志器 ====================
    /// @brief 日志系统核心类
    class Logger
    {
    public:
        Logger() : m_level(LogLevel::INFO), m_async(true) {}

        /// @brief 设置日志级别
        void setLevel(LogLevel level) { m_level = level; }
        
        /// @brief 获取当前日志级别
        LogLevel level() const { return m_level; }

        /// @brief 设置异步模式
        /// @param async 是否启用异步模式
        void setAsync(bool async)
        {
            if (async && !m_async)
            {
                m_async_engine.reset(new AsyncLogEngine());
            }
            m_async = async;
        }
        
        /// @brief 是否异步模式
        bool async() const { return m_async; }

        /// @brief 添加日志输出目标
        /// @param sink 日志输出目标
        void addSink(std::shared_ptr<Sink> sink)
        {
            std::lock_guard<std::mutex> lock(m_sinks_mutex);
            m_sinks.push_back(sink);
        }

        /// @brief 移除所有输出目标
        void removeAllSinks()
        {
            std::lock_guard<std::mutex> lock(m_sinks_mutex);
            m_sinks.clear();
        }

        /// @brief 记录日志
        /// @tparam Args 可变参数类型
        /// @param level 日志级别
        /// @param fmt 格式字符串
        /// @param args 日志参数
        template <typename... Args>
        void log(LogLevel level, const std::string &fmt, Args &&...args)
        {
            if (level < m_level)
                return;

            auto message = format("[{}] {}", levelToString(level),
                                  format(fmt, std::forward<Args>(args)...));

            if (m_async)
            {
                if (!m_async_engine)
                {
                    m_async_engine.reset(new AsyncLogEngine());
                }
                m_async_engine->submit([this, level, message] { logToSinks(level, message); });
            }
            else
            {
                logToSinks(level, message);
            }
        }

        /// @brief 获取日志器单例实例
        static Logger &instance()
        {
            static Logger logger;
            return logger;
        }

        ~Logger()
        {
            if (m_async_engine)
            {
                m_async_engine->stop();
            }
        }

    private:
        /// @brief 将日志写入所有输出目标
        /// @param level 日志级别
        /// @param message 日志消息
        void logToSinks(LogLevel level, const std::string &message)
        {
            std::lock_guard<std::mutex> lock(m_sinks_mutex);
            for (auto &sink : m_sinks)
            {
                if (sink)
                {
                    sink->log(level, message);
                }
            }
        }

        std::atomic<LogLevel> m_level;            ///< 当前日志级别
        std::atomic<bool> m_async;                ///< 是否异步模式
        std::vector<std::shared_ptr<Sink>> m_sinks; ///< 输出目标集合
        std::mutex m_sinks_mutex;                 ///< 保护输出目标的互斥锁
        std::unique_ptr<AsyncLogEngine> m_async_engine; ///< 异步引擎
    };

    // ==================== 便捷宏 ====================
    /// @brief TRACE级别日志宏
    #define LOG_TRACE(fmt, ...) \
        ElegantLog::Logger::instance().log(ElegantLog::LogLevel::TRACE, fmt, ##__VA_ARGS__)
    
    /// @brief DEBUG级别日志宏
    #define LOG_DEBUG(fmt, ...) \
        ElegantLog::Logger::instance().log(ElegantLog::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
    
    /// @brief INFO级别日志宏
    #define LOG_INFO(fmt, ...) \
        ElegantLog::Logger::instance().log(ElegantLog::LogLevel::INFO, fmt, ##__VA_ARGS__)
    
    /// @brief WARN级别日志宏
    #define LOG_WARN(fmt, ...) \
        ElegantLog::Logger::instance().log(ElegantLog::LogLevel::WARN, fmt, ##__VA_ARGS__)
    
    /// @brief ERROR级别日志宏
    #define LOG_ERROR(fmt, ...) \
        ElegantLog::Logger::instance().log(ElegantLog::LogLevel::ERROR, fmt, ##__VA_ARGS__)
    
    /// @brief FATAL级别日志宏
    #define LOG_FATAL(fmt, ...) \
        ElegantLog::Logger::instance().log(ElegantLog::LogLevel::FATAL, fmt, ##__VA_ARGS__)

    // ==================== 初始化工具 ====================
    /// @brief 初始化默认日志器
    /// @param console 是否添加控制台输出
    /// @param file 是否添加文件输出
    /// @param filename 日志文件名（当file为true时有效）
    inline void initDefaultLogger(bool console = true, bool file = false,
                                  const std::string &filename = "log/app.log")
    {
        auto &logger = Logger::instance();
        logger.removeAllSinks();

        if (console)
        {
            logger.addSink(std::make_shared<ConsoleSink>());
        }

        if (file)
        {
            try
            {
                logger.addSink(std::make_shared<FileSink>(filename));
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to create file sink: {}", e.what());
            }
        }
    }

} // namespace ElegantLog