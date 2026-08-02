#include "netc.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <data/elements.h>
#include <data/variables.h>
#include <console/console.h>
#include <utilities/utilities.h>
#include <netsdk/utilities/SString.h>
#include "netsdk/packet.h"
#include <intrin.h>
#include <wincred.h>

// ════════════════════════════════════════════════════════════════════════════
//  Internal helper: create + enable a MinHook in one call
// ════════════════════════════════════════════════════════════════════════════
static void apply_hook(LPVOID target, LPVOID detour, LPVOID* original, const char* tag)
{
    if (!target) {
        console->warn(xorstr_("[netc] %s — address not found\n"), tag);
        return;
    }
    MH_CreateHook(target, detour, original);
    MH_EnableHook(target);
    console->success(xorstr_("[netc] %s hooked\n"), tag);
}

static DWORD resolve_export(const char* dll, const char* sym)
{
    HMODULE h = GetModuleHandleA(dll);
    return h ? reinterpret_cast<DWORD>(GetProcAddress(h, sym)) : 0;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 1 — send_packet
//
//  Filters outgoing packets:
//    packet 34  = player-sync anticheat report
//    packet 91  = weapon-sync AC flag
//    packet 92  = AC detection ping
//    packet 93  = AC ban signal
//    packet 25  = admin/AC alert
//    packet 33  = Lua script injection echo (conditional, see Send_Script_Packet)
// ════════════════════════════════════════════════════════════════════════════
typedef bool (__fastcall* send_packet_raw_t)(
    void* ECX, void* EDX,
    unsigned char ucPacketID, void* bitStream,
    int packetPriority, int packetReliability, int packetOrdering);

send_packet_raw_t o_send_packet_raw = nullptr;

bool __fastcall h_send_packet(void* pNet, void* edx,
    unsigned char ucPacketID, void* bitStream,
    int packetPriority, int packetReliability, int packetOrdering)
{
    // suppress script-injection echo
    if (ucPacketID == 33) {
        if (var->Send_Script_Packet && packetReliability == 3)  { var->Send_Script_Packet = false; return true; }
        if (var->Send_Script_Packet && packetReliability != 1)  { var->Send_Script_Packet = false; return true; }
    }
    // suppress all AC report packets
    if (ucPacketID == 34 || ucPacketID == 91 || ucPacketID == 92 ||
        ucPacketID == 93 || ucPacketID == 25)
        return true;

    if (!netc->c_net_manager)
        netc->c_net_manager = pNet;

    return netc->o_send_packet(pNet, ucPacketID, bitStream,
        packetPriority, packetReliability, packetOrdering);
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 2 — send_report  (legacy cdecl report sender)
// ════════════════════════════════════════════════════════════════════════════
int __cdecl h_send_report(char ArgList, void* a2, int a3, int a4, int a5)
{
    return 1; // pretend success, send nothing
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 3 — driver_send_report  (kernel-driver path)
// ════════════════════════════════════════════════════════════════════════════
HANDLE __stdcall h_driver_send_report(
    LPCWSTR pszLogFileName, ACCESS_MASK fDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES psaLogFile, ULONG fCreateDisposition, ULONG fFlagsAndAttributes)
{
    return NULL;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 4 — client_kick  (server-side kick)
//  Pattern confirmed in netc.dll v1.6-release-24127:
//    \x53\x8B\xDC\x83\xEC\x08\x83\xE4\xF0\x83\xC4\x04\x55\x8B\x6B\x04
//    \x89\x6C\x24\x04\x8B\xEC\x6A\xFF\x68\x??\x??\x??\x??\x64\xA1\x00
//    mask: xxxxxxxxxxxxxxxxxxxxxxxxx????xxx
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_client_kick(int ecx, int edx,
    unsigned int id, int argument4, DWORD* argument5, int argument6)
{
    return;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 5 — local_kick  (AC-triggered local kick)
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_local_kick(int ecx, void* edx, char reason)
{
    return;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 6 — client_ban  (server-side ban)
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_client_ban(int ecx, void* edx,
    int* client_id, int* reason, char ban_flags, int time)
{
    return;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 7 — DisconnectWithReason
//  Prevents the AC-triggered disconnect that normally follows Lua injection
//  detection or a remote kick packet.
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_disconnect_with_reason(void* ecx, void* edx,
    const char* reason, bool bSendReason)
{
    return; // drop silently
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 8 — UpdateDisconnectWithReason
//  Drops deferred kick-reason updates that queue after injection detection.
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_update_disconnect_with_reason(void* ecx, void* edx,
    int a2, int a3, int a4)
{
    return;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 9 — CCheckerAll::Pulse  ← MASTER BYPASS
//
//  This is the single dispatcher function that runs all four AC checkers:
//    CChecker88::Pulse  (module/DLL integrity)
//    CChecker91::Pulse  (weapon sync integrity)
//    CChecker94::DoCheck (cert + text baseline)
//    CChecker99::Pulse  (memory COW / write-protect)
//
//  Hooking ONE function disables ALL FOUR checkers simultaneously.
//
//  Pattern (netc.dll v1.6-release-24127):
//    \x55\x8B\xEC\x6A\xFF\x68\x??\x??\x??\x??\x64\xA1\x??\x??\x??\x??
//    \x50\x81\xEC\x70\x01\x00\x00\xA1\x??\x??\x??\x??
//    mask: xxxxxx????xx????xxxxxxxx????
// ════════════════════════════════════════════════════════════════════════════
bool __fastcall h_checker_all_pulse(void* ecx, void* edx)
{
    return true; // all checks "passed" — MTA never triggers a response
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 10 — CChecker94::DoCheck  (cert + text-baseline verifier)
//
//  Checker94 validates DLL signatures and the in-memory text section against
//  a baseline hash.  If it fails, it calls the local kick routine with a
//  "CChecker94 kick %s @local" log entry.
//
//  Pattern (netc.dll v1.6-release-24127):
//    \x55\x8B\x6B\x04\x89\x6C\x24\x04\x8B\xEC\x6A\xFF\x68\x??\x??\x??\x??
//    \x64\xA1\x??\x??\x??\x??\x50\x53\x81\xEC\x94
//    mask: xxxxxxxxxxxxx????xx????xxxxx
// ════════════════════════════════════════════════════════════════════════════
bool __fastcall h_checker94_docheck(void* ecx, void* edx, int a2)
{
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 11 — CChecker99::Pulse  (COW memory integrity check)
//
//  Separately hooked in addition to the master pulse as a safety net in case
//  MTA spawns it on an independent thread outside the main pulse dispatcher.
//
//  Pattern (netc.dll v1.6-release-24127):
//    \x55\x8B\xEC\x6A\xFF\x68\x??\x??\x??\x??\x64\xA1\x??\x??\x??\x??
//    \x50\x81\xEC\x90\x00\x00\x00\xA1\x??\x??\x??\x??
//    mask: xxxxxx????xx????xxxxxxxx????
// ════════════════════════════════════════════════════════════════════════════
bool __fastcall h_checker99_pulse(void* ecx, void* edx)
{
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 12 — CReportLogger::DoAddNetReportLog  ← REPORT SINK BYPASS
//
//  Single choke-point for ALL outgoing AC and crash telemetry.
//  Every checker, every kick, every crash that MTA tries to report funnels
//  through this function before hitting the network.
//
//  Pattern (netc.dll v1.6-release-24127):
//    \x55\x8B\xEC\x83\xEC\x10\x53\x56\x57\x8B\xF1\xE8\xA0\xB7\xFF\xFF
//    \xE8\x6B\x49\x16\x00\x84\xC0\x74\x29
//    mask: xxxxxxxxxxxxxxxxxxxxxxxx
// ════════════════════════════════════════════════════════════════════════════
int __fastcall h_report_logger(void* ecx, void* edx, void* a2, void* a3, void* a4)
{
    return 1; // pretend success — nothing is queued or sent
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 13 — CCommsRouter::Send  (network upload path)
//  Belt-and-suspenders: even if a report escapes the logger, it still hits
//  this function before going out on the wire.
// ════════════════════════════════════════════════════════════════════════════
int __fastcall h_comms_router_send(void* ecx, void* edx, void* a2, int a3)
{
    return 0; // pretend sent, actually dropped
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 14 — deobfuscate_script
//  Called every time the server pushes an obfuscated Lua script.
//  Intercepts the output buffer to store/dump the raw bytecode or source.
// ════════════════════════════════════════════════════════════════════════════
bool __stdcall h_deobfuscate_script(
    char* cpInBuffer, UINT uiInSize,
    char** pcpOutBuffer, UINT* puiOutSize, char* szScriptName)
{
    bool result = netc->o_deobfuscate_script(
        cpInBuffer, uiInSize, pcpOutBuffer, puiOutSize, szScriptName);

    if (result) {
        const char*  buffer = element->dump.deobfuscate && pcpOutBuffer ? *pcpOutBuffer : cpInBuffer;
        unsigned int size   = element->dump.deobfuscate && puiOutSize   ? *puiOutSize   : uiInSize;

        bool is_compiled = utilities::c_scripts::is_file_compiled(buffer, size);

        s_dumps dump_entry;
        dump_entry.script_name = szScriptName;
        dump_entry.size        = size;
        dump_entry.is_compiled = is_compiled;

        // Only copy bytes into RAM when the user has enabled live dumping.
        if (element->dump.dump_enabled)
            dump_entry.buffer.assign(buffer, buffer + size);

        // Cap list at 256 entries (FIFO) to prevent unbounded heap growth.
        if (element->dump.dumps_list.size() >= 256)
            element->dump.dumps_list.erase(element->dump.dumps_list.begin());
        element->dump.dumps_list.push_back(std::move(dump_entry));

        if (element->dump.dump_enabled) {
            std::string script_name = szScriptName;
            std::replace(script_name.begin(), script_name.end(), '\\', '/');
            std::string file_path = xorstr_("Dumps-Nebul/") + script_name;
            std::filesystem::create_directories(
                std::filesystem::path(file_path).parent_path());
            std::ofstream file(file_path, std::ios::binary);
            if (file.is_open())
                file.write(reinterpret_cast<const char*>(
                    dump_entry.buffer.data()), dump_entry.size);
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 15 — GetSerial  (CPU/HW serial reporter)
//
//  The function reads CPUID data and packs it into an SString that gets sent
//  to the master server for hardware-ban enforcement.  We call the original
//  to keep the object initialised, then zero the output buffer.
//
//  Pattern: function containing THREE CPUID (0F A2) instructions, SEH frame:
//    \x55\x8B\x6B\x04\x89\x6C\x24\x04\x8B\xEC\x6A\xFF\x68\x??\x??\x??\x??
//    \x64\xA1\x??\x??\x??\x??\x50\x53\x81\xEC\x04
//    mask: xxxxxxxxxxxxx????xx????xxxxx
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_get_serial(void* ecx, void* edx, void* pSerialOut)
{
    netc->o_get_serial(ecx, pSerialOut);
    if (pSerialOut)
        memset(pSerialOut, 0, 64);
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 16 — NtTerminateProcess  (netc.dll thunk)
//
//  netc.dll exports its own NtTerminateProcess trampoline that it calls
//  internally when CheckService/CChecker94 decide the game must die.
//  Hooking this thunk prevents that forced kill without touching ntdll.
//
//  Pattern (confirmed thunk bytes):
//    \x55\x8B\xEC\x5D\xE9\x97\xD6\xF6\xFF
//    mask: xxxxxxxxx
// ════════════════════════════════════════════════════════════════════════════
NTSTATUS __stdcall h_nt_terminate_process(HANDLE hProcess, NTSTATUS exitStatus)
{
    return STATUS_SUCCESS; // lie — process was not terminated
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 17 — CheckCompatibility (exported)
//  Gates whether the client DLL is considered compatible with the server.
//  Forcing it to return 1 (compatible) prevents version-mismatch kicks.
// ════════════════════════════════════════════════════════════════════════════
int __cdecl h_check_compatibility(void* a1, void* a2)
{
    return 1;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 18 — CheckService (exported)
//  Called periodically to verify netc's internal service state.  Returning 1
//  prevents the service-failure path from triggering a reconnect/kick loop.
// ════════════════════════════════════════════════════════════════════════════
int __cdecl h_check_service(void* a1, void* a2)
{
    return 1;
}

// ════════════════════════════════════════════════════════════════════════════
//  HOOK 19 — C94_SealSelfMacWitnessed (exported)
//  The 94-GATE checker uses this to attest that the running binary is a
//  legitimate, unmodified MTA installation.  Returning true makes the gate
//  pass unconditionally.
// ════════════════════════════════════════════════════════════════════════════
bool __cdecl h_c94_seal_self()
{
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
//  c_netc::release() — resolve all targets and install hooks
// ════════════════════════════════════════════════════════════════════════════
bool c_netc::release()
{
    // ────────────────────────────────────────────────────────────────────────
    //  A. EXPORTED FUNCTIONS — resolved via GetProcAddress (version-stable)
    // ────────────────────────────────────────────────────────────────────────

    // ── NtTerminateProcess (netc.dll thunk) ──────────────────────────────────
    // Exported by name; also backed up by the unique thunk pattern.
    o_nt_terminate_process = reinterpret_cast<nt_terminate_process_t>(
        resolve_export("netc.dll", "NtTerminateProcess"));
    if (!o_nt_terminate_process)
        o_nt_terminate_process = reinterpret_cast<nt_terminate_process_t>(
            utilities::c_pattern::find_pattern(
                "netc.dll",
                "\x55\x8B\xEC\x5D\xE9\x97\xD6\xF6\xFF",
                "xxxxxxxxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_nt_terminate_process),
               reinterpret_cast<LPVOID>(h_nt_terminate_process),
               reinterpret_cast<LPVOID*>(&o_nt_terminate_process),
               "NtTerminateProcess");

    // ── CheckCompatibility ───────────────────────────────────────────────────
    o_check_compatibility = reinterpret_cast<check_compatibility_t>(
        resolve_export("netc.dll", "CheckCompatibility"));
    apply_hook(reinterpret_cast<LPVOID>(o_check_compatibility),
               reinterpret_cast<LPVOID>(h_check_compatibility),
               reinterpret_cast<LPVOID*>(&o_check_compatibility),
               "CheckCompatibility");

    // ── CheckService ─────────────────────────────────────────────────────────
    o_check_service = reinterpret_cast<check_service_t>(
        resolve_export("netc.dll", "CheckService"));
    apply_hook(reinterpret_cast<LPVOID>(o_check_service),
               reinterpret_cast<LPVOID>(h_check_service),
               reinterpret_cast<LPVOID*>(&o_check_service),
               "CheckService");

    // ── C94_SealSelfMacWitnessed ─────────────────────────────────────────────
    o_c94_seal_self = reinterpret_cast<c94_seal_self_t>(
        resolve_export("netc.dll", "C94_SealSelfMacWitnessed"));
    apply_hook(reinterpret_cast<LPVOID>(o_c94_seal_self),
               reinterpret_cast<LPVOID>(h_c94_seal_self),
               reinterpret_cast<LPVOID*>(&o_c94_seal_self),
               "C94_SealSelfMacWitnessed");

    // ────────────────────────────────────────────────────────────────────────
    //  B. INTERNAL FUNCTIONS — pattern-scanned
    // ────────────────────────────────────────────────────────────────────────

    // ── CCheckerAll::Pulse  ← MASTER BYPASS ──────────────────────────────────
    // One hook kills all four checkers (88, 91, 94, 99).
    // Pattern confirmed in netc.dll v1.6-release-24127.
    o_checker_all_pulse = reinterpret_cast<checker_all_pulse_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00"
            "\x50\x81\xEC\x70\x01\x00\x00\xA1\x00\x00\x00\x00",
            "xxxxxx????xx????xxxxxxxx????"));
    apply_hook(reinterpret_cast<LPVOID>(o_checker_all_pulse),
               reinterpret_cast<LPVOID>(h_checker_all_pulse),
               reinterpret_cast<LPVOID*>(&o_checker_all_pulse),
               "CCheckerAll::Pulse");

    // ── CChecker94::DoCheck ───────────────────────────────────────────────────
    // Cert + text-baseline verifier.  Hooked separately as safety net in case
    // MTA calls it outside the main pulse (e.g. on connect).
    // Pattern: aligned-stack 0x94 frame with 6A FF SEH cookie.
    o_checker94_docheck = reinterpret_cast<checker94_docheck_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\x6B\x04\x89\x6C\x24\x04\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00"
            "\x64\xA1\x00\x00\x00\x00\x50\x53\x81\xEC\x94",
            "xxxxxxxxxxxxx????xx????xxxxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_checker94_docheck),
               reinterpret_cast<LPVOID>(h_checker94_docheck),
               reinterpret_cast<LPVOID*>(&o_checker94_docheck),
               "CChecker94::DoCheck");

    // ── CChecker99::Pulse (COW / write-protect) ───────────────────────────────
    // Pattern: SEH 6A FF frame, 0x90 (144-byte) stack alloc — unique to this fn.
    o_checker99_pulse = reinterpret_cast<checker99_pulse_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00"
            "\x50\x81\xEC\x90\x00\x00\x00\xA1\x00\x00\x00\x00",
            "xxxxxx????xx????xxxxxxxx????"));
    apply_hook(reinterpret_cast<LPVOID>(o_checker99_pulse),
               reinterpret_cast<LPVOID>(h_checker99_pulse),
               reinterpret_cast<LPVOID*>(&o_checker99_pulse),
               "CChecker99::Pulse");

    // ── CReportLogger::DoAddNetReportLog ──────────────────────────────────────
    // Choke-point for ALL AC/crash report telemetry.
    // Pattern: no-SEH stdcall, calls two helpers at known relative offsets,
    //          then immediately tests AL (84 C0) to decide whether to queue.
    o_report_logger = reinterpret_cast<report_logger_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x83\xEC\x10\x53\x56\x57\x8B\xF1\xE8\xA0\xB7\xFF\xFF"
            "\xE8\x6B\x49\x16\x00\x84\xC0\x74\x29",
            "xxxxxxxxxxxxxxxxxxxxxxxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_report_logger),
               reinterpret_cast<LPVOID>(h_report_logger),
               reinterpret_cast<LPVOID*>(&o_report_logger),
               "CReportLogger::DoAddNetReportLog");

    // ── client_kick ───────────────────────────────────────────────────────────
    // Pattern confirmed present in this DLL (aligned-stack variant).
    o_client_kick = reinterpret_cast<client_kick_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x53\x8B\xDC\x83\xEC\x08\x83\xE4\xF0\x83\xC4\x04\x55\x8B\x6B\x04"
            "\x89\x6C\x24\x04\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00",
            "xxxxxxxxxxxxxxxxxxxxxxxxx????xxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_client_kick),
               reinterpret_cast<LPVOID>(h_client_kick),
               reinterpret_cast<LPVOID*>(&o_client_kick),
               "client_kick");

    // ── local_kick ────────────────────────────────────────────────────────────
    // 6A FF cookie; 9D 7F 3E 10 handler (may differ by build — wildcard).
    o_local_kick = reinterpret_cast<local_kick_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81",
            "xxxxxx????xx????xx"));
    apply_hook(reinterpret_cast<LPVOID>(o_local_kick),
               reinterpret_cast<LPVOID>(h_local_kick),
               reinterpret_cast<LPVOID*>(&o_local_kick),
               "local_kick");

    // ── client_ban ────────────────────────────────────────────────────────────
    o_client_ban = reinterpret_cast<client_ban_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x34",
            "xxxxxx????xx????xxxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_client_ban),
               reinterpret_cast<LPVOID>(h_client_ban),
               reinterpret_cast<LPVOID*>(&o_client_ban),
               "client_ban");

    // ── DisconnectWithReason ──────────────────────────────────────────────────
    // 6A FF cookie; 35 08 3E 10 handler (wildcarded).
    o_disconnect_with_reason = reinterpret_cast<disconnect_with_reason_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x48",
            "xxxxxx????xx????xxxx"));
    // Fallback: 6A FE variant (same function, different build slot)
    if (!o_disconnect_with_reason)
        o_disconnect_with_reason = reinterpret_cast<disconnect_with_reason_t>(
            utilities::c_pattern::find_pattern(
                "netc.dll",
                "\x55\x8B\xEC\x6A\xFE\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83",
                "xxxxxx????xx????xx"));
    apply_hook(reinterpret_cast<LPVOID>(o_disconnect_with_reason),
               reinterpret_cast<LPVOID>(h_disconnect_with_reason),
               reinterpret_cast<LPVOID*>(&o_disconnect_with_reason),
               "DisconnectWithReason");

    // ── UpdateDisconnectWithReason ────────────────────────────────────────────
    // 6A FE cookie (note FE not FF); two SEH handler pushes.
    o_update_disconnect_with_reason = reinterpret_cast<update_disconnect_with_reason_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFE\x68\x00\x00\x00\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x81\xEC",
            "xxxxxx????x????xx????xxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_update_disconnect_with_reason),
               reinterpret_cast<LPVOID>(h_update_disconnect_with_reason),
               reinterpret_cast<LPVOID*>(&o_update_disconnect_with_reason),
               "UpdateDisconnectWithReason");

    // ── send_packet ───────────────────────────────────────────────────────────
    // RakNet aligned-stack thiscall variant.
    // Pattern: push-save-restore stdcall wrapper → 53 8B DC 83 EC 08 83 E4 F0...
    o_send_packet = reinterpret_cast<send_packet_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x53\x8B\xDC\x83\xEC\x08\x83\xE4\xF0\x83\xC4\x04\x55\x8B\x6B\x04\x89\x6C\x24\x04"
            "\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00",
            "xxxxxxxxxxxxxxxxxxxxxxxxx????"));
    apply_hook(reinterpret_cast<LPVOID>(o_send_packet),
               reinterpret_cast<LPVOID>(h_send_packet),
               reinterpret_cast<LPVOID*>(&o_send_packet),
               "send_packet");
    // Mirror to the raw pointer used by h_send_packet
    o_send_packet_raw = reinterpret_cast<send_packet_raw_t>(o_send_packet);

    // ── send_report ───────────────────────────────────────────────────────────
    // 6A FF cookie; DC B7 3C 10 SEH handler (wildcarded for safety).
    o_send_report = reinterpret_cast<send_report_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x51",
            "xxxxxx????xx????xx"));
    apply_hook(reinterpret_cast<LPVOID>(o_send_report),
               reinterpret_cast<LPVOID>(h_send_report),
               reinterpret_cast<LPVOID*>(&o_send_report),
               "send_report");

    // ── driver_send_report ────────────────────────────────────────────────────
    o_driver_send_report = reinterpret_cast<driver_send_report_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x1C"
            "\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3",
            "xxxxxx????xx????xxxxx????xxxxxxxxxxxxx"));
    apply_hook(reinterpret_cast<LPVOID>(o_driver_send_report),
               reinterpret_cast<LPVOID>(h_driver_send_report),
               reinterpret_cast<LPVOID*>(&o_driver_send_report),
               "driver_send_report");

    // ── deobfuscate_script ────────────────────────────────────────────────────
    // Non-standard prologue: PUSH EAX + MOV EAX,imm32 (no -1 cookie).
    o_deobfuscate_script = reinterpret_cast<deobfuscate_script_t>(
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\xEC\x50\xB8\x00\x00\x00\x00\xB8\x00\x00\x00\x00\x58\x8B\x0D",
            xorstr_("xxxxx????x????xxx")));
    // Fallback: original signature from earlier netc build
    if (!o_deobfuscate_script)
        o_deobfuscate_script = reinterpret_cast<deobfuscate_script_t>(
            utilities::c_pattern::find_pattern(
                xorstr_("netc.dll"),
                "\x55\x8B\xEC\x50\xB8\x8E\xBF\x5C\xBA\xB8\x42\x22",
                xorstr_("xxxxx?x?x??x")));
    apply_hook(reinterpret_cast<LPVOID>(o_deobfuscate_script),
               reinterpret_cast<LPVOID>(h_deobfuscate_script),
               reinterpret_cast<LPVOID*>(&o_deobfuscate_script),
               "deobfuscate_script");

    // ── GetSerial ─────────────────────────────────────────────────────────────
    // Large SEH frame with THREE CPUID instructions inside. 0x04-byte alloc.
    o_get_serial = reinterpret_cast<get_serial_t>(
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\x6B\x04\x89\x6C\x24\x04\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00"
            "\x64\xA1\x00\x00\x00\x00\x50\x53\x81\xEC\x04",
            xorstr_("xxxxxxxxxxxxx????xx????xxxxx")));
    // Fallback: 0x6C-byte alloc variant from older build
    if (!o_get_serial)
        o_get_serial = reinterpret_cast<get_serial_t>(
            utilities::c_pattern::find_pattern(
                xorstr_("netc.dll"),
                "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x6C",
                xorstr_("xxxxxx????xx????xxxx")));
    apply_hook(reinterpret_cast<LPVOID>(o_get_serial),
               reinterpret_cast<LPVOID>(h_get_serial),
               reinterpret_cast<LPVOID*>(&o_get_serial),
               "GetSerial");

    // ── get_connected_server ──────────────────────────────────────────────────
    o_get_connected_server = reinterpret_cast<get_connected_server_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x80\x7D\x08\x00\x8D\x81\xAC\x00\x00",
            "xxxxxxxxxxxx"));

    // ────────────────────────────────────────────────────────────────────────
    //  C. Flush — enable all hooks installed above
    // ────────────────────────────────────────────────────────────────────────
    MH_EnableHook(MH_ALL_HOOKS);
    console->success(xorstr_("[netc] all bypasses installed\n"));
    return true;
}
