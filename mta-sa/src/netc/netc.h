#pragma once

#include <includes.h>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <windows.h>

#define MAIN_H
#include "netsdk/CNet.h"

struct ScriptData {
    std::string name;
    std::string content;
};

inline std::vector<ScriptData> scriptList;
inline std::vector<std::string> scriptNames;

class c_netc {
private:
    // ── original send/report ─────────────────────────────────────────────────
    typedef bool(__thiscall* send_packet_t)(void* ecx, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
    typedef int(__cdecl*  send_report_t)(char arg_list, void* a2, int a3, int a4, int a5);
    typedef HANDLE(__stdcall* driver_send_report_t)(LPCWSTR pszLogFileName, ACCESS_MASK fDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES psaLogFile, ULONG fCreateDisposition, ULONG fFlagsAndAttributes);

    // ── kick / ban ────────────────────────────────────────────────────────────
    typedef void(__thiscall* client_kick_t)(int ecx, int EAX, unsigned int id, int argument4, DWORD* argument5, int argument6);
    typedef void(__thiscall* local_kick_t)(int ecx, char reason);
    typedef void(__thiscall* client_ban_t)(int ecx, int a2, int* client_id, int* reason, char ban_flags, int time);

    // ── misc netc ─────────────────────────────────────────────────────────────
    typedef int(__thiscall*  stop_network_t)(int ecx);
    typedef bool(__stdcall*  deobfuscate_script_t)(char* cpInBuffer, UINT uiInSize, char** pcpOutBuffer, UINT* puiOutSize, char* szScriptName);
    typedef const char*(__thiscall* get_connected_server_t)(void* ecx, bool includePort);

    // ── NEW: disconnect bypass ────────────────────────────────────────────────
    // void __thiscall CNetDLL::DisconnectWithReason(void* ecx, const char* reason, bool bSendReason)
    typedef void(__thiscall* disconnect_with_reason_t)(void* ecx, const char* reason, bool bSendReason);

    // void __thiscall CNetDLL::UpdateDisconnectWithReason(void* ecx, ...)
    typedef void(__thiscall* update_disconnect_with_reason_t)(void* ecx, int a2, int a3, int a4);

    // ── NEW: CChecker99::Pulse (memory integrity / anticheat pulse) ───────────
    // Returns bool – true means check passed, false triggers kick
    typedef bool(__thiscall* checker99_pulse_t)(void* ecx);

    // ── NEW: GetSerial (CPU/HW serial reporting) ──────────────────────────────
    typedef void(__thiscall* get_serial_t)(void* ecx, void* pSerialOut);

public:
    void* c_net_manager = nullptr;

    send_packet_t                  o_send_packet                  = nullptr;
    send_report_t                  o_send_report                  = nullptr;
    driver_send_report_t           o_driver_send_report           = nullptr;
    client_kick_t                  o_client_kick                  = nullptr;
    local_kick_t                   o_local_kick                   = nullptr;
    client_ban_t                   o_client_ban                   = nullptr;
    stop_network_t                 o_stop_network                 = nullptr;
    deobfuscate_script_t           o_deobfuscate_script           = nullptr;
    get_connected_server_t         o_get_connected_server         = nullptr;

    // new
    disconnect_with_reason_t       o_disconnect_with_reason       = nullptr;
    update_disconnect_with_reason_t o_update_disconnect_with_reason = nullptr;
    checker99_pulse_t              o_checker99_pulse              = nullptr;
    get_serial_t                   o_get_serial                   = nullptr;

    std::map<std::wstring, std::vector<char>> originalScripts;

    bool updated_serial = false;

    bool release();
};

inline c_netc* netc = new c_netc();
