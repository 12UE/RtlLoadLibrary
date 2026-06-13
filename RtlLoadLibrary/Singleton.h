#ifndef SINGLETON_H5C8860F94B8D49198321BF157F1E99E
#define SINGLETON_H5C8860F94B8D49198321BF157F1E99E
#pragma once
#include <mutex>

template<typename T>
class Singleton {
protected:
    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;
    friend T;

private:
    static T* m_pInstance;
    static std::once_flag m_tagOnceFlag;

    static void CreateInstance() {
        m_pInstance = new T();
    }

public:
    static T& GetInstance() {
        std::call_once(m_tagOnceFlag, &Singleton<T>::CreateInstance);
        return *m_pInstance;
    }

    static void DestroyInstance() {
        static std::once_flag tagDestroyFlag;
        std::call_once(tagDestroyFlag, [&]() {
            delete m_pInstance;
            m_pInstance = nullptr;
            });
    }
};

template<typename T>
T* Singleton<T>::m_pInstance = nullptr;

template<typename T>
std::once_flag Singleton<T>::m_tagOnceFlag;
#endif SINGLETON_H5C8860F94B8D49198321BF157F1E99E
