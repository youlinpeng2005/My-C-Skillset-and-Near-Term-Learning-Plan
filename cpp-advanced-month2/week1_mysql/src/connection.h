/**
 * connection.h — RAII MySQL 连接封装
 *
 * 从 day5_mysql_demo.cpp 提取，供 day5 / day6 共用。
 * 依赖：libmariadb（#include <mariadb/mysql.h>）
 */

#pragma once

#include <mariadb/mysql.h>

#include <iostream>
#include <stdexcept>
#include <string>

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

        bool reconnect = true;
        mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

        if (!mysql_real_connect(conn_, host, user, password,
                                db, port, nullptr, 0)) {
            std::string err = mysql_error(conn_);
            mysql_close(conn_);
            conn_ = nullptr;
            throw std::runtime_error("mysql_real_connect failed: " + err);
        }

        mysql_set_character_set(conn_, "utf8mb4");
    }

    ~Connection() {
        if (conn_) {
            mysql_close(conn_);
        }
    }

    // 禁止拷贝，允许移动
    Connection(const Connection&)            = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept : conn_(other.conn_) {
        other.conn_ = nullptr;
    }

    MYSQL* raw() { return conn_; }

    // 执行 SQL，自动消费所有结果集（防止 "Commands out of sync"）
    // SELECT 语句也可以调，结果集会被丢弃；如需读结果请用 query()
    void execute(const std::string& sql) {
        if (mysql_query(conn_, sql.c_str()) != 0) {
            throw std::runtime_error("execute failed: " +
                                     std::string(mysql_error(conn_)) +
                                     "\nSQL: " + sql);
        }
        // 消费掉所有结果集（包括 SELECT 产生的），否则连接进入 out-of-sync 状态
        MYSQL_RES* res = mysql_store_result(conn_);
        if (res) mysql_free_result(res);
    }

    // ping 连接是否存活（可选，用于 idle 超时场景）
    bool ping() {
        return mysql_ping(conn_) == 0;
    }

private:
    MYSQL* conn_ = nullptr;
};
