#pragma once
#include <vector>
#include <functional>
#include <iostream>
#include <cstdlib>

struct CatchTest {
    const char* name;
    std::function<void()> func;
};

inline std::vector<CatchTest>& __catch_tests() {
    static std::vector<CatchTest> t;
    return t;
}

struct CatchRegister {
    CatchRegister(const char* n, std::function<void()> f) {
        __catch_tests().push_back({n, f});
    }
};

#define TEST_CASE(name) \
    static void name(); \
    static CatchRegister catch_reg_##name(#name, name); \
    static void name()

#define REQUIRE(cond) \
    do { if(!(cond)) { \
        std::cerr << "Requirement failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } } while(0)

#ifdef CATCH_CONFIG_MAIN
int main() {
    for (auto& t : __catch_tests()) {
        std::cout << "Running " << t.name << std::endl;
        t.func();
    }
    std::cout << "All tests passed" << std::endl;
    return 0;
}
#endif
