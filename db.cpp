#include "db.h"
#include<stdexcept>
#include<cstring>
#include<iostream>
#include<string>
#include"log.h"

static thread_local MYSQL* tl_conn = nullptr;

void DB::init(const std::string& host,
              const std::string& user,
              const std::string& pass,
              const std::string& dbname,
              unsigned int port) {
    s_host = host; s_user = user; s_password = pass; s_db = dbname; s_port = port;
}

MYSQL* DB::get_conn() {
    if (tl_conn) return tl_conn;

    tl_conn = mysql_init(nullptr);
    std::cout << "check point 1" << "\n";
    if (!tl_conn) throw std::runtime_error("mysql_init failed");

    if (!mysql_real_connect(tl_conn,
                            s_host.c_str(), s_user.c_str(), s_password.c_str(),
                            s_db.c_str(), s_port, nullptr, 0)) {
        throw std::runtime_error(mysql_error(tl_conn));
        LOG_ERROR("Failed to connect to MySQL");
    }

    mysql_set_character_set(tl_conn, "utf8mb4");
    std::cout << "check point 3" << "\n";
    LOG_INFO("MySQL connection established successfully");
    return tl_conn;
}

bool DB::check_login(const std::string& username, const std::string& password) {
    MYSQL* conn = get_conn();

    // 使用预处理，防注入
    const char* sql = "SELECT 1 FROM users WHERE username=? AND password_hash=SHA2(?,256) LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[2]{};
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = (unsigned long)username.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)password.c_str();
    bind[1].buffer_length = (unsigned long)password.size();

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    // 取结果：有一行就算成功
    if (mysql_stmt_store_result(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }
    my_ulonglong rows = mysql_stmt_num_rows(stmt);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);

    return rows > 0;
}
