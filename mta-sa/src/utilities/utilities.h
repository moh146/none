#pragma once
#include <includes.h>
#include <netsdk/CNet.h>
#include <netsdk/packet.h>

namespace utilities {
	class c_report {
	public:
		static void Log(const std::string& filename, const char* format, ...);
	};


	class c_custom {
	public:
		static bool inject_dll(const char* path);
	};

	class c_scripts {
	public:
		static bool is_file_compiled(const void* pData, unsigned int uiLength);
	};

	class c_pattern {
	public:
		static MODULEINFO get_module_info(const char* hModule);
		static DWORD find_pattern(const char* hModule, const char* pattern, const char* mask);
	};

	class c_device {
	public:
		static DWORD find_vtable(DWORD type);
		static DWORD get_address(int vtable);
	};
}