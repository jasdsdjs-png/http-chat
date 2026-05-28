#include "MysqlDao.h"

#include "ConfigMgr.h"

MySqlPool::MySqlPool(const std::string& url,
                     const std::string& user,
                     const std::string& pass,
                     const std::string& schema,
                     int pool_size)
    : url_(url),
      user_(user),
      pass_(pass),
      schema_(schema),
      pool_size_(pool_size),
      b_stop_(false) {
    try {
        for (int i = 0; i < pool_size_; ++i) {
            auto* driver = sql::mysql::get_mysql_driver_instance();
            std::unique_ptr<sql::Connection> con(driver->connect(url_, user_, pass_));
            con->setSchema(schema_);
            pool_.push(std::move(con));
        }
    }
    catch (const sql::SQLException& e) {
        std::cout << "mysql pool init failed: " << e.what() << std::endl;
    }

    keepalive_thread_ = std::thread([this] { KeepAliveLoop(); });
}

MySqlPool::~MySqlPool() {
    Close();
    std::unique_lock<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        pool_.pop();
    }
}

std::unique_ptr<sql::Connection> MySqlPool::GetConnection() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            auto con = std::move(pool_.front());
            pool_.pop();
            if (IsConnectionValid(con)) {
                return con;
            }
        }

        if (b_stop_) {
            return nullptr;
        }
    }

    return CreateNewConnection();
}

void MySqlPool::ReturnConnection(std::unique_ptr<sql::Connection> con) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (b_stop_ || !con) {
        return;
    }
    pool_.push(std::move(con));
    cond_.notify_one();
}

void MySqlPool::Close() {
    b_stop_ = true;
    cond_.notify_all();
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }
}

std::unique_ptr<sql::Connection> MySqlPool::CreateNewConnection() {
    try {
        auto* driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> con(driver->connect(url_, user_, pass_));
        con->setSchema(schema_);
        return con;
    }
    catch (const sql::SQLException& e) {
        std::cout << "create new connection failed: " << e.what() << std::endl;
        return nullptr;
    }
}

bool MySqlPool::IsConnectionValid(std::unique_ptr<sql::Connection>& con) {
    try {
        return con && con->isValid();
    }
    catch (...) {
        return false;
    }
}

void MySqlPool::KeepAliveLoop() {
    while (!b_stop_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait_for(lock, std::chrono::minutes(10), [this] {
                return b_stop_.load();
            });
        }
        if (b_stop_) {
            break;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        size_t size = pool_.size();
        for (size_t i = 0; i < size; ++i) {
            auto con = std::move(pool_.front());
            pool_.pop();
            if (IsConnectionValid(con)) {
                pool_.push(std::move(con));
            }
        }

        while (pool_.size() < static_cast<size_t>(pool_size_)) {
            auto new_con = CreateNewConnection();
            if (!new_con) {
                break;
            }
            pool_.push(std::move(new_con));
        }
    }
}

MysqlDao::MysqlDao() {
    auto& cfg = ConfigMgr::Inst();
    const auto& host = cfg["Mysql"]["Host"];
    const auto& port = cfg["Mysql"]["Port"];
    const auto& pwd = cfg["Mysql"]["Passwd"];
    const auto& schema = cfg["Mysql"]["Schema"];
    const auto& user = cfg["Mysql"]["User"];

    pool_.reset(new MySqlPool("tcp://" + host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao() {
    if (pool_) {
        pool_->Close();
    }
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid) {
    auto con = pool_->GetConnection();
    if (!con) {
        return nullptr;
    }

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement("SELECT uid, name, email, pwd FROM user WHERE uid = ?"));
        stmt->setInt(1, uid);

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        if (!res->next()) {
            pool_->ReturnConnection(std::move(con));
            return nullptr;
        }

        auto user_info = std::make_shared<UserInfo>();
        user_info->uid = res->getInt("uid");
        user_info->name = res->getString("name");
        user_info->email = res->getString("email");
        user_info->pwd = res->getString("pwd");

        pool_->ReturnConnection(std::move(con));
        return user_info;
    }
    catch (const sql::SQLException& e) {
        pool_->ReturnConnection(std::move(con));
        std::cerr << "SQLException: " << e.what()
                  << " (MySQL error code: " << e.getErrorCode()
                  << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return nullptr;
    }
}
