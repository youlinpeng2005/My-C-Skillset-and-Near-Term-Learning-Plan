// 第一周 Day1：智能指针原理
// 学习目标：理解 shared_ptr 引用计数、weak_ptr 打破循环引用、unique_ptr 独占所有权
// 学习策略：先读懂下面的参考实现，然后在末尾的练习区自己手写 MySharedPtr

#include <iostream>
#include <memory>
#include <string>

// ============================================================
// Part 1: 原理展示 —— shared_ptr 引用计数机制
// ============================================================

void demo_shared_ptr() {
    std::cout << "=== shared_ptr 引用计数演示 ===" << std::endl;

    auto sp1 = std::make_shared<int>(42);
    std::cout << "sp1 创建后，use_count = " << sp1.use_count() << std::endl; // 1

    {
        auto sp2 = sp1; // 拷贝，引用计数 +1
        std::cout << "sp2 拷贝后，use_count = " << sp1.use_count() << std::endl; // 2

        auto sp3 = std::move(sp1); // 移动，sp1 变为 nullptr，计数不变
        std::cout << "sp3 移动后，sp1.use_count = " << sp1.use_count()  // 0（sp1 为空）
                  << "，sp3.use_count = " << sp3.use_count() << std::endl; // 2
    }
    // sp2 sp3 离开作用域，计数归零，内存自动释放
    std::cout << "sp2/sp3 离开作用域后，内存已释放" << std::endl;
}

// ============================================================
// Part 2: 原理展示 —— weak_ptr 打破循环引用
// ============================================================

struct Node {
    std::string name;
    std::shared_ptr<Node> next; // 改成 weak_ptr 即可打破循环
    // std::weak_ptr<Node> next; // 正确做法

    ~Node() { std::cout << name << " 被析构" << std::endl; }
};

void demo_cycle_leak() {
    std::cout << "\n=== 循环引用导致内存泄漏 ===" << std::endl;
    auto a = std::make_shared<Node>(Node{"A", nullptr});
    auto b = std::make_shared<Node>(Node{"B", nullptr});
    a->next = b;
    b->next = a; // 循环引用！a 和 b 都无法析构
    std::cout << "函数结束，但 A 和 B 不会被析构（泄漏）" << std::endl;
}

struct SafeNode {
    std::string name;
    std::weak_ptr<SafeNode> next; // weak_ptr 不增加引用计数

    ~SafeNode() { std::cout << name << " 被安全析构" << std::endl; }
};

void demo_weak_ptr() {
    std::cout << "\n=== weak_ptr 打破循环引用 ===" << std::endl;
    auto a = std::make_shared<SafeNode>(SafeNode{"A"});
    auto b = std::make_shared<SafeNode>(SafeNode{"B"});
    a->next = b;
    b->next = a;
    std::cout << "函数结束，A 和 B 会正常析构：" << std::endl;
    // 使用 weak_ptr 时需要先 lock()
    if (auto sp = a->next.lock()) {
        std::cout << "a->next 指向: " << sp->name << std::endl;
    }
}

// ============================================================
// Part 3: 原理展示 —— unique_ptr 独占所有权
// ============================================================

void demo_unique_ptr() {
    std::cout << "\n=== unique_ptr 独占所有权 ===" << std::endl;
    auto up1 = std::make_unique<int>(100);
    std::cout << "up1 = " << *up1 << std::endl;

    // auto up2 = up1;  // 编译错误！unique_ptr 不能拷贝
    auto up2 = std::move(up1); // 只能移动，up1 变为 nullptr
    std::cout << "move 后 up1 是否为空: " << (up1 == nullptr ? "是" : "否") << std::endl;
    std::cout << "up2 = " << *up2 << std::endl;
}

// ============================================================
// Part 4: 参考实现 —— 简化版 MySharedPtr
// 先读懂这个实现，再去练习区自己写一遍！
// ============================================================

template <typename T>
class MySharedPtr {
public:
    // 构造：分配引用计数
    explicit MySharedPtr(T* ptr = nullptr)
        : ptr_(ptr), ref_count_(ptr ? new int(1) : nullptr) {}

    // 拷贝构造：共享引用计数并 +1
    MySharedPtr(const MySharedPtr& other)
        : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        if (ref_count_) ++(*ref_count_);
    }

    // 移动构造：接管资源，原指针置空
    MySharedPtr(MySharedPtr&& other) noexcept
        : ptr_(other.ptr_), ref_count_(other.ref_count_) {
        other.ptr_ = nullptr;
        other.ref_count_ = nullptr;
    }

    // 析构：引用计数 -1，归零则释放
    ~MySharedPtr() { release(); }

    // 拷贝赋值
    MySharedPtr& operator=(const MySharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            ref_count_ = other.ref_count_;
            if (ref_count_) ++(*ref_count_);
        }
        return *this;
    }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }
    int use_count() const { return ref_count_ ? *ref_count_ : 0; }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    void release() {
        if (ref_count_ && --(*ref_count_) == 0) {
            delete ptr_;
            delete ref_count_;
        }
        ptr_ = nullptr;
        ref_count_ = nullptr;
    }

    T* ptr_;
    int* ref_count_; // 注意：真实实现是 atomic<int>，这里简化
};

