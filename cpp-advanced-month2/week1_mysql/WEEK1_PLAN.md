# Week1 计划：MySQL 工程化 + C++ 接入

**时间**：5/25 (周一) ~ 5/31 (周日)
**主题**：把 MySQL 从"会写 SELECT"升级到"会设计索引、看 EXPLAIN、用 C++ 写连接池"

---

## 本周里程碑（周日验收）

- [ ] 能在白板画出 B+ 树结构，解释为什么不用 B 树 / 红黑树 / Hash
- [ ] 能口述 4 种事务隔离级别 + 各自能防的并发问题
- [ ] 能看懂 EXPLAIN 输出，能用 EXPLAIN 给一条慢 SQL 出优化建议
- [ ] 写出能跑通的 C++ MySQL 连接池（≥ 5 个连接，RAII 获取释放）
- [ ] 建好 `chat_db` 库，设计好 `users` 表（含合理索引），用 C++ 完成 CRUD
- [ ] WEEK1_SUMMARY.md ≥ 100 行
- [ ] INTERVIEW_NOTES.md 第 1 周章节追加 ≥ 5 条

---

## 每日任务卡

> 格式：耗时 + 难度 (★~★★★★★) + DoD 三条 + 产出文件

---

### Day 1 (周一 5/25) — SQL 复习 + chat_db 设计

- **耗时**：2 ~ 3 h
- **难度**：★★
- **任务**：
  1. 快速过《MySQL 必知必会》前 10 章（如果熟练可直接跳到下面）
  2. 在 WSL MySQL 里建 `chat_db` 库，设计 3 张表的 schema：
     - `users` (id, username, password_hash, nickname, created_at, last_login_at)
     - `messages` (id, sender_id, receiver_id, content, created_at)（暂存全部聊天记录）
     - `friends` (user_id, friend_id, status, created_at)
  3. 想清楚每张表**主键**、**唯一约束**、**外键** 该怎么设
- **DoD**：
  1. `src/day1_schema.sql` 文件能直接 `mysql -u root chat_db < day1_schema.sql` 跑通
  2. 用 DBeaver 看到 3 张表结构正确
  3. 写 `src/day1_notes.md`：解释每张表为什么这样设计（≥ 30 行）
- **产出**：`src/day1_schema.sql`、`src/day1_notes.md`

---

### Day 2 (周二 5/26) — 索引原理 + B+ 树

- **耗时**：2 ~ 3 h
- **难度**：★★★★
- **任务**：
  1. 读《高性能 MySQL》第 5 章 5.1 ~ 5.3 节（索引基础 + B-Tree 索引）
  2. 给 `users` 表加索引：`username` 唯一索引、`last_login_at` 普通索引
  3. 给 `messages` 表加联合索引 `(sender_id, created_at)`，想清楚为什么这样建
  4. 写 `day2_index.md`：画 B+ 树查找过程、对比聚簇 vs 非聚簇
- **DoD**：
  1. 索引建好，`SHOW INDEX FROM users` 能看到
  2. `day2_index.md` ≥ 50 行，画出 B+ 树查找示意
  3. 能口头回答："为什么联合索引顺序要把区分度高的放前面？"
- **产出**：`src/day2_add_indexes.sql`、`src/day2_index.md`

---

### Day 3 (周三 5/27) — EXPLAIN + 慢查询定位

- **耗时**：2 ~ 3 h
- **难度**：★★★★
- **任务**：
  1. 往 `messages` 表插入 10w+ 条假数据（用脚本批量 INSERT 或 stored procedure）
  2. 写 3 条故意没用索引的查询，再写 3 条用了索引的查询
  3. 对比两组 EXPLAIN 输出，重点看 `type` / `key` / `rows` / `Extra` 列
  4. 开启慢查询日志：`SET GLOBAL slow_query_log = ON; SET GLOBAL long_query_time = 0.1;`
- **DoD**：
  1. `src/day3_data_gen.sql` 能生成 10w 条假数据
  2. `src/day3_explain.md` 包含 6 个查询 + 6 份 EXPLAIN 截图/文本
  3. 能解释 type 列从最差到最好的顺序（ALL < index < range < ref < eq_ref < const）
- **产出**：`src/day3_data_gen.sql`、`src/day3_explain.md`

---

### Day 4 (周四 5/28) — 事务与隔离级别

- **耗时**：2 ~ 3 h
- **难度**：★★★★
- **任务**：
  1. 读《高性能 MySQL》第 1 章 1.3 节（事务）
  2. 开两个 mysql cli 窗口，分别演示 4 种隔离级别下的：
     - 脏读（DIRTY READ）
     - 不可重复读（NON-REPEATABLE READ）
     - 幻读（PHANTOM READ）
  3. 写 `day4_transaction.md` 记录每个实验的 SQL 序列 + 现象
- **DoD**：
  1. 4 种隔离级别都亲手切过：`SET SESSION TRANSACTION ISOLATION LEVEL ...`
  2. `day4_transaction.md` ≥ 60 行，每种现象有 SQL 复现步骤
  3. 能讲清 MVCC 是什么 + InnoDB 为什么默认 RR 而不是 RC
