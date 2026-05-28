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
#include <cppconn/statement.h>
#include <mysql_connection.h>
#include <mysql_driver.h>

class MySqlPool {
public:
    MySqlPool(const std::string& url,
              const std::string& user,
              const std::string& pass,
              const std::string& schema,
              int poolSize)
        : url_(url),
          user_(user),
          pass_(pass),
          schema_(schema),
          poolSize_(poolSize),
          b_stop_(false) {
        try {
            for (int i = 0; i < poolSize_; ++i) {
                auto* driver = sql::mysql::get_mysql_driver_instance();
                std::unique_ptr<sql::Connection> con(driver->connect(url_, user_, pass_));
                con->setSchema(schema_);
                pool_.push(std::move(con));
            }
        }
        catch (sql::SQLException& e) {
            std::cout << "mysql pool init failed: " << e.what() << std::endl;
        }

        keepalive_thread_ = std::thread([this] { KeepAliveLoop(); });
    }

    std::unique_ptr<sql::Connection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] {
            if (b_stop_) {
                return true;
            }
            return !pool_.empty();
        });
        if (b_stop_) {
            return nullptr;
        }

        while (!pool_.empty()) {
            std::unique_ptr<sql::Connection> con(std::move(pool_.front()));
            pool_.pop();
            if (IsConnectionValid(con)) {
                return con;
            }
        }

        return CreateNewConnection();
    }

    void returnConnection(std::unique_ptr<sql::Connection> con) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (b_stop_) {
            return;
        }
        pool_.push(std::move(con));
        cond_.notify_one();
    }

    void Close() {
        b_stop_ = true;
        cond_.notify_all();
        if (keepalive_thread_.joinable()) {
            keepalive_thread_.join();
        }
    }

    ~MySqlPool() {
        Close();
        std::unique_lock<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
    }

private:
    std::unique_ptr<sql::Connection> CreateNewConnection() {
        try {
            auto* driver = sql::mysql::get_mysql_driver_instance();
            std::unique_ptr<sql::Connection> con(driver->connect(url_, user_, pass_));
            con->setSchema(schema_);
            return con;
        }
        catch (sql::SQLException& e) {
            std::cout << "create new connection failed: " << e.what() << std::endl;
            return nullptr;
        }
    }

    bool IsConnectionValid(std::unique_ptr<sql::Connection>& con) {
        try {
            if (!con) return false;
            return con->isValid();
        }
        catch (...) {
            return false;
        }
    }

    void KeepAliveLoop() {
        while (!b_stop_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait_for(lock, std::chrono::minutes(10), [this] {
                    return b_stop_.load();
                });
            }
            if (b_stop_) break;

            std::unique_lock<std::mutex> lock(mutex_);
            size_t size = pool_.size();
            for (size_t i = 0; i < size; ++i) {
                std::unique_ptr<sql::Connection> con(std::move(pool_.front()));
                pool_.pop();
                if (IsConnectionValid(con)) {
                    pool_.push(std::move(con));
                }
            }

            while (pool_.size() < static_cast<size_t>(poolSize_)) {
                auto newCon = CreateNewConnection();
                if (newCon) {
                    pool_.push(std::move(newCon));
                } else {
                    break;
                }
            }
            std::cout << "MySQL pool keepalive check done, pool size: " << pool_.size() << std::endl;
        }
    }

    std::string url_;
    std::string user_;
    std::string pass_;
    std::string schema_;
    int poolSize_;
    std::queue<std::unique_ptr<sql::Connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> b_stop_;
    std::thread keepalive_thread_;
};

class MysqlDao
{
public:
    MysqlDao();
    ~MysqlDao();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
    int ResetPwd(const std::string& name, const std::string& email, const std::string& pwd);
    bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);

private:
    std::unique_ptr<MySqlPool> pool_;
};
