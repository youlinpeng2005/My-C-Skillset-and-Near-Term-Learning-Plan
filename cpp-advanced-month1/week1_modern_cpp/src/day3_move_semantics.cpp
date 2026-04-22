// 第一周 Day3：move语义 + 右值引用
// 学习目标：彻底搞清楚左值/右值/将亡值，理解 move 语义为什么能提升性能
// 重点：std::move 本身不移动任何东西，它只是类型转换

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <utility>

// ============================================================
// Part 1: 左值 vs 右值 vs 将亡值
// ============================================================

void demo_value_categories() {
    std::cout << "=== 值类别理解 ===" << std::endl;

    int a = 10;       // a 是左值（有名字，有地址）
    int b = a + 5;    // (a + 5) 是右值（临时值，没有名字）
    // int& ref1 = a + 5;   // 错误！不能用左值引用绑定右值
    const int& ref2 = a + 5; // 正确：const 左值引用可以绑定右值（生命期延长）
    int&& rref = a + 5;      // 右值引用：专门绑定右值

    std::cout << "左值引用 ref2 = " << ref2 << std::endl;
    std::cout << "右值引用 rref = " << rref << std::endl;
    // 注意：rref 本身是左值！（有名字，有地址）
}

// ============================================================
// Part 2: 移动构造 vs 拷贝构造 —— 看清楚区别
// ============================================================

class BigData {
public:
    explicit BigData(size_t size) : size_(size), data_(new int[size]) {
        std::cout << "[构造] 分配 " << size_ << " 个 int" << std::endl;
    }

    // 拷贝构造：深拷贝，需要分配新内存
    BigData(const BigData& other) : size_(other.size_), data_(new int[other.size_]) {
        std::copy(other.data_, other.data_ + other.size_, data_);
        std::cout << "[拷贝构造] 深拷贝 " << size_ << " 个 int（慢！）" << std::endl;
    }

    // 移动构造：直接"偷走"资源，不分配新内存
    BigData(BigData&& other) noexcept
        : size_(other.size_), data_(other.data_) {
        other.data_ = nullptr; // 原对象放弃所有权
        other.size_ = 0;
        std::cout << "[移动构造] 转移指针，无内存分配（快！）" << std::endl;
    }

    ~BigData() {
        delete[] data_;
    }

    size_t size() const { return size_; }

private:
    size_t size_;
    int* data_;
};

void demo_copy_vs_move() {
    std::cout << "\n=== 拷贝 vs 移动 ===" << std::endl;

    BigData d1(1000000);

    std::cout << "--- 拷贝 ---" << std::endl;
    BigData d2 = d1;  // 调用拷贝构造

    std::cout << "--- 移动 ---" << std::endl;
    BigData d3 = std::move(d1);  // 调用移动构造，d1 之后不可用
    // d1 被移动后：data_ == nullptr，size_ == 0
    std::cout << "d1.size() after move = " << d1.size() << "（已被清空）" << std::endl;
}

// ============================================================
// Part 3: vector push_back 中 move 的实际效果
// ============================================================

void demo_vector_move() {
    std::cout << "\n=== vector + move 性能对比 ===" << std::endl;

    std::vector<std::string> v;
    v.reserve(3);

    std::string s1 = "hello world, this is a long string to avoid SSO";
    std::string s2 = "another long string for testing move semantics here";

    // 拷贝：s1 仍然有效
    v.push_back(s1);
    std::cout << "拷贝后 s1 仍有效: " << s1.substr(0, 5) << "..." << std::endl;

    // 移动：s2 变为空
    v.push_back(std::move(s2));
    std::cout << "移动后 s2 = \"" << s2 << "\"（已被置空）" << std::endl;

    // 直接构造（最优）：原地构造，无拷贝无移动
    v.emplace_back("emplace_back 直接原地构造，最高效");
    std::cout << "v 中元素数量: " << v.size() << std::endl;
}

// ============================================================
// Part 4: 完美转发 std::forward（了解概念）
// 在线程池的 submit 函数中会用到
// ============================================================

// 辅助函数：区分左值和右值
void process(int& x)  { std::cout << "处理左值: " << x << std::endl; }
void process(int&& x) { std::cout << "处理右值: " << x << std::endl; }

// 不用 forward：会丢失右值属性
template<typename T>
void bad_relay(T&& arg) {
    process(arg); // arg 是具名变量，永远当做左值传递！
}

// 用 forward：保留原始的左值/右值属性
template<typename T>
void good_relay(T&& arg) {
    process(std::forward<T>(arg)); // 完美转发
}

void demo_perfect_forward() {
    std::cout << "\n=== 完美转发 ===" << std::endl;
    int x = 42;

    bad_relay(x);         // 传左值 -> 始终调用左值版本
    bad_relay(100);       // 传右值 -> 仍调用左值版本（错误！）

    good_relay(x);        // 传左值 -> 调用左值版本（正确）
    good_relay(100);      // 传右值 -> 调用右值版本（正确）
}

// ============================================================
// Part 5: 实现一个支持 move 的简单 Buffer 类（动手练习）
// 先不看，自己实现后再对照
// ============================================================

class Buffer {
public:
    explicit Buffer(size_t capacity)
        : capacity_(capacity), size_(0), data_(new char[capacity]) {
        std::cout << "Buffer 构造，容量 = " << capacity_ << std::endl;
    }

    // TODO：自己实现拷贝构造
    Buffer(const Buffer& other)
        : capacity_(other.capacity_), size_(other.size_),
          data_(new char[other.capacity_]) {
        std::copy(other.data_, other.data_ + other.size_, data_);
    }

    // TODO：自己实现移动构造
    Buffer(Buffer&& other) noexcept
        : capacity_(other.capacity_), size_(other.size_), data_(other.data_) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    ~Buffer() { delete[] data_; }

    void append(const char* src, size_t len) {
        if (size_ + len > capacity_) throw std::runtime_error("Buffer overflow");
        std::copy(src, src + len, data_ + size_);
        size_ += len;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    size_t size_;
    char* data_;
};

void demo_buffer() {
    std::cout << "\n=== Buffer 移动测试 ===" << std::endl;
    Buffer b1(1024);
    std::string msg = "hello";
    b1.append(msg.c_str(), msg.size());
    std::cout << "b1.size() = " << b1.size() << std::endl;

    Buffer b2 = std::move(b1);
    std::cout << "移动后 b2.size() = " << b2.size()
              << "，b1.capacity() = " << b1.capacity() << std::endl;
}

// ============================================================
// 自测问题（Day3 结束前回答）
// Q1: std::move 做了什么？它会移动数据吗？
// Q2: 移动后的对象处于什么状态？能否继续使用？
// Q3: 为什么移动构造要加 noexcept？（提示：vector 扩容时的行为）
// Q4: 什么是"五法则"（Rule of Five）？
// ============================================================

int main() {
    demo_value_categories();
    demo_copy_vs_move();
    demo_vector_move();
    demo_perfect_forward();
    demo_buffer();

    std::cout << "\n=== Day3 完成！明天任务：lambda + std::function + bind ===" << std::endl;
    return 0;
}

Buffer(const Buffer& other)
    : capacity_(other.capacity_), size_(other.size),data(new char[other.capacity_])
    {
        std::copy(other.data,other.data + other.size,data);
    }

Buffer(Buffer&& other) noexcept
    : capacity_(other.capacity_), size_(other.size), data(other.data)
{
    other.data = nullptr;
    other.capacity_ = 0;
    other.size_ = 0;
}