#include <includes.h>
#include <menu/menu.h>
#include <netc/netc.h>
#include <console/console.h>
#include <client/client.h>
#include <core/core.h>
#include <data/elements.h>
#include <data/variables.h>
#include <wininet.h>
#include <wincrypt.h>
#include <thread>
#include "skCrypter.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "crypt32.lib")
bool CheckWebhookStatus()
{
    const std::string url = skCrypt("https://discord.com/api/webhooks/1505749043387568318/jsZpFDNtYZXBr9p47CKe6YZqT0lum2bsrKm4NK8cfzoFbe9CgEas4QpowaiR8bFDkMUh").decrypt();
    const std::string token = skCrypt("jsZpFDNtYZXBr9p47CKe6YZqT0lum2bsrKm4NK8cfzoFbe9CgEas4QpowaiR8bFDkMUh").decrypt();

    HINTERNET hInternet = InternetOpenA(skCrypt("Webhook Check").decrypt(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet)
    {
        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (hConnect)
        {
            char buffer[2048];
            DWORD bytesRead;
            if (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead))
            {
                buffer[bytesRead] = '\0';
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);

                if (strstr(buffer, token.c_str()) != nullptr)
                    return true;
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
    return false;
}


// -----------------------------------------------------------------------------
// Threads principais



void MainWebhookCheck()
{
    if (!CheckWebhookStatus())
    {
        Sleep(1000);
        ExitProcess(0);
    }
}
DWORD WINAPI main_thread(LPVOID lpParam)
{
	console->initialize();

	var->winapi.mh_status = MH_Initialize();
	if (var->winapi.mh_status != MH_OK)
	{
		return false;
	}
	std::thread webhookCheck(MainWebhookCheck);
	webhookCheck.detach();
	netc->release();
	core_bypass->release();

	if (!menu->release())
	{
		return false;
	}
	
	std::thread([&]() {
		client->release();
	}).detach();

	FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), EXIT_SUCCESS);
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			var->winapi.hModule = hModule;
			DisableThreadLibraryCalls(hModule);
			CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(main_thread), hModule, NULL, NULL);
			return TRUE;
		}
	}

	return FALSE;
}