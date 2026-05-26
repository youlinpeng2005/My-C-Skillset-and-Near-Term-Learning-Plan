# Day4 — 事务与隔离级别实战

## 一、四种隔离级别速览

| 隔离级别             | 脏读 | 不可重复读 | 幻读 |
|----------------------|------|------------|------|
| READ UNCOMMITTED     | ✅可能 | ✅可能   | ✅可能 |
| READ COMMITTED       | ❌防住 | ✅可能   | ✅可能 |
| REPEATABLE READ (默认)| ❌防住 | ❌防住   | ⚠️理论可能，InnoDB 用 Gap Lock 基本防住 |
| SERIALIZABLE         | ❌防住 | ❌防住   | ❌防住 |

切换命令：
```sql
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
-- 查看当前级别
SELECT @@transaction_isolation;
```

---

## 二、实验一：脏读（DIRTY READ）

**触发条件**：READ UNCOMMITTED

**现象**：事务 A 读到了事务 B 尚未提交的数据，若 B 随后回滚，A 读到的就是"脏数据"。

### SQL 序列

```sql
-- === 窗口 A ===
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
START TRANSACTION;
-- 此时先不操作，等 B 修改后再读

-- === 窗口 B ===
SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
START TRANSACTION;
UPDATE users SET nickname = 'DIRTY_VALUE' WHERE id = 1;
-- 注意：B 还没有 COMMIT

-- === 窗口 A ===
SELECT nickname FROM users WHERE id = 1;
-- 结果：DIRTY_VALUE  ← 读到了 B 未提交的数据，这就是脏读

-- === 窗口 B ===
ROLLBACK;  -- B 回滚了

-- === 窗口 A ===
SELECT nickname FROM users WHERE id = 1;
-- 结果：回到原值    ← A 之前读到的 DIRTY_VALUE 是不存在的数据

-- 清理
ROLLBACK;
```

**现象说明**：A 在 B 回滚前读到了 `DIRTY_VALUE`，但这行数据最终并不存在，产生了脏读。

---

## 三、实验二：不可重复读（NON-REPEATABLE READ）

**触发条件**：READ COMMITTED

**现象**：同一事务内，两次读同一行，结果不同（另一个事务在两次读之间提交了修改）。

### SQL 序列

```sql
-- === 窗口 A ===
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
START TRANSACTION;
SELECT nickname FROM users WHERE id = 1;
-- 结果：Alice（第一次读）

-- === 窗口 B ===
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
START TRANSACTION;
UPDATE users SET nickname = 'Alice_Modified' WHERE id = 1;
COMMIT;  -- B 已提交

-- === 窗口 A ===
SELECT nickname FROM users WHERE id = 1;
-- 结果：Alice_Modified（第二次读，同一事务内结果变了）← 不可重复读

ROLLBACK;

-- 恢复数据
UPDATE users SET nickname = 'Alice' WHERE id = 1;
```

**现象说明**：A 的事务内两次 SELECT 同一行，结果不同。RC 级别下每次读都获取最新快照，无法保证同一事务内读一致性。

---

## 四、实验三：幻读（PHANTOM READ）

**触发条件**：READ COMMITTED（甚至 REPEATABLE READ 的特定场景）

**现象**：同一事务内，两次范围查询行数不同（另一个事务在两次查询之间插入了新行并提交）。

### SQL 序列

```sql
-- === 窗口 A ===
SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED;
START TRANSACTION;
SELECT COUNT(*) FROM messages WHERE sender_id = 1;
-- 结果：假设是 10021 行（第一次范围查询）

-- === 窗口 B ===
START TRANSACTION;
INSERT INTO messages (sender_id, receiver_id, content) VALUES (1, 2, 'phantom row');
COMMIT;

-- === 窗口 A ===
SELECT COUNT(*) FROM messages WHERE sender_id = 1;
-- 结果：10022 行（多了一行）← 幻读，范围查询结果集变了

ROLLBACK;
```

**现象说明**：A 的范围查询结果集前后不一致，多出了 B 新插入的行，这就是幻读。

---

## 五、实验四：REPEATABLE READ 防止不可重复读

**验证 InnoDB 默认 RR 级别的快照读行为**

```sql
-- === 窗口 A ===
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION;
SELECT nickname FROM users WHERE id = 1;
-- 结果：Alice（建立快照）

-- === 窗口 B ===
START TRANSACTION;
UPDATE users SET nickname = 'Alice_RR_Test' WHERE id = 1;
COMMIT;

-- === 窗口 A ===
SELECT nickname FROM users WHERE id = 1;
-- 结果：Alice（依然是快照时的值，不可重复读被防住了）

COMMIT;

-- 查看真实值（新事务才能看到最新）
SELECT nickname FROM users WHERE id = 1;
-- 结果：Alice_RR_Test

-- 恢复
UPDATE users SET nickname = 'Alice' WHERE id = 1;
```

**现象说明**：RR 级别下，事务开启时创建 Read View（快照），事务内所有快照读都基于该时间点，其他事务的提交对本事务不可见。

---

## 六、MVCC 是什么

MVCC（Multi-Version Concurrency Control，多版本并发控制）是 InnoDB 实现事务隔离的底层机制，**不加锁就能做到读写并发**。

### 核心数据结构

每行数据在 InnoDB 内部实际存储了额外的隐藏列：

| 隐藏列 | 含义 |
|--------|------|
| `DB_TRX_ID` | 最后修改该行的事务 ID |
| `DB_ROLL_PTR` | 指向 undo log 中上一个版本的指针 |
| `DB_ROW_ID` | 行 ID（无主键时自动生成） |

### Read View（读视图）

事务开启时（RC 是每次 SELECT，RR 是第一次 SELECT），生成一个 Read View，记录：
- `m_ids`：当前活跃（未提交）的事务 ID 列表
- `min_trx_id`：活跃事务中最小的 ID
- `max_trx_id`：下一个将分配的事务 ID

读取某行时，沿 `DB_ROLL_PTR` 链回溯 undo log，找到第一个对本 Read View 可见的版本（即 `DB_TRX_ID < min_trx_id` 或不在 `m_ids` 中），返回该版本。

### 可见性判断

```
行版本的 trx_id < min_trx_id    → 该版本在 Read View 创建前已提交，可见
行版本的 trx_id >= max_trx_id   → 该版本在 Read View 创建后才开启，不可见
行版本的 trx_id 在 m_ids 中     → 该版本是活跃事务修改的，不可见（读旧版本）
```

---

## 七、InnoDB 为什么默认 RR 而不是 RC

**历史原因（Binlog 格式）**

MySQL 早期 binlog 默认是 `STATEMENT` 格式（记录 SQL 语句），在 RC 级别下，并发执行相同 SQL 的顺序可能与主库不同，导致**主从数据不一致**。

RR 级别配合 Gap Lock（间隙锁），能保证同一事务内的执行结果是确定的，replaying 到从库时结果一致。

**现状**

现代 MySQL 的 binlog 已普遍使用 `ROW` 格式（记录行变更），从技术上 RC 也能保证主从一致。但 InnoDB 沿用 RR 作为默认值，原因是：

1. **兼容性**：大量存量应用依赖 RR 的快照读语义
2. **更强的一致性保证**：RR 防止了不可重复读，业务代码写起来更简单
3. **Gap Lock 防幻读**：对于当前读（`SELECT ... FOR UPDATE`），RR + Gap Lock 能防止幻读，而 RC 不加 Gap Lock

> RC 的优势是并发更高（Gap Lock 少，死锁概率低），在互联网高并发读多写少场景下不少公司选择 RC，这是合理的权衡。
