// 第一周 Day5：STL高级用法 + 手写LRU缓存
// 学习目标：理解迭代器失效、哈希冲突、手写 LRU 缓存（面试高频 + 后端实用）
// 重点：LRU = 双向链表（维护顺序）+ unordered_map（O(1)查找）

#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <stdexcept>

// ============================================================
// Part 1: 迭代器失效规则（必须掌握，避免 UB）
// ============================================================

void demo_iterator_invalidation() {
    std::cout << "=== 迭代器失效规则 ===" << std::endl;

    // vector：在中间插入/删除，该位置之后的迭代器全部失效
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto it = v.begin() + 2; // 指向 3
    std::cout << "插入前 *it = " << *it << std::endl;
    v.insert(v.begin(), 0);  // 头部插入，导致所有迭代器失效！
    // std::cout << *it;  // UB! 不要这样用

    // 正确做法：插入后重新获取迭代器
    it = v.begin() + 3; // 重新定位
    std::cout << "重新定位后 *it = " << *it << std::endl;

    // list：插入/删除不影响其他元素的迭代器（list 的优势）
    std::list<int> lst = {1, 2, 3, 4, 5};
    auto lst_it = std::next(lst.begin(), 2); // 指向 3
    lst.insert(lst.begin(), 0); // 头部插入
    std::cout << "list 插入后 *lst_it = " << *lst_it << "（迭代器仍然有效）" << std::endl;

    // 循环中删除 vector 元素的正确写法
    std::vector<int> nums = {1, 2, 3, 4, 5, 6};
    // 删除所有偶数（正确写法）
    for (auto it2 = nums.begin(); it2 != nums.end(); ) {
        if (*it2 % 2 == 0) {
            it2 = nums.erase(it2); // erase 返回下一个有效迭代器
        } else {
            ++it2;
        }
    }
    std::cout << "删除偶数后: ";
    for (int n : nums) std::cout << n << " ";
    std::cout << std::endl;
}

// ============================================================
// Part 2: unordered_map 哈希冲突与性能
// ============================================================

void demo_unordered_map() {
    std::cout << "\n=== unordered_map 性能特性 ===" << std::endl;

    std::unordered_map<std::string, int> umap;
    umap.reserve(100); // 预分配桶，减少 rehash

    umap["alice"] = 1;
    umap["bob"] = 2;
    umap["charlie"] = 3;

    // O(1) 查找（平均），O(n) 最坏（哈希碰撞严重时）
    auto it = umap.find("bob");
    if (it != umap.end()) {
        std::cout << "找到 bob，值 = " << it->second << std::endl;
    }

    // 注意：operator[] 会插入默认值（可能导致意外扩容）
    std::cout << "访问不存在的键 'dave': " << umap["dave"] << std::endl; // 插入了默认值 0
    std::cout << "umap 大小（变成4了）: " << umap.size() << std::endl;

    // 安全查找：用 find 或 count
    if (umap.count("dave")) {
        std::cout << "dave 存在" << std::endl;
    }
}

// ============================================================
// Part 3: LRU 缓存实现（核心！）
// LRU = Least Recently Used
// 原理：最近使用的放链表头，淘汰时删除链表尾
// 结构：list<pair<key,value>> + unordered_map<key, list::iterator>
// ============================================================

template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(int capacity) : capacity_(capacity) {
        if (capacity <= 0) throw std::invalid_argument("capacity must be > 0");
    }

    // 获取：O(1)
    V get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            throw std::out_of_range("key not found");
        }
        // 将访问的节点移到链表头（最近使用）
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    // 插入/更新：O(1)
    void put(const K& key, const V& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            // 已存在：更新值并移到链表头
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }

        // 超出容量：淘汰链表尾（最久未使用）
        if ((int)list_.size() >= capacity_) {
            auto last = list_.back();
            map_.erase(last.first);
            list_.pop_back();
            std::cout << "[LRU 淘汰] key=" << last.first << std::endl;
        }

        // 插入新节点到链表头
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    bool contains(const K& key) const {
        return map_.count(key) > 0;
    }

    int size() const { return (int)list_.size(); }

    void print() const {
        std::cout << "LRU 状态（头=最近，尾=最久）: ";
        for (auto& [k, v] : list_) {
            std::cout << k << ":" << v << " -> ";
        }
        std::cout << "NULL" << std::endl;
    }

