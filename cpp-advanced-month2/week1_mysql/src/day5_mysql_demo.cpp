/**
 * Day5 — C++ 接入 MySQL（裸客户端版）
 *
 * 知识点：
 *   - libmariadb / libmysqlclient C API
 *   - RAII 封装 Connection（构造连接、析构断开）
 *   - Prepared Statement 防 SQL 注入
 *   - users 表完整 CRUD
 */

#include <mariadb/mysql.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// ──────────────────────────────────────────────
// RAII Connection 封装
// ──────────────────────────────────────────────
class Connection {
public:
    Connection(const char* host, const char* user,
               const char* password, const char* db,
               unsigned int port = 3306)
    {
        conn_ = mysql_init(nullptr);
        if (!conn_) {
            throw std::runtime_error("mysql_init failed");
        }

        // 自动重连
        bool reconnect = true;
        mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

        if (!mysql_real_connect(conn_, host, user, password,
                                db, port, nullptr, 0)) {
            std::string err = mysql_error(conn_);
            mysql_close(conn_);
            conn_ = nullptr;
            throw std::runtime_error("mysql_real_connect failed: " + err);
        }

        // 统一使用 UTF-8
        mysql_set_character_set(conn_, "utf8mb4");
        std::cout << "[Connection] connected to " << db
                  << " @ " << host << ":" << port << "\n";
    }

    ~Connection() {
        if (conn_) {
            mysql_close(conn_);
            std::cout << "[Connection] disconnected\n";
        }
    }

    // 禁止拷贝，允许移动
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept : conn_(other.conn_) {
        other.conn_ = nullptr;
    }

    MYSQL* raw() { return conn_; }

    // 执行无返回集的 SQL（INSERT / UPDATE / DELETE / DDL）
    void execute(const std::string& sql) {
        if (mysql_query(conn_, sql.c_str()) != 0) {
            throw std::runtime_error("execute failed: " +
                                     std::string(mysql_error(conn_)) +
                                     "\nSQL: " + sql);
        }
    }

private:
    MYSQL* conn_ = nullptr;
};

// ──────────────────────────────────────────────
// CRUD 操作（均使用 Prepared Statement）
// ──────────────────────────────────────────────

// CREATE：插入一条用户记录，返回新行的 id
unsigned long long insertUser(Connection& conn,
                              const std::string& username,
                              const std::string& password_hash,
                              const std::string& nickname)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn.raw());
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    const char* sql =
        "INSERT INTO users (username, password_hash, nickname) "
        "VALUES (?, ?, ?)";

    if (mysql_stmt_prepare(stmt, sql, -1) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt prepare failed: " + err);
    }

    MYSQL_BIND bind[3] = {};

    bind[0].buffer_type   = MYSQL_TYPE_STRING;
    bind[0].buffer        = const_cast<char*>(username.c_str());
    bind[0].buffer_length = username.size();

    bind[1].buffer_type   = MYSQL_TYPE_STRING;
    bind[1].buffer        = const_cast<char*>(password_hash.c_str());
    bind[1].buffer_length = password_hash.size();

    bind[2].buffer_type   = MYSQL_TYPE_STRING;
    bind[2].buffer        = const_cast<char*>(nickname.c_str());
    bind[2].buffer_length = nickname.size();

    mysql_stmt_bind_param(stmt, bind);

    if (mysql_stmt_execute(stmt) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt execute failed: " + err);
    }

    unsigned long long last_id = mysql_stmt_insert_id(stmt);
    mysql_stmt_close(stmt);
    return last_id;
}

