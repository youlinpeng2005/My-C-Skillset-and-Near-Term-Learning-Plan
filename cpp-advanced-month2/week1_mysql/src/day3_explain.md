# Day3 — EXPLAIN 实战分析（10w 数据量）

**表结构回顾**
- `messages`：`id`(PK), `sender_id`, `receiver_id`, `content`(text), `created_at`
- 已有索引：`PRIMARY KEY(id)`、联合索引 `idx_sender_created(sender_id, created_at)`

---

## 环境准备

```sql
-- 确认数据量
SELECT COUNT(*) FROM messages;
-- 预期：>= 100000

-- 开启慢查询日志（阈值 0.1s）
SET GLOBAL slow_query_log    = ON;
SET GLOBAL long_query_time   = 0.1;
-- 查看日志文件位置
SHOW VARIABLES LIKE 'slow_query_log_file';
```

---

## 一、未使用索引的查询（3 条）

### Q1：全表扫描 — content 模糊匹配

```sql
EXPLAIN SELECT * FROM messages WHERE content LIKE '%hello%';
```

```
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
| id | select_type | table    | type | possible_keys | key  | key_len | ref  | rows   | Extra       |
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
|  1 | SIMPLE      | messages | ALL  | NULL          | NULL | NULL    | NULL | 99452  | Using where |
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
```

**分析**：`type=ALL`，全表扫描约 10w 行。`content` 是 text 列且前缀为 `%`，无法走任何索引，是最差情况。

---

### Q2：全表扫描 — receiver_id 无索引列等值查询

```sql
EXPLAIN SELECT * FROM messages WHERE receiver_id = 5;
```

```
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
| id | select_type | table    | type | possible_keys | key  | key_len | ref  | rows   | Extra       |
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
|  1 | SIMPLE      | messages | ALL  | NULL          | NULL | NULL    | NULL | 99452  | Using where |
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
```

**分析**：`receiver_id` 没有独立索引，`type=ALL`。虽然是等值查询，但 MySQL 无法快速定位，只能逐行过滤。

---

### Q3：全表扫描 — 对索引列做函数运算导致索引失效

```sql
EXPLAIN SELECT * FROM messages WHERE YEAR(created_at) = 2026;
```

```
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
| id | select_type | table    | type | possible_keys | key  | key_len | ref  | rows   | Extra       |
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
|  1 | SIMPLE      | messages | ALL  | NULL          | NULL | NULL    | NULL | 99452  | Using where |
+----+-------------+----------+------+---------------+------+---------+------+--------+-------------+
```

**分析**：`created_at` 在联合索引里，但对其做 `YEAR()` 函数运算后，索引无法使用（函数破坏了 B+ 树有序性）。`type=ALL`，典型的"索引列上加函数"陷阱。

---

## 二、使用索引的查询（3 条）

### Q4：联合索引前缀 — sender_id 等值

```sql
EXPLAIN SELECT * FROM messages WHERE sender_id = 1;
```

```
+----+-------------+----------+------+----------------------+----------------------+---------+-------+-------+-------+
| id | select_type | table    | type | possible_keys        | key                  | key_len | ref   | rows  | Extra |
+----+-------------+----------+------+----------------------+----------------------+---------+-------+-------+-------+
|  1 | SIMPLE      | messages | ref  | idx_sender_created   | idx_sender_created   | 4       | const | 10021 |       |
+----+-------------+----------+------+----------------------+----------------------+---------+-------+-------+-------+
```

**分析**：`type=ref`，命中联合索引第一列。`rows` 约 1w（10 个用户均分 10w 条），比全表扫描少 10 倍。`key_len=4` 表示只用了 `sender_id`（int = 4 bytes）。

---

### Q5：联合索引全列 — sender_id + created_at 范围

```sql
EXPLAIN SELECT * FROM messages
WHERE sender_id = 2 AND created_at >= '2026-01-01';
```

```
+----+-------------+----------+-------+----------------------+----------------------+---------+------+------+---------------------------+
| id | select_type | table    | type  | possible_keys        | key                  | key_len | ref  | rows | Extra                     |
+----+-------------+----------+-------+----------------------+----------------------+---------+------+------+---------------------------+
|  1 | SIMPLE      | messages | range | idx_sender_created   | idx_sender_created   | 9       | NULL | 4823 | Using index condition     |
+----+-------------+----------+-------+----------------------+----------------------+---------+------+------+---------------------------+
```

**分析**：`type=range`，联合索引两列都用上了。`key_len=9`（int 4 + datetime 5）。`Extra: Using index condition` 表示 ICP（Index Condition Pushdown）生效，进一步减少回表次数。

---

### Q6：主键等值 — id 精确查找

```sql
EXPLAIN SELECT * FROM messages WHERE id = 50000;
```

```
+----+-------------+----------+-------+---------------+---------+---------+-------+------+-------+
| id | select_type | table    | type  | possible_keys | key     | key_len | ref   | rows | Extra |
+----+-------------+----------+-------+---------------+---------+---------+-------+------+-------+
|  1 | SIMPLE      | messages | const | PRIMARY       | PRIMARY | 8       | const |    1 |       |
+----+-------------+----------+-------+---------------+---------+---------+-------+------+-------+
```

**分析**：`type=const`，主键等值查找，MySQL 在优化阶段就能确定唯一一行，`rows=1`，是最优情况。

---

## 三、对比汇总

| 编号 | 查询条件 | type  | rows  | key                | 结论           |
|------|----------|-------|-------|--------------------|----------------|
| Q1   | `content LIKE '%hello%'` | ALL   | ~10w  | NULL               | 全表扫，最差   |
| Q2   | `receiver_id = 5`        | ALL   | ~10w  | NULL               | 无索引列，全扫 |
| Q3   | `YEAR(created_at) = 2026`| ALL   | ~10w  | NULL               | 函数破坏索引   |
| Q4   | `sender_id = 1`          | ref   | ~1w   | idx_sender_created | 索引前缀命中   |
| Q5   | `sender_id=2 AND created_at>=...` | range | ~5k | idx_sender_created | 联合索引全用   |
| Q6   | `id = 50000`             | const | 1     | PRIMARY            | 主键精确，最优 |

---

## 四、type 列从差到好的顺序

```
ALL < index < range < ref < eq_ref < const
```

| type    | 含义                                         |
|---------|----------------------------------------------|
| ALL     | 全表扫描，最差，数据量大时必须优化           |
| index   | 扫描整棵索引树（比 ALL 好一点，仍是全扫）    |
| range   | 索引范围扫描（`>`、`<`、`BETWEEN`、`IN`）    |
| ref     | 非唯一索引等值查找，返回多行                 |
| eq_ref  | 唯一索引等值查找，JOIN 时每行最多匹配一行    |
| const   | 主键或唯一索引等值查找，最多一行，最优       |

**实际优化目标**：生产环境中尽量把 `ALL` 消灭，`range` 及以上都是可接受的。

---

## 五、慢查询日志验证

运行 Q1（全表扫描）后，检查慢查询日志：

```bash
# WSL 中查看日志文件
sudo tail -20 /var/log/mysql/mysql-slow.log
```

日志中应出现类似：

```
# Query_time: 0.312  Lock_time: 0.000  Rows_sent: 0  Rows_examined: 100000
SELECT * FROM messages WHERE content LIKE '%hello%';
```

`Rows_examined: 100000` 与 `Rows_sent: 0` 对比，直观反映了全表扫描的代价。
