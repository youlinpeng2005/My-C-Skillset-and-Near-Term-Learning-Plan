# Day5 笔记 — C++ 接入 MySQL（裸客户端版）

**日期**：2026-05-28  
**耗时**：约 2 h  
**产出**：`day5_mysql_demo.cpp`、`CMakeLists.txt`

---

## 运行结果

```
[Connection] connected to chat_db @ 127.0.0.1:3306

--- CREATE ---
[INSERT] new user id = 1

--- READ ---
[SELECT] id=1 username=alice nickname=Alice in Wonderland created_at=2026-05-28 14:28:19

--- UPDATE ---
[UPDATE] affected rows: 1
[SELECT] id=1 username=alice nickname=Alice Updated created_at=2026-05-28 14:28:19

--- DELETE ---
[DELETE] affected rows: 1
[SELECT] no row found for id=1
[Connection] disconnected
```

CRUD 全链路跑通，DBeaver 可观察到数据变化（最终 DELETE 后行消失）。

---

## 核心知识点

### 1. libmariadb C API 连接生命线

```
mysql_init()          // 分配 MYSQL 句柄
  └─ mysql_real_connect()  // 建立 TCP 连接
       └─ mysql_close()    // 释放连接（必须调用）
```

- `mysql_library_init()` / `mysql_library_end()`：进程级初始化，多线程程序必须在主线程调用一次
- `mysql_options(conn, MYSQL_OPT_RECONNECT, &true)`：连接断开后自动重连
- `mysql_set_character_set(conn, "utf8mb4")`：统一字符集，避免中文乱码

### 2. RAII 封装 Connection

```cpp
class Connection {
public:
    Connection(...) { conn_ = mysql_init(); mysql_real_connect(...); }
    ~Connection()   { if (conn_) mysql_close(conn_); }

    Connection(const Connection&) = delete;             // 禁止拷贝
    Connection& operator=(const Connection&) = delete;  // 禁止赋值
    Connection(Connection&&) noexcept;                  // 允许移动
};
```

**为什么用 RAII**：MySQL 连接是昂贵的系统资源（TCP + 服务器线程），忘记 `mysql_close` 会导致：
- 服务端连接数泄漏（`max_connections` 耗尽后新连接被拒）
- 客户端文件描述符泄漏

RAII 让析构函数兜底，异常路径也不会漏关。

### 3. Prepared Statement vs 拼字符串 SQL

#### 拼字符串（危险）

```cpp
// 危险写法：username 来自用户输入
std::string sql = "SELECT * FROM users WHERE username = '" + username + "'";
mysql_query(conn, sql.c_str());
```

如果用户输入 `' OR '1'='1`，最终 SQL 变成：

```sql
SELECT * FROM users WHERE username = '' OR '1'='1'
```

查询返回所有用户，绕过了身份验证——这就是 **SQL 注入**。

#### Prepared Statement（安全）

```cpp
const char* sql = "SELECT * FROM users WHERE username = ?";
mysql_stmt_prepare(stmt, sql, -1);   // SQL 结构固定，服务端预编译
// 绑定参数（数据与 SQL 结构分离传输）
mysql_stmt_bind_param(stmt, bind);
mysql_stmt_execute(stmt);
```

**为什么安全**：参数通过独立的二进制协议传输，服务端把它当纯数据处理，不会被解析为 SQL 语法，注入攻击失效。

**额外收益**：同一条 SQL 多次执行时，服务端只编译一次，性能更好。

---

## 遇到的问题与解决

### 问题 1：`build.sh` 在 WSL 里报 `$'\r': command not found`

**原因**：Windows 写文件默认用 `CRLF`（`\r\n`）行尾，WSL bash 把 `\r` 当成命令的一部分导致解析失败。

**解决**：不用 `build.sh`，直接手动在 WSL 终端里执行 cmake + make：
```bash
mkdir build && cd build && cmake .. && make
```
或者用 `dos2unix build.sh` 转换行尾后再执行。

### 问题 2：`mysql/mysql.h: No such file or directory`

**原因**：`libmariadb-dev` 安装后头文件路径是 `/usr/include/mariadb/mysql.h`，不是 `mysql/mysql.h`。  
CMakeLists 里用 `pkg_check_modules(MARIADB REQUIRED libmariadb)` 找到的 include 路径是 `/usr/include/mariadb`，所以 `#include` 要写 `<mariadb/mysql.h>`。

**解决**：把源码里的 `#include <mysql/mysql.h>` 改为 `#include <mariadb/mysql.h>`。

### 问题 3：`cd build` 找不到目录（`$'\r'` 污染路径）

**原因**：从 Cursor 聊天框复制多行命令粘贴到 WSL 时，`\r` 被附加到命令末尾，`mkdir build\r` 创建的目录名带了 `^M`，后续 `cd build`（不带 `^M`）自然找不到。

**解决**：用单行一次性命令避免多行粘贴：
```bash
SRC=<路径> && cd "$SRC" && rm -rf build* && mkdir build && cd build && cmake .. && make
```

---

## CMakeLists.txt 要点

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

target_include_directories(day5_mysql_demo PRIVATE ${MARIADB_INCLUDE_DIRS})
target_link_libraries(day5_mysql_demo PRIVATE ${MARIADB_LIBRARIES})
```

`pkg_check_modules` 会自动从 `/usr/lib/pkgconfig/libmariadb.pc` 读取 include 路径和链接参数，比硬编码路径更工程化，换机器不用改 CMakeLists。

---

## DoD 自检

- [x] 程序跑通 CRUD，DBeaver 可观察数据变化
- [x] CMakeLists 使用 `pkg_check_modules`，工程级写法
- [x] 能讲清 prepared statement vs 拼接 SQL 的安全性差异（见上文 SQL 注入部分）
