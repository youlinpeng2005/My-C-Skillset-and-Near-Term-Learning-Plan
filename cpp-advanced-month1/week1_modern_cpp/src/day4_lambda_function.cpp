// 第一周 Day4：lambda + std::function + bind
// 学习目标：掌握闭包捕获、std::function 类型擦除、回调注册器设计
// 项目关联：将服务器的函数指针回调改写为 std::function，为 Reactor 模式做准备

#include <iostream>
#include <functional>
#include <vector>
#include <string>
#include <map>
#include <memory>

// ============================================================
// Part 1: lambda 捕获列表详解
// ============================================================

void demo_lambda_capture() {
    std::cout << "=== lambda 捕获列表 ===" << std::endl;

    int x = 10;
    std::string name = "Alice";

    // 值捕获：捕获时拷贝，lambda 内修改不影响外部
    auto by_value = [x, name]() {
        std::cout << "值捕获: x=" << x << ", name=" << name << std::endl;
        // x = 20;  // 错误！值捕获默认是 const
    };

    // 引用捕获：捕获引用，lambda 内修改影响外部
    auto by_ref = [&x, &name]() {
        x = 99;
        name = "Bob";
        std::cout << "引用捕获（修改后）: x=" << x << ", name=" << name << std::endl;
    };

    // mutable：允许修改值捕获的副本（不影响外部）
    auto mutable_lambda = [x]() mutable {
        x = 999; // 修改的是副本
        std::cout << "mutable 内部 x=" << x << std::endl;
    };

    by_value();
    by_ref();
    std::cout << "外部 x=" << x << ", name=" << name << "（已被引用捕获修改）" << std::endl;
    mutable_lambda();
    std::cout << "mutable 后外部 x=" << x << "（未变）" << std::endl;

    // 警告：引用捕获的悬空引用问题
    std::function<void()> dangling;
    {
        int local = 42;
        // dangling = [&local]() { std::cout << local; }; // 危险！local 可能已析构
        dangling = [local]() { std::cout << "安全的值捕获: " << local << std::endl; };
    }
    dangling(); // 值捕获安全，引用捕获会崩溃
}

// ============================================================
// Part 2: std::function 类型擦除
// 可以存储任意可调用对象：lambda、函数指针、仿函数、bind结果
// ============================================================

// 普通函数
int add(int a, int b) { return a + b; }

// 仿函数（函数对象）
struct Multiplier {
    int factor;
    int operator()(int x) const { return x * factor; }
};

void demo_std_function() {
    std::cout << "\n=== std::function 类型擦除 ===" << std::endl;

    // std::function 可以统一存储不同类型的可调用对象
    std::function<int(int, int)> f1 = add;                          // 函数指针
    std::function<int(int, int)> f2 = [](int a, int b) { return a - b; }; // lambda
    std::function<int(int)>      f3 = Multiplier{3};                // 仿函数

    std::cout << "f1(3,4) = " << f1(3, 4) << std::endl; // 7
    std::cout << "f2(10,3) = " << f2(10, 3) << std::endl; // 7
    std::cout << "f3(5) = " << f3(5) << std::endl;       // 15

    // std::bind：绑定参数，生成新的可调用对象
    auto add5 = std::bind(add, std::placeholders::_1, 5);
    std::function<int(int)> f4 = add5;
    std::cout << "add5(10) = " << f4(10) << std::endl; // 15

    // 注意：std::function 有额外开销（类型擦除的代价），性能敏感场合用模板
}

// ============================================================
// Part 3: 回调注册器（EventHandler）—— Reactor 模式的雏形
// 这是你的聊天服务器将要用到的核心设计
// ============================================================

class EventHandler {
public:
    using Callback = std::function<void(int fd, const std::string& data)>;

    // 注册事件回调
    void on(const std::string& event, Callback cb) {
        handlers_[event] = std::move(cb);
    }

    // 触发事件
    void emit(const std::string& event, int fd, const std::string& data) {
        auto it = handlers_.find(event);
        if (it != handlers_.end() && it->second) {
            it->second(fd, data);
        } else {
            std::cout << "[警告] 事件 '" << event << "' 没有注册处理器" << std::endl;
        }
    }

private:
    std::map<std::string, Callback> handlers_;
};

void demo_event_handler() {
    std::cout << "\n=== EventHandler 回调注册器 ===" << std::endl;

    EventHandler handler;

    // 注册连接事件
    handler.on("connect", [](int fd, const std::string& /*data*/) {
        std::cout << "[connect] 新客户端 fd=" << fd << " 已连接" << std::endl;
    });

    // 注册消息事件
    handler.on("message", [](int fd, const std::string& data) {
        std::cout << "[message] fd=" << fd << " 说: " << data << std::endl;
    });

    // 注册断开事件
    std::string server_name = "ChatServer"; // 捕获外部变量
    handler.on("disconnect", [server_name](int fd, const std::string& /*data*/) {
        std::cout << "[" << server_name << "] fd=" << fd << " 已断开" << std::endl;
    });

    // 模拟事件触发
    handler.emit("connect", 42, "");
    handler.emit("message", 42, "你好，世界！");
    handler.emit("message", 43, "hello from client 43");
    handler.emit("disconnect", 42, "");
    handler.emit("unknown", 1, ""); // 没注册的事件
}

// ============================================================
// Part 4: 定时器任务队列（lambda + 时间的实际应用）
// ============================================================

#include <queue>
#include <chrono>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct TimerTask {
    TimePoint when;
    std::function<void()> callback;
    bool operator>(const TimerTask& other) const { return when > other.when; }
};

class Timer {
public:
    void schedule(int delay_ms, std::function<void()> cb) {
        TimerTask task{
            Clock::now() + std::chrono::milliseconds(delay_ms),
            std::move(cb)
        };
        tasks_.push(std::move(task));
    }

    void run_expired() {
        auto now = Clock::now();
        while (!tasks_.empty() && tasks_.top().when <= now) {
            tasks_.top().callback();
            tasks_.pop();
        }
    }

    bool empty() const { return tasks_.empty(); }

private:
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> tasks_;
};

void demo_timer() {
    std::cout << "\n=== 定时器（lambda + 时间） ===" << std::endl;

    Timer timer;
    timer.schedule(0, []() { std::cout << "立即执行的任务" << std::endl; });
    timer.schedule(0, []() { std::cout << "心跳检测：发送 ping" << std::endl; });

    timer.run_expired();
}

// ============================================================
// 自测问题（Day4 结束前回答）
// Q1: 值捕获和引用捕获各适合什么场景？哪种更安全？
// Q2: std::function 比普通函数指针有什么优势？代价是什么？
// Q3: 为什么 callback 参数用 std::move(cb) 而不是直接存储？
// Q4: 写出下面代码的输出：
//     int n = 1;
//     auto f = [n]() mutable { n++; return n; };
//     cout << f() << f() << n;
// ============================================================

int main() {
    demo_lambda_capture();
    demo_std_function();
    demo_event_handler();
    demo_timer();

    std::cout << "\n=== Day4 完成！明天任务：STL高级用法 + 手写LRU缓存 ===" << std::endl;
    return 0;
}
