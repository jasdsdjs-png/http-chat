#ifndef SINGLETON_H
#define SINGLETON_H
#include"global.h"

#include <iostream>
#include <memory>
#include <mutex>

// 【模板基类】CRTP 模式：派生类以自身为模板参数继承此类
// 例如：class MyClass : public SingleTon<MyClass>
template <class T>
class SingleTon {
protected:
    // 【protected 默认构造】阻止外部直接构造实例，但允许派生类构造
    //生成默认构造
    SingleTon() = default;

    // 【删除拷贝构造】禁止拷贝，确保全局只有一个实例
    SingleTon(const SingleTon<T>&) = delete;

    // 【protected 析构】阻止外部直接 delete 实例。
    // 派生类析构时会自动调用，或通过自定义删除器调用
    ~SingleTon() {
        std::cout << "this is singleton destruct" << std::endl;
    }

    // 【删除拷贝赋值】禁止赋值操作，彻底杜绝实例被复制

    SingleTon<T>& operator=(const SingleTon<T>&) = delete;

    // 【静态实例指针】存储唯一的 shared_ptr<T>
    // 必须使用自定义删除器 Deleter，否则 shared_ptr 在类外销毁时无法访问 protected 析构函数
    static std::shared_ptr<T> _instance;

public:
    // 【获取实例】懒汉式构造：第一次调用时才创建对象
    static std::shared_ptr<T> GetInstance() {
        // 【局部静态 once_flag】程序生命周期内只初始化一次，线程安全由编译器保证
        static std::once_flag s_flag;

        // 【call_once】确保 lambda 表达式在多线程环境下绝对只执行一次
        std::call_once(s_flag, []() {
            // 使用 new 直接构造：因为 T 的构造函数通常是 protected，无法使用 std::make_shared
            //make_share无法访问public,而且不能用自定义删除器
            _instance = std::shared_ptr<T>(new T);
        });

        // 返回 shared_ptr，外部通过引用计数共享所有权
        return _instance;
    }
};

template <class T>
std::shared_ptr<T> SingleTon<T>::_instance=nullptr;



#endif // SINGLETON_H