// READ：按 id 查询单个用户，打印结果
void selectUser(Connection& conn, unsigned long long id) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn.raw());
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    const char* sql =
        "SELECT id, username, nickname, created_at FROM users WHERE id = ?";

    if (mysql_stmt_prepare(stmt, sql, -1) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt prepare failed: " + err);
    }

    // 绑定参数
    MYSQL_BIND param[1] = {};
    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer      = &id;
    mysql_stmt_bind_param(stmt, param);

    // 绑定结果
    char username[128] = {}, nickname[128] = {}, created_at[32] = {};
    unsigned long long row_id = 0;
    unsigned long username_len = 0, nickname_len = 0, created_at_len = 0;

    MYSQL_BIND result[4] = {};
    result[0].buffer_type   = MYSQL_TYPE_LONGLONG;
    result[0].buffer        = &row_id;

    result[1].buffer_type   = MYSQL_TYPE_STRING;
    result[1].buffer        = username;
    result[1].buffer_length = sizeof(username);
    result[1].length        = &username_len;

    result[2].buffer_type   = MYSQL_TYPE_STRING;
    result[2].buffer        = nickname;
    result[2].buffer_length = sizeof(nickname);
    result[2].length        = &nickname_len;

    result[3].buffer_type   = MYSQL_TYPE_STRING;
    result[3].buffer        = created_at;
    result[3].buffer_length = sizeof(created_at);
    result[3].length        = &created_at_len;

    mysql_stmt_bind_result(stmt, result);

    if (mysql_stmt_execute(stmt) != 0 ||
        mysql_stmt_store_result(stmt) != 0)
    {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt execute/store failed: " + err);
    }

    if (mysql_stmt_fetch(stmt) == 0) {
        std::cout << "[SELECT] id=" << row_id
                  << " username=" << username
                  << " nickname=" << nickname
                  << " created_at=" << created_at << "\n";
    } else {
        std::cout << "[SELECT] no row found for id=" << id << "\n";
    }

    mysql_stmt_close(stmt);
}

// UPDATE：修改 nickname
void updateNickname(Connection& conn,
                    unsigned long long id,
                    const std::string& new_nickname)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn.raw());
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    const char* sql = "UPDATE users SET nickname = ? WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, -1) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt prepare failed: " + err);
    }

    MYSQL_BIND bind[2] = {};
    bind[0].buffer_type   = MYSQL_TYPE_STRING;
    bind[0].buffer        = const_cast<char*>(new_nickname.c_str());
    bind[0].buffer_length = new_nickname.size();

    bind[1].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[1].buffer      = &id;

    mysql_stmt_bind_param(stmt, bind);

    if (mysql_stmt_execute(stmt) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt execute failed: " + err);
    }

    std::cout << "[UPDATE] affected rows: "
              << mysql_stmt_affected_rows(stmt) << "\n";
    mysql_stmt_close(stmt);
}

// DELETE：按 id 删除
void deleteUser(Connection& conn, unsigned long long id) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn.raw());
    if (!stmt) throw std::runtime_error("mysql_stmt_init failed");

    const char* sql = "DELETE FROM users WHERE id = ?";
    if (mysql_stmt_prepare(stmt, sql, -1) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt prepare failed: " + err);
    }

    MYSQL_BIND bind[1] = {};
    bind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    bind[0].buffer      = &id;

    mysql_stmt_bind_param(stmt, bind);

    if (mysql_stmt_execute(stmt) != 0) {
        std::string err = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        throw std::runtime_error("stmt execute failed: " + err);
    }

    std::cout << "[DELETE] affected rows: "
              << mysql_stmt_affected_rows(stmt) << "\n";
    mysql_stmt_close(stmt);
}

// ──────────────────────────────────────────────
// main
// ──────────────────────────────────────────────
int main() {
    // mysql_library_init 是线程安全初始化，单线程程序可省略但建议保留
    mysql_library_init(0, nullptr, nullptr);

    try {
        // ① 建立连接（RAII：离开作用域自动断开）
        Connection conn("127.0.0.1", "root", "pyl200511", "chat_db");

        // ② CREATE
        std::cout << "\n--- CREATE ---\n";
        auto id = insertUser(conn,
                             "alice",
                             "hashed_pw_placeholder",
                             "Alice in Wonderland");
        std::cout << "[INSERT] new user id = " << id << "\n";

        // ③ READ
        std::cout << "\n--- READ ---\n";
        selectUser(conn, id);

        // ④ UPDATE
        std::cout << "\n--- UPDATE ---\n";
        updateNickname(conn, id, "Alice Updated");
        selectUser(conn, id);

        // ⑤ DELETE
        std::cout << "\n--- DELETE ---\n";
        deleteUser(conn, id);
        selectUser(conn, id);   // 应该找不到该行

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        mysql_library_end();
        return 1;
    }

    mysql_library_end();
    return 0;
}
