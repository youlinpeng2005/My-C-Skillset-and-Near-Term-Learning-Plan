# Week2 计划：Redis 入门 + 应用 + hiredis 接入

**时间**：6/1 (周一) ~ 6/7 (周日)
**主题**：从"会用 redis-cli"到"能在 chat_server 里用 Redis 做缓存/会话/在线列表"

> **本计划为大纲版**。Week1 结束（5/31）后，根据实际进度细化为 day 级任务卡，避免提前定死后期被迫返工。

---

## 本周里程碑（周日 6/7 验收）

- [ ] 5 大基础数据类型每个能讲 ≥ 1 个真实业务场景
- [ ] 能口述缓存穿透 / 雪崩 / 击穿 三大问题 + 解决方案
- [ ] 自己封装一个 `RedisClient` C++ 类（基于 hiredis），支持连接复用
- [ ] 用 Redis 实现一个"分布式锁"小 demo，能讲清 SETNX 的坑
- [ ] WEEK2_SUMMARY.md ≥ 100 行 + INTERVIEW_NOTES 第 2 周章节追加 ≥ 5 条

---

## 七天主题大纲（5/31 细化为 day 卡）

| Day | 主题 | 难度 | 核心产出 |
|-----|-----|-----|---------|
| Day 8  (6/1 周一) | Redis 基础 + String/Hash 实战 | ★★ | 5 大类型笔记 + 一组 redis-cli 演示脚本 |
| Day 9  (6/2 周二) | List/Set/ZSet 实战（消息队列 / 在线列表 / 排行榜） | ★★★ | 3 个业务场景代码 |
| Day 10 (6/3 周三) | 持久化（RDB / AOF）+ 过期策略 + 内存淘汰 | ★★★ | 配置文件解读 + 笔记 |
| Day 11 (6/4 周四) | hiredis C 库入门 + 同步/异步 API | ★★★ | hiredis demo |
| Day 12 (6/5 周五) | 封装 `RedisClient` C++ 类（连接池 + RAII） | ★★★★★ | RedisClient.h/.cpp + 单测 |
| Day 13 (6/6 周六) | 缓存三大问题 + 分布式锁 demo | ★★★★ | 锁 demo + 三问题对策代码 |
| Day 14 (6/7 周日) | Buffer + WEEK2_SUMMARY + 面试题累积 | ★★ | SUMMARY + 5 条面试题 |

---

## 关键参考

- 《Redis 设计与实现》黄健宏：第 1~9 章
- hiredis: https://github.com/redis/hiredis
- 《Redis 实战》第 4、5 章（缓存 + 计数器）

---

## DoD / commit / 落后预警机制

> 与 Week1 完全一致，5/31 细化时复制过来。
