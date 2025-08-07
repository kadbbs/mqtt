// ElegantLog.hpp
#pragma once
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
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

    std::string formathex(const std::string &fmt,uint8_t data){
        char buffer[10];
        snprintf(buffer, sizeof(buffer), fmt.c_str(), data);
        return std::string(buffer);
    }

    // ==================== 核心枚举和工具 ====================
    enum class LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    inline const std::string levelToString(LogLevel level)
    {
        static const char *levels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        auto index = static_cast<size_t>(level);
        return (index < sizeof(levels) / sizeof(levels[0])) ? levels[index] : "UNKNOWN";
    }

    // ==================== 格式化工具 ====================
    namespace detail
    {
        // 结束递归
        inline void formatHelper(std::ostringstream &oss, const std::string &fmt, size_t pos = 0)
        {
            oss << fmt.substr(pos);
        }

        // 单参数版本
        template <typename T>
        void formatHelper(std::ostringstream &oss, const std::string &fmt, size_t pos, T &&arg)
        {
            size_t next_pos = fmt.find("{}", pos);
            if (next_pos == std::string::npos)
            {
                oss << fmt.substr(pos); // 只输出剩余部分
                return;
            }
            oss << fmt.substr(pos, next_pos - pos);
            oss << std::forward<T>(arg);
            formatHelper(oss, fmt, next_pos + 2);
        }

        // 多参数版本
        template <typename T, typename... Args>
        void formatHelper(std::ostringstream &oss, const std::string &fmt, size_t pos, T &&arg, Args &&...args)
        {
            size_t next_pos = fmt.find("{}", pos);
            if (next_pos == std::string::npos)
            {
                oss << fmt.substr(pos);
                return;
            }
            oss << fmt.substr(pos, next_pos - pos);
            oss << std::forward<T>(arg);
            formatHelper(oss, fmt, next_pos + 2, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    std::string format(const std::string &fmt, Args &&...args)
    {
        std::ostringstream oss;
        detail::formatHelper(oss, fmt, 0, std::forward<Args>(args)...);
        return oss.str();
    }

    // ==================== 日志输出目标 ====================
    class Sink
    {
    public:
        virtual ~Sink() = default;
        virtual void log(LogLevel level, const std::string &message) = 0;
        virtual void flush() = 0;
    };

    class ConsoleSink : public Sink
    {
    public:
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
        std::string getColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::TRACE:
                return "\033[37m"; // White
            case LogLevel::DEBUG:
                return "\033[36m"; // Cyan
            case LogLevel::INFO:
                return "\033[32m"; // Green
            case LogLevel::WARN:
                return "\033[33m"; // Yellow
            case LogLevel::ERROR:
                return "\033[31m"; // Red
            case LogLevel::FATAL:
                return "\033[35m"; // Magenta
            default:
                return "";
            }
        }

        std::mutex m_mutex;
        bool m_use_color;
    };

    // 在 ElegantLog.hpp 中完善 FileSink 类
    class FileSink : public Sink
    {
    public:
        /**
         * @param base_filename  基础文件名 (如 "app.log")
         * @param max_size       单个日志文件最大字节数 (默认10MB)
         * @param max_files      保留的日志文件个数 (默认5个)
         * @param flush_interval 自动刷新间隔(秒) (默认3秒)
         */
        explicit FileSink(const std::string &base_filename,
                          size_t max_size = 10 * 1024 * 1024,
                          uint8_t max_files = 5,
                          int flush_interval = 3)
            : m_base_filename(base_filename),
              m_max_size(max_size),
              m_max_files(max_files),
              m_flush_interval(flush_interval)
        {
            openFile();
            startFlushThread();
        }

        ~FileSink()
        {
            stopFlushThread();
            if (m_file.is_open())
            {
                m_file.flush();
                m_file.close();
            }
        }

        void log(LogLevel level, const std::string &message) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // 检查是否需要轮转
            if (m_current_size + message.size() > m_max_size)
            {
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

        void flush() override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_file.is_open())
            {
                m_file.flush();
                m_needs_flush = false;
            }
        }

    private:
        void openFile()
        {
            m_file.open(m_base_filename, std::ios::app);
            if (!m_file.is_open())
            {
                throw std::runtime_error("无法打开日志文件: " + m_base_filename);
            }
            m_file.seekp(0, std::ios::end);
            m_current_size = m_file.tellp();
        }

        void rotateFile()
        {
            m_file.close();

            // 轮转旧文件 (app.log.1 → app.log.2, etc.)
            for (int i = m_max_files - 1; i > 0; --i)
            {
                std::string old_name = m_base_filename + "." + std::to_string(i);
                std::string new_name = m_base_filename + "." + std::to_string(i + 1);
                std::rename(old_name.c_str(), new_name.c_str());
            }

            // 当前文件 → app.log.1
            std::rename(m_base_filename.c_str(), (m_base_filename + ".1").c_str());

            // 创建新文件
            openFile();
        }

        std::string formatTime(const std::chrono::system_clock::time_point &tp)
        {
            auto in_time_t = std::chrono::system_clock::to_time_t(tp);
            std::tm tm;
            localtime_r(&in_time_t, &tm); // 线程安全版本

            char buffer[80];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
            return buffer;
        }

        void startFlushThread()
        {
            m_flush_thread = std::thread([this]
                                         {
            while (!m_stop_flush_thread) {
                std::this_thread::sleep_for(std::chrono::seconds(m_flush_interval));
                if (m_needs_flush) {
                    flush();
                }
            } });
        }

        void stopFlushThread()
        {
            m_stop_flush_thread = true;
            if (m_flush_thread.joinable())
            {
                m_flush_thread.join();
            }
        }

        std::string m_base_filename;
        std::ofstream m_file;
        size_t m_current_size = 0;
        const size_t m_max_size;
        const uint8_t m_max_files;
        const int m_flush_interval;
        std::mutex m_mutex;
        std::thread m_flush_thread;
        std::atomic<bool> m_stop_flush_thread{false};
        std::atomic<bool> m_needs_flush{false};
    };

    // ==================== 异步日志引擎 ====================
    class AsyncLogEngine
    {
    public:
        AsyncLogEngine() : m_running(true)
        {
            m_worker = std::thread([this]
                                   { work(); });
        }
        ~AsyncLogEngine()
        {
            stop();
        }

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
        void work()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this]
                              { return !m_queue.empty() || !m_running; });

                    if (!m_running && m_queue.empty()) break;

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

        std::queue<std::function<void()>> m_queue;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_running;
        std::thread m_worker;
    };

    // ==================== 核心日志器 ====================
    class Logger
    {
    public:
        Logger() : m_level(LogLevel::INFO), m_async(true) {}

        void setLevel(LogLevel level=ElegantLog::LogLevel::DEBUG) { m_level = level; }
        LogLevel level() const { return m_level; }

        void setAsync(bool async)
        {
            if (async && !m_async)
            {
                m_async_engine.reset(new AsyncLogEngine());
            }
            m_async = async;
        }
        bool async() const { return m_async; }

        void addSink(std::shared_ptr<Sink> sink)
        {
            std::lock_guard<std::mutex> lock(m_sinks_mutex);
            m_sinks.push_back(sink);
        }

        void removeAllSinks()
        {
            std::lock_guard<std::mutex> lock(m_sinks_mutex);
            m_sinks.clear();
        }

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
                m_async_engine->submit([this, level, message]
                                       { logToSinks(level, message); });
            }
            else
            {
                logToSinks(level, message);
            }
        }

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

        std::atomic<LogLevel> m_level;
        std::atomic<bool> m_async;
        std::vector<std::shared_ptr<Sink>> m_sinks;
        std::mutex m_sinks_mutex;
        std::unique_ptr<AsyncLogEngine> m_async_engine;
    };

    // ==================== 便捷宏 ====================

