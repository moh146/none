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

// ════════════════════════════════════════════════════════════════════════════
//  c_netc — all hooks and bypasses for netc.dll
//
//  Pattern source: static analysis of netc.dll v1.6-release-24127
//
//  Bypass map:
//
//  ── ANTI-CHEAT CHECKERS ─────────────────────────────────────────────────
//    CCheckerAll::Pulse    → return true      master pulse (88+91+94+99 at once)
//    CChecker94::DoCheck   → return true      cert/text-baseline checker
//    CheckCompatibility    → return true      DLL version compatibility gate
//    CheckService          → return true      service health check gate
//    C94_SealSelfMacWitnessed → return true   MAC/seal self-attestation
//
//  ── REPORTING / TELEMETRY ───────────────────────────────────────────────
//    CReportLogger::DoAddNetReportLog → NOP   drops ALL outgoing AC reports
//    CCommsRouter::Send    → NOP             drops network report uploads
//    send_report           → return 1        legacy report sender
//    driver_send_report    → return NULL     kernel-driver report sender
//
//  ── KICK / BAN ──────────────────────────────────────────────────────────
//    client_kick           → NOP             server-side kick
//    local_kick            → NOP             local AC-triggered kick
//    client_ban            → NOP             server-side ban
//    DisconnectWithReason  → NOP             disconnect + reason logger
//    UpdateDisconnectWithReason → NOP        deferred kick-reason update
//    NtTerminateProcess    → NOP             forced process termination
//
//  ── DETECTION BYPASSES ──────────────────────────────────────────────────
//    GetSerial             → zero serial     hardware serial / HW-ban bypass
//    CChecker99::Pulse     → return true     memory COW/integrity checker
//    send_packet           → filter IDs      drop AC packet IDs 34/91/92/93/25
//    deobfuscate_script    → intercept       script dump on deobfuscate
// ════════════════════════════════════════════════════════════════════════════

class c_netc {
private:
    // ── send / report ────────────────────────────────────────────────────────
    typedef bool (__thiscall* send_packet_t)(void* ecx, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);
    typedef int  (__cdecl*    send_report_t)(char arg_list, void* a2, int a3, int a4, int a5);
    typedef HANDLE (__stdcall* driver_send_report_t)(LPCWSTR pszLogFileName, ACCESS_MASK fDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES psaLogFile, ULONG fCreateDisposition, ULONG fFlagsAndAttributes);

    // ── kick / ban ──────────────────────────────────────────────────────────
    typedef void (__thiscall* client_kick_t)(int ecx, int EAX, unsigned int id, int argument4, DWORD* argument5, int argument6);
    typedef void (__thiscall* local_kick_t) (int ecx, char reason);
    typedef void (__thiscall* client_ban_t) (int ecx, int a2, int* client_id, int* reason, char ban_flags, int time);

    // ── disconnect bypass ───────────────────────────────────────────────────
    typedef void (__thiscall* disconnect_with_reason_t)        (void* ecx, const char* reason, bool bSendReason);
    typedef void (__thiscall* update_disconnect_with_reason_t) (void* ecx, int a2, int a3, int a4);

    // ── checkers ────────────────────────────────────────────────────────────
    // CCheckerAll::Pulse: master dispatcher running checkers 88, 91, 94, 99.
    // Hooking this ONE function disables all four checkers simultaneously.
    typedef bool (__thiscall* checker_all_pulse_t)(void* ecx);

    // CChecker94::DoCheck: cert / text-baseline signature verifier.
    typedef bool (__thiscall* checker94_docheck_t)(void* ecx, int a2);

    // CChecker99::Pulse: memory COW / write-protect integrity check.
    typedef bool (__thiscall* checker99_pulse_t)(void* ecx);

    // ── report logger ───────────────────────────────────────────────────────
    // CReportLogger::DoAddNetReportLog: single choke-point for ALL AC reports.
    typedef int (__thiscall* report_logger_t)(void* ecx, void* a2, void* a3, void* a4);

    // CCommsRouter::Send: network upload path for crash/AC telemetry.
    typedef int (__thiscall* comms_router_send_t)(void* ecx, void* a2, int a3);

    // ── misc ────────────────────────────────────────────────────────────────
    typedef bool   (__stdcall*  deobfuscate_script_t)(char* cpInBuffer, UINT uiInSize, char** pcpOutBuffer, UINT* puiOutSize, char* szScriptName);
    typedef const char* (__thiscall* get_connected_server_t)(void* ecx, bool includePort);

    // GetSerial: collects CPU/HW identifiers for master-server reporting.
    typedef void (__thiscall* get_serial_t)(void* ecx, void* pSerialOut);

    // ── exported gates (resolved via GetProcAddress) ─────────────────────────
    typedef int  (__cdecl* check_compatibility_t)(void* a1, void* a2);
    typedef int  (__cdecl* check_service_t)       (void* a1, void* a2);
    typedef bool (__cdecl* c94_seal_self_t)        ();

    // NtTerminateProcess thunk inside netc.dll (proxy to ntdll).
    typedef NTSTATUS (__stdcall* nt_terminate_process_t)(HANDLE hProcess, NTSTATUS exitStatus);

    // ── send_packet (RakNet, fast-call aligned-stack variant) ──────────────
    typedef bool (__fastcall* send_packet_raw_t)(void* ECX, void* EDX, unsigned char ucPacketID, void* bitStream, int packetPriority, int packetReliability, int packetOrdering);

public:
    void* c_net_manager = nullptr;

    // originals — all public so hook bodies can call them
    send_packet_raw_t                o_send_packet                   = nullptr;
    send_report_t                    o_send_report                   = nullptr;
    driver_send_report_t             o_driver_send_report            = nullptr;
    client_kick_t                    o_client_kick                   = nullptr;
    local_kick_t                     o_local_kick                    = nullptr;
    client_ban_t                     o_client_ban                    = nullptr;
    disconnect_with_reason_t         o_disconnect_with_reason        = nullptr;
    update_disconnect_with_reason_t  o_update_disconnect_with_reason = nullptr;
    checker_all_pulse_t              o_checker_all_pulse             = nullptr;
    checker94_docheck_t              o_checker94_docheck             = nullptr;
    checker99_pulse_t                o_checker99_pulse               = nullptr;
    report_logger_t                  o_report_logger                 = nullptr;
    comms_router_send_t              o_comms_router_send             = nullptr;
    deobfuscate_script_t             o_deobfuscate_script            = nullptr;
    get_connected_server_t           o_get_connected_server          = nullptr;
    get_serial_t                     o_get_serial                    = nullptr;
    check_compatibility_t            o_check_compatibility           = nullptr;
    check_service_t                  o_check_service                 = nullptr;
    c94_seal_self_t                  o_c94_seal_self                 = nullptr;
    nt_terminate_process_t           o_nt_terminate_process          = nullptr;

    std::map<std::wstring, std::vector<char>> originalScripts;
    bool updated_serial = false;

    bool release();
};

inline c_netc* netc = new c_netc();
