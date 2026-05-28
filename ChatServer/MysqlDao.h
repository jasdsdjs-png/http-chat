#pragma once

#include "const.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <mysql_connection.h>
#include <mysql_driver.h>

class MySqlPool {
public:
    MySqlPool(const std::string& url,
              const std::string& user,
              const std::string& pass,
              const std::string& schema,
              int pool_size);
    ~MySqlPool();

    std::unique_ptr<sql::Connection> GetConnection();
    void ReturnConnection(std::unique_ptr<sql::Connection> con);
    void Close();

private:
    std::unique_ptr<sql::Connection> CreateNewConnection();
    bool IsConnectionValid(std::unique_ptr<sql::Connection>& con);
    void KeepAliveLoop();

    std::string url_;
    std::string user_;
    std::string pass_;
    std::string schema_;
    int pool_size_;
    std::queue<std::unique_ptr<sql::Connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> b_stop_;
    std::thread keepalive_thread_;
};

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    std::shared_ptr<UserInfo> GetUser(int uid);

private:
    std::unique_ptr<MySqlPool> pool_;
};

