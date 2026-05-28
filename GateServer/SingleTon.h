#pragma once

#include <iostream>
#include <memory>
#include <mutex>

// CRTP singleton template
// Usage: class Foo : public SingleTon<Foo> { friend class SingleTon<Foo>; ... };
template <class T>
class SingleTon {
protected:
    SingleTon() = default;
    SingleTon(const SingleTon<T>&) = delete;
    SingleTon<T>& operator=(const SingleTon<T>&) = delete;
    static std::shared_ptr<T> _instance;

public:
    ~SingleTon() {
        std::cout << "this is singleton destruct" << std::endl;
    }

    static std::shared_ptr<T> GetInstance() {
        static std::once_flag s_flag;
        std::call_once(s_flag, []() {
            _instance = std::shared_ptr<T>(new T);
        });
        return _instance;
    }
};

template <class T>
std::shared_ptr<T> SingleTon<T>::_instance = nullptr;
