1. Redis 基础 + String/Hash 实战 

什么是redis：
redis本质是：一个运行在内存中的 Key-Value 数据库

```
username -> youlinpeng

user:1 -> {
    name: 张三,
    age: 18
}
```

与mysql区别
| MySQL | Redis     |
| ----- | --------- |
| 磁盘数据库 | 内存数据库     |
| 表结构   | Key-Value |
| SQL   | Redis命令   |
| 慢一些   | 非常快       |

redis快的原因
1 内存结构，redis从cpu访问内存
2 单线程模型
```cpp
while(true)
{
    epoll_wait();

    处理命令;
}
```
期间不用加锁
3 io多路复用

redis五大数据类型
String ——————>单个值
Hash ——————>对象
List——————>链表
Set——————>集合
ZSet——————>带分数排序集合

redis部分指令
- SET(存或覆盖数据)
```
SET name youlinpeng
```

- GET(查看结果)
```
GET name
```

- DEL(删除)
```
DEL name
```

- EXISTS(判断是否存在)
```
EXISTS name
返回1存在，0不存在
```
- INCR(数字自增)
```
SET views 100
INCR views
结果为101
```

- EXPIRE(设置过期时间)
```
SET code 123456
EXPIRE code 60
TTL code
60秒后自动删除。

一条命令设置
SET code 123456 EX 60
```

String与hash实战

hash结构类似于unordered_map<string,string>
user:1001
 ├─ name = 张三
 ├─ age = 18
 └─ city = 西安

```redis
SET name youlinpeng
GET name

SET views 100
INCR views
INCR views
GET views

SET code 123456 EX 60
TTL code

HSET user:1001 name 张三
HSET user:1001 age 18
HSET user:1001 city 西安

HGET user:1001 name

HGETALL user:1001

HEXISTS user:1001 age

HDEL user:1001 city

HGETALL user:1001
```

