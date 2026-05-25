---
month: 2
week: 1
current_day: 3
last_completed_day: 2
last_update: 2026-05-25
status: in_progress
next_task: "Day3 — 往 messages 插 10w 假数据 + EXPLAIN 实战"
plan_file: "cpp-advanced-month2/week1_mysql/WEEK1_PLAN.md"
---

# 学习进度真相源（PROGRESS.md）

> **本文件是单一进度真相源**（Single Source of Truth）。
> - 🟢 **新会话开始时必读**：先读本文件再读其他任何文档
> - 🟢 **完成 day 任务时必更新**：完成当日任务并 commit 后，立即更新本文件
> - 🟢 **frontmatter 是机器字段**，请保持字段名不变；下方表格人类阅读
> - 🟢 不要把"长期计划"塞进这里——长期计划在 `cpp-advanced-month1/2/3/README.md`

---

## 🎯 当前状态（一眼速览）

| 字段 | 值 |
|---|---|
| 当前月份 | **month2**（MySQL + Redis + chat_server 重构） |
| 当前周 | **Week1 — MySQL 工程化**（5/25 ~ 5/31） |
| 当前 day | **Day3**（2026/05/27 周三） |
| 上次完成的 day | Day2（2026/05/25，commit 4446a23） |
| 下一个待办 | Day3 — 往 messages 插 10w 假数据 + EXPLAIN 实战 |
| 详细任务卡 | [`cpp-advanced-month2/week1_mysql/WEEK1_PLAN.md`](cpp-advanced-month2/week1_mysql/WEEK1_PLAN.md) |
| 当前阻塞 | 无 |

---

## 📊 三个月总览

| 月 | 时间 | 主题 | 状态 |
|---|---|---|---|
| month1 | 4/19 ~ 5/23 | 现代C++ + epoll + 并发 + 工程级聊天服务器 | ✅ 已完成（实际 5/23 收官，比计划晚 5 天） |
| month2 | 5/25 ~ 6/21 | MySQL + Redis + chat_server v2 重构 | 🔥 进行中（Day2） |
| month3 | 6/22 ~ 7/19 | 完整 IM 系统（分布式 / 高级特性） | ⏳ 待规划 |

---

## 📋 month2 周进度

| 周 | 时间 | 主题 | 完成 day | 状态 |
|---|---|---|---|---|
| Week1 | 5/25 ~ 5/31 | MySQL 工程化 + 连接池 | 2 / 7 | 🔥 进行中 |
| Week2 | 6/01 ~ 6/07 | Redis 入门 + hiredis 封装 | 0 / 7 | ⏳ 待开始 |
| Week3 | 6/08 ~ 6/14 | chat_server 重构 + 登录注册 | 0 / 7 | ⏳ 待开始 |
| Week4 | 6/15 ~ 6/21 | 完整集成 + 日志增强 + 压测 | 0 / 7 | ⏳ 待开始 |

---

## 📝 最近 5 次完成记录

> 倒序排列，最新的在最上面。每条格式：`YYYY-MM-DD | 标签 | 简述 | commit hash`

- 2026-05-25 | day2 | 索引原理 + B+树 + 聚簇/非聚簇索引笔记 | 4446a23
- 2026-05-25 | day1 | chat_db 建库建表 3 张，schema 设计说明 | c35358a

---

## 🚧 当前阻塞 / 备忘

> 暂无

---

## 🔄 如何更新本文件（操作清单）

完成一个 day 任务后，按顺序：

1. 先把代码 / 笔记 commit 到 git（**不能跳过**，未 commit = 未完成）
2. 在"📝 最近 5 次完成记录"顶部插入一行：
   `2026-MM-DD | day{N} | <主题> | <commit short hash>`
3. 更新 frontmatter：`last_completed_day`、`current_day`、`last_update`、`next_task`
4. 更新"📋 month2 周进度"表里的 `完成 day` 计数
5. 如果当前周完成（写完 WEEKx_SUMMARY）：把对应周状态改为 ✅，下一周改 🔥
6. 把 PROGRESS.md 一起 commit：`docs(progress): day{N} done, next day{N+1}`

---

## 📅 接下来 3 天预告

| 日期 | 计划 |
|---|---|
| 2026-05-25 周一 | Day1：建 chat_db + 设计 3 张表 schema |
| 2026-05-26 周二 | Day2：索引原理 + 给 users / messages 表加索引 |
| 2026-05-27 周三 | Day3：往 messages 插 10w 假数据 + EXPLAIN 实战 |

---

> **历史归档**：month1 完成详情见 [`cpp-advanced-month1/README.md`](cpp-advanced-month1/README.md)
