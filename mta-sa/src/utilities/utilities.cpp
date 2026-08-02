#include "utilities.h"
#include <console/console.h>

void utilities::c_report::Log(const std::string& filename, const char* format, ...)
{
	std::ofstream logFile(filename, std::ios_base::app);

	if (!logFile.is_open())
	{
		return;
	}

	char buffer[1024];

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	logFile << buffer << std::endl;
	logFile.close();
}


MODULEINFO utilities::c_pattern::get_module_info(const char* hModule)
{
	MODULEINFO dwModuleInfo = { 0 };

	HMODULE dwModule = GetModuleHandleA(hModule);
	if (!dwModule)
	{
		return dwModuleInfo;
	}

	K32GetModuleInformation(GetCurrentProcess(), dwModule, &dwModuleInfo, sizeof(MODULEINFO));

	return dwModuleInfo;
}

DWORD utilities::c_pattern::find_pattern(const char* hModule, const char* pattern, const char* mask)
{
	MODULEINFO dwModuleInfo = get_module_info(hModule);
	DWORD BaseOfDll = (DWORD)dwModuleInfo.lpBaseOfDll;
	DWORD SizeOfImage = (DWORD)dwModuleInfo.SizeOfImage;
	DWORD PatternLength = (DWORD)strlen(mask);

	for (DWORD i = 0; i < SizeOfImage - PatternLength; i++)
	{
		bool found = true;
		for (DWORD j = 0; j < PatternLength; j++)
		{
			found &= mask[j] == '?' || pattern[j] == *(char*)(BaseOfDll + i + j);
		}

		if (found)
		{
			return BaseOfDll + i;
		}
	}

	return NULL;
}

DWORD utilities::c_device::find_vtable(DWORD type)
{
	DWORD dwBaseObj = 0;

	char info[MAX_PATH];
	GetSystemDirectoryA(info, MAX_PATH);
	strcat_s(info, MAX_PATH, "\\d3d9.dll");

	dwBaseObj = (DWORD)LoadLibraryA(info);

	while (dwBaseObj++ < dwBaseObj + type)
	{
		if ((*(WORD*)(dwBaseObj + 0x00)) == 0x06C7 && (*(WORD*)(dwBaseObj + 0x06)) == 0x8689 && (*(WORD*)(dwBaseObj + 0x0C)) == 0x8689)
		{
			dwBaseObj += 2;
			break;
		}
	}

	return(dwBaseObj);
}

DWORD utilities::c_device::get_address(int vtable)
{
	PDWORD table;
	*(DWORD*)&table = *(DWORD*)find_vtable(0x128000);
	return table[vtable];
}

bool is_utf8_bom(const void* pData, unsigned int uiLength)
{
	const unsigned char* pCharData = (const unsigned char*)pData;
	return (uiLength > 2 && pCharData[0] == 0xEF && pCharData[1] == 0xBB && pCharData[2] == 0xBF);
}


bool utilities::c_scripts::is_file_compiled(const void* pData, unsigned int uiLength)
{
	const unsigned char* pCharData = (const unsigned char*)pData;
	if (is_utf8_bom(pCharData, uiLength))
	{
		pCharData += 3;
		uiLength -= 3;
	}
	return (uiLength > 0 && pCharData[0] == 0x1B);
}

bool utilities::c_custom::inject_dll(const char* path)
{
	HANDLE h_process = GetCurrentProcess();
	if (!h_process)
	{
		return false;
	}

	LPVOID p_dll_path = VirtualAllocEx(h_process, NULL, strlen(path) + 1, MEM_COMMIT, PAGE_READWRITE);
	if (!p_dll_path)
	{
		return false;
	}

	if (!WriteProcessMemory(h_process, p_dll_path, (LPVOID)path, strlen(path) + 1, NULL))
	{
		VirtualFreeEx(h_process, p_dll_path, 0, MEM_RELEASE);
		return false;
	}

	FARPROC p_load_library_a = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	if (!p_load_library_a)
	{
		VirtualFreeEx(h_process, p_dll_path, 0, MEM_RELEASE);
		return false;
	}

	HANDLE h_remote_thread = CreateRemoteThread(h_process, NULL, 0, (LPTHREAD_START_ROUTINE)p_load_library_a, p_dll_path, 0, NULL);
	if (!h_remote_thread)
	{
		VirtualFreeEx(h_process, p_dll_path, 0, MEM_RELEASE);
		return false;
	}

	WaitForSingleObject(h_remote_thread, INFINITE);

	CloseHandle(h_remote_thread);
	VirtualFreeEx(h_process, p_dll_path, 0, MEM_RELEASE);

	return true;
}