- **产出**：`src/day4_transaction.md`

---

### Day 5 (周五 5/29) — C++ 接入 MySQL（裸客户端版）

- **耗时**：2 ~ 3 h
- **难度**：★★★
- **任务**：
  1. 选库：`libmysqlclient`（C API）或 `MariaDB Connector/C++`（C++ API，推荐）
  2. 写 `day5_mysql_demo.cpp`：连接 chat_db，对 users 表做完整 CRUD
  3. 写 `CMakeLists.txt`，能链接 MySQL 客户端库
  4. **重点**：用 RAII 封装 Connection（构造连接、析构断开），杜绝忘记 close
- **DoD**：
  1. 程序能跑通 CRUD，DBeaver 能看到数据变化
  2. CMakeLists 是工程级写法（`find_package` 或 `pkg_check_modules`）
  3. 能讲清"prepared statement vs 拼字符串 SQL"的安全性差异（SQL 注入）
- **产出**：`src/day5_mysql_demo.cpp`、`src/CMakeLists.txt`

---

### Day 6 (周六 5/30) — 手写 MySQL 连接池

- **耗时**：6 ~ 8 h（周末加大投入，本周最重要的一天）
- **难度**：★★★★★
- **任务**：
  1. 设计 `ConnectionPool` 类（参考 month1 的 ThreadPool 思想）：
     - 启动时预创建 N 个连接
     - `acquire()` 阻塞获取一个连接（用 `condition_variable`）
     - `release()` 归还连接（用 RAII guard：析构自动归还）
     - 支持 idle 连接超时回收（可选）
  2. 写 unit test（不用框架，手写 main 函数测）：
     - 单线程拿+还
     - 多线程并发抢
     - 池满时 acquire 阻塞、release 后唤醒
  3. **思考题**：连接池大小定多少合适？为什么不是越大越好？（提示：`Hikari CP wiki: about-pool-sizing`）
- **DoD**：
  1. `src/day6_conn_pool.h` + `.cpp` 实现完整，无内存泄漏（valgrind 验证）
  2. 多线程压测：100 个线程抢 10 个连接，不死锁不崩
  3. `day6_summary.md` 解释连接池大小选型公式：`pool_size = ((core_count * 2) + effective_spindle_count)` 来源
- **产出**：`src/day6_conn_pool.h`、`src/day6_conn_pool.cpp`、`src/day6_conn_pool_test.cpp`、`src/day6_summary.md`

---

### Day 7 (周日 5/31) — Buffer 日 + WEEK1_SUMMARY

- **耗时**：6 ~ 8 h
- **难度**：★★★
- **任务**：
  1. **如果前 6 天准时完成**：把 day6 连接池接入 month1 chat_server，让 chat_server 启动时初始化 MySQL 连接池（为 week3 重构铺路）
  2. **如果前 6 天落后**：用今天追赶（限时一天）
  3. 写 `WEEK1_SUMMARY.md`（必交，结构参考 month1 WEEK1_SUMMARY）
  4. 往 `../INTERVIEW_NOTES.md` 第 1 周章节追加 ≥ 5 条面试题（已经留了 4 个 placeholder）
  5. 更新 `../README.md` 进度追踪表
- **DoD**：
  1. WEEK1_SUMMARY.md ≥ 100 行，含本周路线图 + 5 个最重要知识点 + 5 条面试题
  2. INTERVIEW_NOTES 第 1 周章节至少 5 条，每条都有要点回答
  3. README.md 进度表 Week1 标记为 ✅
- **产出**：`WEEK1_SUMMARY.md`、更新两个全局 md

---

## 本周 git commit 节奏

```
day1 → git commit -m "feat(month2-week1): add day1 chat_db schema design"
day2 → git commit -m "feat(month2-week1): day2 indexes + b+ tree notes"
day3 → git commit -m "feat(month2-week1): day3 explain analysis on 100k rows"
day4 → git commit -m "docs(month2-week1): day4 transaction isolation hands-on"
day5 → git commit -m "feat(month2-week1): day5 mysql c++ client CRUD demo"
day6 → git commit -m "feat(month2-week1): day6 mysql connection pool, thread-safe"
day7 → git commit -m "docs(month2-week1): WEEK1_SUMMARY, interview notes +5"
```

> **规则**：当天任务必须当天 commit，否则视为未完成（沿用 leader 监督规则）。

---

## 落后预警机制

| 触发条件 | 处理 |
|---------|------|
| 任何一天 24:00 前没 commit | 第二天晨会必须先解释原因，不接受"等一下" |
| 连续 2 天未 commit | 当周 buffer 日（day7）取消休息，全部用于追赶 |
| 周日仍未补齐 | 本周状态标 ⚠️ 延期，下周减新内容 1 天用来补 |

---

## 本周学习记录区（每天填）

### Day 1 完成情况
- 完成时间：
- 实际耗时：
- 遇到的卡点：
- 待续追问：

### Day 2 ~ Day 7（同上格式）