#define LOG_TRACE(fmt, ...) \
    ElegantLog::Logger::instance().log(ElegantLog::LogLevel::TRACE, "[{}:{}][{}] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    ElegantLog::Logger::instance().log(ElegantLog::LogLevel::DEBUG, "[{}:{}][{}] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    ElegantLog::Logger::instance().log(ElegantLog::LogLevel::INFO, "[{}:{}][{}] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
    ElegantLog::Logger::instance().log(ElegantLog::LogLevel::WARN, "[{}:{}][{}] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
    ElegantLog::Logger::instance().log(ElegantLog::LogLevel::ERROR, "[{}:{}][{}] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) \
    ElegantLog::Logger::instance().log(ElegantLog::LogLevel::FATAL, "[{}:{}][{}] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

    // ==================== 初始化工具 ====================
    inline void initDefaultLogger(bool console = true, bool file = false,
                                  const std::string &filename = "myapp.log")
    {
        auto &logger = Logger::instance();
        logger.removeAllSinks();
        logger.setLevel(ElegantLog::LogLevel::DEBUG);

        if (console)
        {
            logger.addSink(std::make_shared<ConsoleSink>());
        }

        if (file)
        {
            try
            {

                const char *dir = "./log";

                struct stat st;
                if (stat(dir, &st) == -1)
                {
                    LOG_ERROR("log 目录不存在，正在创建...\n");
                    if (mkdir(dir, 0755) == 0)
                    {
                        LOG_ERROR("log 目录创建成功\n");
                        logger.addSink(std::make_shared<FileSink>(filename));
                    }
                    else
                    {

                        LOG_ERROR("log 目录创建失败\n");
                    }
                }
                else if (!S_ISDIR(st.st_mode))
                {

                    LOG_ERROR("存在 log，但不是一个目录\n");
                }
                else
                {

                    LOG_INFO("log 目录已存在\n");
                    logger.addSink(std::make_shared<FileSink>(filename));
                }

                
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Failed to create file sink: {}", e.what());
            }
        }
    }

} // namespace ElegantLog