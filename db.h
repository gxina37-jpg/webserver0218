#ifndef DB_H
#define DB_H

#include<mysql/mysql.h>
#include<cstring>
#include<iostream>
#include<string>

class DB {
    public:
        static void init(const std::string& host, 
                         const std::string& user,
                         const std::string& password,
                         const std::string& db_name,
                         unsigned int port
        );
        static bool check_login(const std::string& username, const std::string& password);
    private:
        static MYSQL* get_conn();
        static inline std::string s_host, s_user, s_password, s_db;
        static inline unsigned int s_port = 3306;

};

#endif