# Week3 计划：chat_server 重构 + 登录注册系统

**时间**：6/8 (周一) ~ 6/14 (周日)
**主题**：把 month1 的扁平 chat_server 拆成清晰的分层架构，并接上 Week1 的 MySQL + Week2 的 Redis 实现完整的用户系统

> **本计划为大纲版**。Week2 结束（6/7）后细化为 day 级任务卡。

---

## 本周里程碑（周日 6/14 验收）

- [ ] 项目目录拆成清晰的 5 层：`base/` `net/` `db/` `cache/` `business/`
- [ ] 实现注册 / 登录 / 登出 三个业务接口，全部走 MySQL + Redis
- [ ] 用户密码用 bcrypt 哈希存储（绝不能明文）
- [ ] 登录态用 Redis 维护 `session_id → user_id` 映射，TTL 24h
- [ ] 在线列表用 Redis SET 维护，断连自动移除
- [ ] 写一份 `ARCHITECTURE.md` 包含分层架构图 + 每层职责说明
- [ ] WEEK3_SUMMARY.md ≥ 100 行 + INTERVIEW_NOTES 第 3 周章节追加 ≥ 5 条

---

## 七天主题大纲（6/7 细化）

| Day | 主题 | 难度 | 核心产出 |
|-----|-----|-----|---------|
| Day 15 (6/8 周一) | 架构设计：画出 v2 分层图 + 写 ARCHITECTURE.md | ★★★ | ARCHITECTURE.md v1 |
| Day 16 (6/9 周二) | 目录重构 + 多模块 CMake（拆成 5 个 library） | ★★★★ | 新目录结构 + 能编译通过 |
| Day 17 (6/10 周三) | 协议扩展：定义 REGISTER / LOGIN / LOGOUT 消息类型 | ★★★ | 协议头扩展 + 序列化 |
| Day 18 (6/11 周四) | 业务层：UserService（注册/登录/登出）+ bcrypt | ★★★★ | UserService.cpp + 单测 |
| Day 19 (6/12 周五) | 接入 chat_server：注册/登录走完整链路 | ★★★★★ | 端到端跑通 |
| Day 20 (6/13 周六) | 在线列表 + session 校验 + 中间件思想 | ★★★★ | 在线广播 + 鉴权拦截 |
| Day 21 (6/14 周日) | Buffer + WEEK3_SUMMARY + ARCHITECTURE.md 定稿 | ★★ | SUMMARY + 5 条面试题 |

---

## 目标分层结构

```
src/
├── base/        # 日志、线程池、阻塞队列（沿用 month1）
├── net/         # epoll、Channel、Acceptor、Connection、Protocol（沿用 month1）
├── db/          # MySQL 连接池、UserDao、MessageDao（Week1 产出）
├── cache/       # RedisClient、SessionCache、OnlineUsers（Week2 产出）
└── business/    # UserService、ChatService、Router（Week3 重点）
```

---

## DoD / commit / 落后预警

> 与 Week1 一致，6/7 细化时复制过来。
