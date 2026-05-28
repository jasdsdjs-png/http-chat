#pragma once

#include <memory>
#include <mutex>

// 通用单例模板。
// 使用方式：
// class Foo : public Singleton<Foo> { friend class Singleton<Foo>; ... };
// 外部通过 Foo::GetInstance() 获取唯一实例。
//
// std::call_once 保证多线程环境下只会创建一次对象；
// shared_ptr 用来管理单例生命周期，程序退出时自动释放。
template <class T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    static std::shared_ptr<T> _instance;

public:
    virtual ~Singleton() = default;

    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, []() {
            _instance = std::shared_ptr<T>(new T);
        });
        return _instance;
    }
};

template <class T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
