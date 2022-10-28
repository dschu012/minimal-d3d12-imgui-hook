#pragma once
#include <iostream>
#include <format>

// Uncomment to create console and enable logging
//#undef NDEBUG

#ifdef NDEBUG
#define LOG(...) (void)0
#define LOGW(...) (void)0
#else
#define LOG(...) std::cout << "[" << __FILE__  << ":" << __LINE__ << "] " << std::format(__VA_ARGS__) << std::endl
#define LOGW(...) std::wcout << "[" << __FILE__  << ":" << __LINE__ << "] " << std::format(__VA_ARGS__) << std::endl
#endif