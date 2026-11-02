#pragma once
#include "fnq/core.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#define CHECK(...) do { if(!(__VA_ARGS__)) throw std::runtime_error(#__VA_ARGS__); } while(false)
inline int test_number(char** argv) { return std::atoi(argv[1]); }
template<class F> int run(F f) { try { f(); return 0; } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
using namespace fnq;
inline constexpr auto hi=std::numeric_limits<std::int64_t>::max();
inline constexpr auto lo=std::numeric_limits<std::int64_t>::min();
