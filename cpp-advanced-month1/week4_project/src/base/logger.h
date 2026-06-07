#pragma once
// 异步日志模块（5月13日实现）
// 原理：日志写入不阻塞业务线程，通过 BlockingQueue 异步写文件
// 设计参考：spdlog / muduo AsyncLogging

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iostream>
#include <atomic>
#include <iomanip>
#include <ctime>

// 日志级别
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

inline std::string level_to_str(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

// 获取当前时间字符串
inline std::string current_time_str() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ============================================================
// AsyncLogger：异步日志器
// ============================================================

class AsyncLogger {
public:
    explicit AsyncLogger(const std::string& log_file = "", LogLevel min_level = LogLevel::DEBUG)
        : min_level_(min_level), stopped_(false) {

        if (!log_file.empty()) {
            file_.open(log_file, std::ios::app);
        }

        // 启动后台日志写入线程
        log_thread_ = std::thread([this] { log_worker(); });
    }

    ~AsyncLogger() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
        if (log_thread_.joinable()) log_thread_.join();
        if (file_.is_open()) file_.close();
    }

    // 日志接口
    void log(LogLevel level, const std::string& msg,
             const char* file = "", int line = 0) {
        if (level < min_level_) return;

        std::ostringstream oss;
        oss << "[" << current_time_str() << "] "
            << "[" << level_to_str(level) << "] ";
        if (file && file[0]) {
            oss << "[" << file << ":" << line << "] ";
        }
        oss << msg;

        std::string entry = oss.str();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push(std::move(entry));
        }
        cv_.notify_one();
    }

    // 便捷宏（在下面定义）
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
    void info (const std::string& msg) { log(LogLevel::INFO,  msg); }
    void warn (const std::string& msg) { log(LogLevel::WARN,  msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }

private:
    void log_worker() {
        while (true) {
            std::string entry;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });
                if (queue_.empty()) return;
                entry = std::move(queue_.front());
                queue_.pop();
            }
            // 写到标准输出
            std::cout << entry << std::endl;
            // 写到文件（如果配置了）
            if (file_.is_open()) {
                file_ << entry << "\n";
                file_.flush();
            }
        }
    }

    LogLevel min_level_;
    std::atomic<bool> stopped_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
    std::thread log_thread_;
    std::ofstream file_;
};

// 全局日志器（单例）
inline AsyncLogger& get_logger() {
    static AsyncLogger logger("server.log", LogLevel::DEBUG);
    return logger;
}

// 日志宏（带文件名和行号）
#define LOG_DEBUG(msg) get_logger().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  get_logger().log(LogLevel::INFO,  msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  get_logger().log(LogLevel::WARN,  msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) get_logger().log(LogLevel::ERROR, msg, __FILE__, __LINE__)