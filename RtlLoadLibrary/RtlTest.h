#pragma once
#include "pch.h"

class RtlTestRunner
{
public:
    typedef void (*TestFunc)();

    struct TestCase
    {
        const char* name;
        TestFunc   func;
        bool       passed;
    };

    void Register(const char* name, TestFunc func)
    {
        m_tests.push_back({ name, func, false });
    }

    int Run()
    {
        int nPassed = 0;
        int nFailed = 0;
        printf("\n==============================\n");
        printf("  RtlLoadLibrary Test Suite\n");
        printf("==============================\n\n");

        for (auto& test : m_tests)
        {
            printf("[RUN ] %s\n", test.name);
            try
            {
                SetLastError(0);
                test.func();
                test.passed = true;
                nPassed++;
                printf("[PASS] %s\n", test.name);
            }
            catch (const std::exception& e)
            {
                nFailed++;
                printf("[FAIL] %s -- exception: %s\n", test.name, e.what());
            }
            catch (...)
            {
                nFailed++;
                printf("[FAIL] %s -- unknown exception\n", test.name);
            }
        }

        printf("\n==============================\n");
        printf("  Results: %d passed, %d failed\n", nPassed, nFailed);
        printf("==============================\n");

        return nFailed;
    }

private:
    std::vector<TestCase> m_tests;
};

inline RtlTestRunner& GetTestRunner()
{
    static RtlTestRunner s_runner;
    return s_runner;
}

#define TEST_CASE(name) \
    static void test_##name(); \
    static const int _reg_##name = (GetTestRunner().Register(#name, test_##name), 0); \
    static void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error("ASSERT_TRUE(" #expr ") failed"); \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            throw std::runtime_error("ASSERT_FALSE(" #expr ") failed"); \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            throw std::runtime_error("ASSERT_EQ(" #a ", " #b ") failed"); \
        } \
    } while(0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            throw std::runtime_error("ASSERT_NE(" #a ", " #b ") failed"); \
        } \
    } while(0)

#define ASSERT_GT(a, b) \
    do { \
        if (!((a) > (b))) { \
            throw std::runtime_error("ASSERT_GT(" #a ", " #b ") failed"); \
        } \
    } while(0)

#define ASSERT_STREQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            throw std::runtime_error("ASSERT_STREQ failed"); \
        } \
    } while(0)

#define ASSERT_WSTREQ(a, b) \
    do { \
        if (wcscmp((a), (b)) != 0) { \
            throw std::runtime_error("ASSERT_WSTREQ failed"); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(a) \
    do { \
        if (!(a)) { \
            throw std::runtime_error("ASSERT_NOT_NULL(" #a ") failed"); \
        } \
    } while(0)