void demo_my_shared_ptr() {
    std::cout << "\n=== MySharedPtr 测试 ===" << std::endl;
    MySharedPtr<int> sp1(new int(42));
    std::cout << "sp1 use_count = " << sp1.use_count() << std::endl; // 1

    MySharedPtr<int> sp2 = sp1;
    std::cout << "sp2 拷贝后 use_count = " << sp1.use_count() << std::endl; // 2

    {
        MySharedPtr<int> sp3 = std::move(sp2);
        std::cout << "sp3 移动后 use_count = " << sp1.use_count() << std::endl; // 2
    }
    std::cout << "sp3 析构后 use_count = " << sp1.use_count() << std::endl; // 1
}

// ============================================================
// 自测问题（Day1 结束前回答）
// Q1: shared_ptr 本身是线程安全的吗？引用计数操作是线程安全的吗？
// Q2: MySharedPtr 的 ref_count_ 为什么在真实实现中需要用 atomic<int>？
// Q3: weak_ptr 如何知道对象是否还存活？（提示：control block）
// ============================================================

int main() {
    demo_shared_ptr();
    demo_cycle_leak();
    demo_weak_ptr();
    demo_unique_ptr();
    demo_my_shared_ptr();

    std::cout << "\n=== Day1 完成！明天任务：RAII + unique_ptr 改写项目裸指针 ===" << std::endl;
    return 0;
}

// 自测问题
//A1:shared_ptr 的引用计数操作是线程安全的（真实实现用 atomic）。但 shared_ptr 对象本身不是线程安全的——多个线程同时对同一个 shared_ptr 变量执行读写（比如一个线程在拷贝，另一个线程在 reset），会有数据竞争，需要加锁

//A2:ref_count_ 存在堆上的 control block 里，被所有共享该对象的 shared_ptr 同时访问。多线程下并发执行 ++ 或 -- 是非原子操作，会产生竞态条件（race condition）。用 atomic<int> 可以让自增/自减成为一条不可中断的硬件指令，既解决了并发问题，又避免了 mutex 的重量级开销。

//A3:control block 里维护两个计数：强引用计数（strong_count） 和 弱引用计数（weak_count）。
//当 strong_count == 0 时，托管对象被销毁（调用析构函数/delete）
//当 strong_count == 0 && weak_count == 0 时，control block 本身才被释放
//weak_ptr::lock() 的本质就是：检查 strong_count > 0，若是则原子地 +1 并返回一个有效的 shared_ptr，否则返回空。

// ============================================================
// 练习区：自己实现后在这里填写，不要直接看上面的参考
// 目标：实现 MySharedPtr<T>，要求：
//   1. 构造/拷贝构造/移动构造/析构
//   2. use_count() 返回引用计数
//   3. operator* 和 operator->
// ============================================================

template <typename T>
class MySharedPtr {
    public:
        
        explicit MySharedPtr(T* ptr = nullptr)
            : ptr_(ptr), ref_count_(ptr ? new int(1) : nullptr) {}
    
        
        MySharedPtr(const MySharedPtr& other)
            : ptr_(other.ptr_), ref_count_(other.ref_count_) {
            if (ref_count_) ++(*ref_count_);
        }
    
        
        MySharedPtr(MySharedPtr&& other) noexcept
            : ptr_(other.ptr_), ref_count_(other.ref_count_) {
            other.ptr_ = nullptr;
            other.ref_count_ = nullptr;
        }
    
       
        ~MySharedPtr() { release(); }
    
       
        MySharedPtr& operator=(const MySharedPtr& other) {
            if (this != &other) {
                release();
                ptr_ = other.ptr_;
                ref_count_ = other.ref_count_;
                if (ref_count_) ++(*ref_count_);
            }
            return *this;
        }
    
        T& operator*() const { return *ptr_; }
        T* operator->() const { return ptr_; }
        T* get() const { return ptr_; }
        int use_count() const { return ref_count_ ? *ref_count_ : 0; }
        explicit operator bool() const { return ptr_ != nullptr; }
    
    private:
        void release() {
            if (ref_count_ && --(*ref_count_) == 0) {
                delete ptr_;
                delete ref_count_;
            }
            ptr_ = nullptr;
            ref_count_ = nullptr;
        }
    
        T* ptr_;
        int* ref_count_; 
    };