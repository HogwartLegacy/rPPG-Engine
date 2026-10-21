// Minimal self-contained test harness (no external dependency).
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace rt {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

inline int run_all() {
    int failed_cases = 0;
    for (auto& c : registry()) {
        const int before = failures();
        c.fn();
        const bool ok = failures() == before;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
        failed_cases += ok ? 0 : 1;
    }
    std::printf("%zu cases, %d failed\n", registry().size(), failed_cases);
    return failed_cases == 0 ? 0 : 1;
}

}  // namespace rt

#define RT_CAT2(a, b) a##b
#define RT_CAT(a, b) RT_CAT2(a, b)
#define TEST_CASE(name)                                                   \
    static void RT_CAT(test_fn_, __LINE__)();                             \
    static rt::Registrar RT_CAT(test_reg_, __LINE__)(name, RT_CAT(test_fn_, __LINE__)); \
    static void RT_CAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::printf("  %s:%d CHECK failed: %s\n", __FILE__, __LINE__, #cond);  \
            ++rt::failures();                                                      \
        }                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                   \
    do {                                                                                        \
        const double va_ = (a), vb_ = (b);                                                      \
        if (!(std::abs(va_ - vb_) <= (tol))) {                                                  \
            std::printf("  %s:%d CHECK_NEAR failed: %s=%.10g vs %s=%.10g (tol %g)\n", __FILE__, \
                        __LINE__, #a, va_, #b, vb_, static_cast<double>(tol));                  \
            ++rt::failures();                                                                   \
        }                                                                                       \
    } while (0)
