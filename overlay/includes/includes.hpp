#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif // _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif // _CRT_SECURE_NO_WARNINGS

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <phnt_windows.h>
#include <phnt.h>

#include <tlhelp32.h>
#endif // _WIN32

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>
#include <string_view>
#include <string>
#include <unordered_map>
#include <codecvt>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <csignal>
#include <format>
#include <print>
#include <map>
#include <set>
