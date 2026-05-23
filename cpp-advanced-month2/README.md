# C++ 进阶第二个月学习代码库

**时间**：2026 年 5 月 25 日 ~ 6 月 21 日（共 4 周 28 天）
**主题**：工程化与后端能力（MySQL 工程化 + Redis 引入 + 项目结构重构）
**对标总纲**：[`../c++进阶.md`](../c++进阶.md) 第 2 个月计划
**目标**：把 month1 写的 chat_server 从"功能服务器"升级为"具备登录注册 + 缓存 + 异步日志 + 模块分层"的**企业级后端项目**

---

## 立项背景

month1 完成了 `epoll + 线程池 + 自定义协议` 的应用层聊天服务器，但它只是个**裸功能服务器**——没用户系统、没数据库、没缓存、目录里所有文件堆在一起。

month2 按 `c++进阶.md` 第二个月的规划走，三个核心目标：

1. **MySQL 工程化**：不是会写 SQL 就完了，要懂索引设计、慢查询定位、事务隔离级别，并用 C++ 客户端把它接入 chat_server 做用户注册/登录
2. **Redis 引入**：用 Redis 做会话缓存、在线列表、限流，理解"缓存 + DB"双写一致性
3. **项目结构重构**：把 month1 的扁平目录重构成清晰的分层架构（net / db / cache / business / base），写工程级 CMake，加完整异步日志

月底产出物：
- 一个有 MySQL + Redis 接入、支持注册/登录/在线列表的 chat_server v2
- 一份"如果让面试官看，能看 30 分钟"的项目 README + 架构图
- 60+ 条面试题累积（MySQL/Redis/项目设计三个方向）

---

## 目录结构

```
cpp-advanced-month2/
├── README.md                            # 本文件：总进度 + 里程碑
├── INTERVIEW_NOTES.md                   # 月底累积 ≥ 60 条面试题（每周强制追加）
│
├── week1_mysql/                         # 5/25 ~ 5/31  MySQL 工程化 + C++ 接入
│   ├── WEEK1_PLAN.md                    # 每日任务卡 + DoD
│   ├── WEEK1_SUMMARY.md                 # 周末必写
│   └── src/  day1 ~ day7
│
├── week2_redis/                         # 6/01 ~ 6/07  Redis 入门 + 应用 + hiredis 接入
│   ├── WEEK2_PLAN.md
│   ├── WEEK2_SUMMARY.md
│   └── src/  day8 ~ day14
│
├── week3_chatserver_refactor/           # 6/08 ~ 6/14  chat_server 重构 + 登录注册
│   ├── WEEK3_PLAN.md
│   ├── WEEK3_SUMMARY.md
│   ├── ARCHITECTURE.md                  # 重构后的分层架构图 + 决策记录
│   └── src/  day15 ~ day21
│
└── week4_integration/                   # 6/15 ~ 6/21  完整集成 + 日志增强 + 压测
    ├── WEEK4_PLAN.md
    ├── WEEK4_SUMMARY.md
    ├── PROJECT_README.md                # chat_server v2 的最终对外文档（可贴简历）
    └── src/  day22 ~ day28
```

---

## 四周主题与里程碑

| 周次 | 计划日期       | 主题                                    | 里程碑（DoD） |
|-----|--------------|----------------------------------------|--------------|
| 第1周 | 5/25 ~ 5/31 | **MySQL 工程化** + C++ 接入             | 能口述索引底层 (B+ 树) / 事务四种隔离级别 / EXPLAIN 三大关键字段；写一个 C++ MySQL 连接池 |
| 第2周 | 6/01 ~ 6/07 | **Redis 入门** + 缓存/分布式锁/排行榜    | 能讲清 5 大数据类型实战场景 + 缓存三大问题；hiredis 封装 RedisClient 可用 |
| 第3周 | 6/08 ~ 6/14 | **chat_server 重构** + 登录注册系统      | 项目分 5 层目录；chat_server 支持注册/登录/在线列表，全部走 MySQL + Redis |
| 第4周 | 6/15 ~ 6/21 | **完整集成** + 异步日志增强 + 压测       | 压测 200 并发跑通完整业务流；产出可贴简历的 PROJECT_README.md |

每周末（周日）必产出 `WEEKx_SUMMARY.md`，不少于 100 行，否则视为本周未达标。

---

## month1 复盘 → month2 强约束（沿用）

| month1 问题 | month2 改进 |
|---------|---------|
| 任务"完成"标准过软 | 每个 day 卡片必须有 **DoD 三条**：①代码跑通 ②通过 N 个测试/压测 ③能讲清 3 个面试题 |
| Week2/3 没有 SUMMARY.md | **每周日必出 SUMMARY.md ≥ 100 行**，写不出 = 没学透 = 本周不算完成 |
| 进度滑坡无缓冲 | 每周第 7 天明确为 **buffer 日**，准时则做精读 / 不准时则追赶 |
| day 任务粒度不均 | 每个 day 标注 **预计耗时 + 难度 (★~★★★★★)** |
| 面试知识点零散 | 根目录 `INTERVIEW_NOTES.md` **每周强制追加 ≥ 5 条**，月底累积 ≥ 60 条 |

