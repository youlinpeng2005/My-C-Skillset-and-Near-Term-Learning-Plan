## List

### 什么是List
本质是有序可重复两端可插入删除的消息队列类似于c++里的deque

### 常用指令
1. LPUSH
2. RPUSH
3. LPOP
4. RPOP

## Set
### 什么是SET
本质是无需不可重复自动去重的哈希表

### 常用指令
1. SADD
2. SMEMBERS（查看）
3. SREM(删除)
4. SISMEMBER(判断存在)

## ZSet
### 什么是ZSet
本质是哈希映射，key->value

### 常用指令
1. ZADD
2. ZRANGE(查看)
3. ZREVRANGE（倒序查看）
4. ZINCRBY(增加)
5. ZREVRANK(查看单个对象)

### 实战
```redis
RPUSH task_queue task1
RPUSH task_queue task2
RPUSH task_queue task3

LRANGE task_queue 0 -1

LPOP task_queue

LRANGE task_queue 0 -1
```

```
SADD online_users 1001
SADD online_users 1002
SADD online_users 1003

SMEMBERS online_users

SISMEMBER online_users 1002

SCARD online_users

SREM online_users 1002

SMEMBERS online_users
```

```
ZADD rank 100 user1
ZADD rank 90 user2
ZADD rank 80 user3

ZRANGE rank 0 -1 WITHSCORES

ZREVRANGE rank 0 -1 WITHSCORES

ZINCRBY rank 20 user2

ZREVRANGE rank 0 -1 WITHSCORES

ZREVRANK rank user2
```