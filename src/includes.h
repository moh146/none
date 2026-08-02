#pragma once
#ifndef INCLUDES
#define INCLUDES
#define WIN32_LEAN_AND_MEAN 
#define _DX9_SDK_INSTALLED
#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define IMGUI_DEFINE_MATH_OPERATORS

#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <d3d9.h>

#if defined(__has_include)
#  if __has_include(<d3dx9.h>)
#    include <d3dx9.h>
#  else
#    pragma message("warning: d3dx9.h not found — install Microsoft DirectX SDK (June 2010)")
#  endif
#else
#  include <d3dx9.h>
#endif

#include <TlHelp32.h>
#include <Psapi.h>
#include <fstream>
#include <cstdarg>
#include <direct.h>
#include <sstream>
#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string_view>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <variant>
#include <thread>
#include <optional>
#include <string.h>
#include <stdio.h>
#include <mmsystem.h>
#include <winsock.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_texteditor.h>
#include <imgui/imgui_keybind.h>

#include <minhook/include/MinHook.h>

#include <protection/xorstr.h>

#endif