---

## 学习进度追踪

| 周   | 计划日期       | 实际执行日期      | 主题              | 状态   |
|-----|--------------|--------------|-----------------|------|
| 第1周 | 5/25 ~ 5/31  | -            | MySQL 工程化     | ⏳ 待开始 |
| 第2周 | 6/01 ~ 6/07  | -            | Redis 入门与应用 | ⏳ 待开始 |
| 第3周 | 6/08 ~ 6/14  | -            | chat_server 重构 | ⏳ 待开始 |
| 第4周 | 6/15 ~ 6/21  | -            | 完整集成 + 压测   | ⏳ 待开始 |

> 状态图例：⏳ 待开始 / 🔥 进行中 / ✅ 已完成 / ⚠️ 延期

---

## chat_server v1 → v2 升级路线（贯穿月2 全程）

```
v1（month1 产出）                  v2（month2 月底交付）
─────────────────                  ──────────────────────────
epoll + 线程池                  →   epoll + 线程池（保留）
自定义协议 + 粘包               →   自定义协议（扩展 LOGIN/REGISTER 消息）
无用户系统                      →   MySQL 用户表 + bcrypt 密码哈希
无会话管理                      →   Redis 存 session_id → user_id
无在线列表                      →   Redis SET 维护在线用户
日志（简单异步）                →   日志（分级 + 滚动 + 异步队列）
扁平目录                        →   分层目录（base/net/db/cache/business）
单一 CMakeLists                 →   多模块 CMakeLists + 单元测试
压测 200 并发 / X QPS          →   压测 200 并发完整业务流 / Y QPS
```

---

## 投入时间估算

- 工作日 2 ~ 3 h × 5 天 = 12.5 h
- 周末 6 ~ 8 h × 2 天 = 14 h
- 单周合计 ≈ 26 h，月总投入 ≈ 104 h

---

## 关键参考资料

### MySQL 方向（Week1）
- **《MySQL 必知必会》** Ben Forta：快速过一遍 SQL（如果已熟练可跳）
- **《高性能 MySQL》第 3 版** 第 5 章（索引）+ 第 6 章（查询优化）：必读
- **MySQL 官方文档 `EXPLAIN` 章节**：背下来 type 列的取值含义（ALL/index/range/ref/eq_ref/const）
- **C++ 客户端选型**：`libmysqlclient` 或 `MariaDB Connector/C++`（推荐后者，API 更现代）

### Redis 方向（Week2）
- **《Redis 设计与实现》** 黄健宏：第 1~9 章（数据结构 + 编码）
- **Redis 官方文档 commands 页**：5 大类型每个挑 5 个最常用命令背
- **hiredis 库**：https://github.com/redis/hiredis，自己封装一层 RedisClient
- **《Redis 实战》** Josiah Carlson：第 4 章（缓存）+ 第 5 章（计数器/排行榜）

### 项目重构（Week3~4）
- **muduo 源码** 的整体目录结构：参考 base/net/http 分层方式
- **CMake Cookbook**（O'Reilly）第 1、3、5 章：写出 add_library + target_link_libraries 的工程级 CMake

---

## 环境准备清单（5/24 必须完成）

> **5/24（周日）**必须搞定，否则 day1 卡环境：

- [ ] WSL2 Ubuntu 22.04 安装 MySQL 8.0：`sudo apt install mysql-server`
- [ ] 启动 MySQL：`sudo service mysql start`；设置 root 密码；建一个练习用的库 `chat_db`
- [ ] 安装 MySQL C++ 客户端开发包：`sudo apt install libmysqlclient-dev`（或 mariadb-connector）
- [ ] 安装 Redis：`sudo apt install redis-server`；`redis-cli ping` 返回 PONG
- [ ] 安装 hiredis：`sudo apt install libhiredis-dev`
- [ ] 安装可视化客户端（可选但强烈推荐）：Windows 上装 **DBeaver**（MySQL）和 **Another Redis Desktop Manager**（Redis），方便看数据
- [ ] 在 `cpp-advanced-month2/scripts/check_env.sh` 写一个脚本，自动检查上面所有项

---

## git commit 规范（沿用 month1）

```
feat(month2-week1):  add day1 sql basics and chat_db schema
feat(month2-week2):  redis hot-key cache for user info
docs(month2-week3):  WEEK3_SUMMARY chat_server v2 architecture
feat(month2-week4):  async logger rotate, stress test 200 conn pass
```

---

## 第二个月的"职场承诺"

> 我在 7~8 月投递简历前，能给面试官讲清楚：
> 1. 我项目里的 user 表为什么这样设计索引，EXPLAIN 输出怎么读
> 2. 为什么用 Redis 做 session，如果用 MySQL 做有什么问题
> 3. 缓存穿透/雪崩/击穿在我的项目里有没有，怎么防的
> 4. 我的 chat_server v2 是怎么分层的，每层职责是什么
>
> 如果月底任何一条讲不清，month2 算失败。
