#ifndef HYDR_TYPES_HPP
#define HYDR_TYPES_HPP

#include <GLFW/glfw3.h>
#include <cstdint>
#include <fmt/format.h>
#include <array>
#include <glm/fwd.hpp>
#include <optional>
#include <vector>

using u64 = uint_fast64_t;
using u32 = uint_fast32_t;
using u16 = uint_fast16_t;
using u8  = uint_fast8_t;

using i64 = int_fast64_t;
//using i32 = int_fast32_t;
using i32 = int;
using i16 = int_fast16_t;

using byte = uint_least8_t;

template <typename... T>
using Uq_ptr = std::unique_ptr<T...>;

template <typename... T>
using Opt = std::optional<T...>;

template <typename T, size_t n>
using Arr = std::array<T, n>;

template <typename T>
using Vec = std::vector<T>;

GLFWwindow* init_window(glm::uvec2 window_size, const char* window_title, bool* error_bool);
void init_imgui(GLFWwindow* window);
void destroy_window(GLFWwindow* win);
void destroy_imgui();

// defer
#ifndef defer
struct defer_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif // defer

#define LOG(...) \
    fmt::print(stdout, "{}\n", fmt::format(__VA_ARGS__))
#define LOG_ERR(...) \
    fmt::print(stderr, "\033[1;31m{}\033[0m\n", fmt::format(__VA_ARGS__))

#ifdef DEBUG 
#define LOG_DBG(...) \
    fmt::print(stderr, "\033[1;33m{}\033[0m\n", fmt::format(__VA_ARGS__))
#define LOG_NOFORMAT_DBG(...) \
    fmt::print(stderr, "\033[1;33m{}\033[0m\n", __VA_ARGS__)
#else 
#define LOG_DBG(...) ;
#define LOG_NOFORMAT_DBG(...) ;
#endif

#endif // HYDR_TYPES_HPP