private:
    int capacity_;
    // list 维护访问顺序，存储 <key, value>
    std::list<std::pair<K, V>> list_;
    // map 存储 key 到 list 迭代器的映射，实现 O(1) 访问
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
};

void demo_lru_cache() {
    std::cout << "\n=== LRU 缓存测试 ===" << std::endl;

    LRUCache<int, std::string> cache(3); // 容量为 3

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    cache.print(); // 3:three -> 2:two -> 1:one

    cache.get(1);  // 访问 1，移到头部
    cache.print(); // 1:one -> 3:three -> 2:two

    cache.put(4, "four"); // 容量满，淘汰尾部（2）
    cache.print(); // 4:four -> 1:one -> 3:three

    std::cout << "contains(2): " << (cache.contains(2) ? "yes" : "no") << std::endl; // no
    std::cout << "contains(3): " << (cache.contains(3) ? "yes" : "no") << std::endl; // yes

    cache.put(1, "ONE"); // 更新已存在的 key
    cache.print(); // 1:ONE -> 4:four -> 3:three
}

// ============================================================
// Part 4: 优先队列（堆）用法 —— 后续心跳定时器要用
// ============================================================

#include <queue>
#include <chrono>

void demo_priority_queue() {
    std::cout << "\n=== 优先队列（小根堆）===" << std::endl;

    // 默认大根堆，用 greater<int> 变成小根堆
    std::priority_queue<int, std::vector<int>, std::greater<int>> min_heap;

    min_heap.push(5);
    min_heap.push(1);
    min_heap.push(3);
    min_heap.push(2);
    min_heap.push(4);

    std::cout << "按升序出队: ";
    while (!min_heap.empty()) {
        std::cout << min_heap.top() << " ";
        min_heap.pop();
    }
    std::cout << std::endl;
}

// ============================================================
// 自测问题（Day5 结束前回答）
// Q1: LRU 中为什么用 list 而不是 vector 来维护顺序？
// Q2: list::splice 的时间复杂度是多少？它做了什么？
// Q3: 如何把 LRUCache 改成线程安全的版本？
// Q4: unordered_map 的负载因子是什么？默认值是多少？
// ============================================================

Q1：因为 LRU 需要频繁进行“中间删除 + 头部插入”，list 的节点删除和插入只需要修改指针，时间复杂度是 O(1)，而 vector 中间删除会导致后续元素整体移动，复杂度是 O(n)，不适合高频淘汰场景。

Q2：list::splice 的时间复杂度通常是 O(1)（单节点或已知区间情况下），它本质上是把链表节点从一个位置“摘下来”再挂到另一个位置，不会拷贝或移动元素内容，只调整节点指针，因此效率很高。

Q3：可以在 get() 和 put() 等会访问共享数据的地方加 std::mutex 进行互斥保护，常见做法是用 std::lock_guard<std::mutex> 自动加锁解锁，保证 unordered_map 和 list 不会被多个线程同时修改导致数据竞争。

Q4：负载因子（load factor）表示 元素个数 / 桶数（bucket_count），反映哈希表的拥挤程度。默认最大负载因子是 1.0，超过这个值时，unordered_map 通常会自动进行 rehash 扩容。

int main() {
    demo_iterator_invalidation();
    demo_unordered_map();
    demo_lru_cache();
    demo_priority_queue();

    std::cout << "\n=== Day5 完成！明天任务：模板编程 + 泛型线程安全队列 ===" << std::endl;
    return 0;
}
