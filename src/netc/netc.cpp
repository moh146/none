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
//  Helpers
// ════════════════════════════════════════════════════════════════════════════
void MH_CreateAndEnableHook(LPVOID target, LPVOID detour, LPVOID* original) {
    MH_CreateHook(target, detour, original);
    MH_EnableHook(target);
}

// ════════════════════════════════════════════════════════════════════════════
//  send_packet hook
// ════════════════════════════════════════════════════════════════════════════
typedef bool(__fastcall* send_packet_raw_t)(
    void* ECX, void* EDX,
    unsigned char ucPacketID, void* bitStream,
    int packetPriority, int packetReliability, int packetOrdering);

send_packet_raw_t o_send_packet = nullptr;

bool __fastcall h_send_packet(void* pNet, void* edx,
    unsigned char ucPacketID, void* bitStream,
    int packetPriority, int packetReliability, int packetOrdering)
{
    // block script-injection packet echoes
    if (ucPacketID == 33) {
        if (var->Send_Script_Packet && packetReliability == 3) {
            var->Send_Script_Packet = false; return true;
        }
        if (var->Send_Script_Packet && packetReliability != 1) {
            var->Send_Script_Packet = false; return true;
        }
    }
    // block various anticheat report packets
    if (ucPacketID == 34 || ucPacketID == 91 || ucPacketID == 92 ||
        ucPacketID == 93 || ucPacketID == 25)
        return true;

    if (!netc->c_net_manager)
        netc->c_net_manager = pNet;

    return netc->o_send_packet(pNet, ucPacketID, bitStream,
        packetPriority, packetReliability, packetOrdering);
}

// ════════════════════════════════════════════════════════════════════════════
//  send_report / driver_send_report  — block all crash/AC reports
// ════════════════════════════════════════════════════════════════════════════
int __cdecl h_send_report(char ArgList, void* a2, int a3, int a4, int a5)
{
    return 1; // pretend success, send nothing
}

HANDLE __stdcall h_driver_send_report(
    LPCWSTR pszLogFileName, ACCESS_MASK fDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES psaLogFile, ULONG fCreateDisposition, ULONG fFlagsAndAttributes)
{
    return NULL;
}

// ════════════════════════════════════════════════════════════════════════════
//  Kick / ban hooks
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_client_kick(int ecx, int edx,
    unsigned int id, int argument4, DWORD* argument5, int argument6)
{
    return; // swallow server-side kick
}

void __fastcall h_local_kick(int ecx, char reason)
{
    return; // swallow local kick (AC-triggered)
}

void __fastcall h_client_ban(int ecx, int a2,
    int* client_id, int* reason, char ban_flags, int time)
{
    return; // swallow ban
}

// ════════════════════════════════════════════════════════════════════════════
//  DisconnectWithReason — NEW bypass
//  Called by MTA whenever a kick/ban or AC detection happens.
//  Returning without calling the original prevents the disconnect that
//  normally crashes the game after Lua injection is detected.
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_disconnect_with_reason(void* ecx, void* /*edx*/,
    const char* reason, bool bSendReason)
{
    // Log for debug but DO NOT forward to the real function
    console->warning(xorstr_("[netc] DisconnectWithReason blocked: %s\n"),
        reason ? reason : "(null)");
    return;
}

// ════════════════════════════════════════════════════════════════════════════
//  UpdateDisconnectWithReason — NEW bypass
//  Queues a disconnect-reason update; blocking it prevents deferred kicks.
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_update_disconnect_with_reason(void* ecx, void* /*edx*/,
    int a2, int a3, int a4)
{
    return; // drop silently
}

// ════════════════════════════════════════════════════════════════════════════
//  CChecker99::Pulse — NEW bypass
//  Memory integrity / COW checker.  If it returns false the game triggers a
//  disconnect.  We always return true (= "all checks passed").
// ════════════════════════════════════════════════════════════════════════════
bool __fastcall h_checker99_pulse(void* ecx, void* /*edx*/)
{
    return true; // tell MTA that everything is fine
}

// ════════════════════════════════════════════════════════════════════════════
//  GetSerial — NEW bypass
//  Fills the caller's buffer with the CPU/HW serial that gets reported to
//  the master server.  We zero it out so the DLL sends an empty serial,
//  preventing hardware bans and serial-based detection.
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_get_serial(void* ecx, void* /*edx*/, void* pSerialOut)
{
    // Call original first so the object is fully initialised, then wipe it
    netc->o_get_serial(ecx, pSerialOut);
    if (pSerialOut)
        memset(pSerialOut, 0, 64); // SString is usually ≤ 64 chars
}

// ════════════════════════════════════════════════════════════════════════════
//  deobfuscate_script hook — dump scripts on the fly
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
        dump_entry.buffer.assign(buffer, buffer + size);
        dump_entry.size        = size;
        dump_entry.is_compiled = is_compiled;
        element->dump.dumps_list.push_back(dump_entry);

        if (element->dump.dump_enabled) {
            std::string script_name = szScriptName;
            std::replace(script_name.begin(), script_name.end(), '\\', '/');
            std::string file_path = xorstr_("Dumps-Nebul/") + script_name;
            std::filesystem::create_directories(
                std::filesystem::path(file_path).parent_path());
            std::ofstream file(file_path, std::ios::binary);
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(
                    dump_entry.buffer.data()), dump_entry.size);
            }
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
//  c_netc::release() — find & hook everything
// ════════════════════════════════════════════════════════════════════════════
bool c_netc::release()
{
    // ── get_connected_server ──────────────────────────────────────────────
    o_get_connected_server = (get_connected_server_t)
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x80\x7D\x08\x00\x8D\x81\xAC\x00\x00",
            "xxxxxxxxxxxx");

    // ── send_report ───────────────────────────────────────────────────────
    // 6A FF cookie; 50 51 suffix (push eax / push ecx)
    o_send_report = (send_report_t)
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\xDC\xB7\x3C\x10\x64\xA1\x00\x00\x00\x00\x50\x51",
            "xxxxxx?xxxxx?xxxxx");
    if (o_send_report) {
        MH_CreateAndEnableHook((LPVOID)o_send_report,
            &h_send_report, (LPVOID*)&o_send_report);
        console->success(xorstr_("send_report hooked\n"));
    }

    // ── driver_send_report ────────────────────────────────────────────────
    o_driver_send_report = (driver_send_report_t)
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\xBD\x0B\x3D\x10\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x1C"
            "\xA1\x40\x69\x60\x10\x33\xC5\x89\x45\xF0\x56\x57\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\x0D",
            "xxxxxx??xxxx?xxxxxxxx?xxxxxxxxxxxxxxxx?xxxxx");
    if (o_driver_send_report) {
        MH_CreateAndEnableHook((LPVOID)o_driver_send_report,
            &h_driver_send_report, (LPVOID*)&o_driver_send_report);
        console->success(xorstr_("driver_send_report hooked\n"));
    }

    // ── client_kick ───────────────────────────────────────────────────────
    o_client_kick = (client_kick_t)
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x53\x8B\xDC\x83\xEC\x08\x83\xE4\xF0\x83\xC4\x04\x55\x8B\x6B\x04\x89\x6C\x24\x04"
            "\x8B\xEC\x6A\xFF\x68\xBF\x66\x3D\x10\x64\xA1\x00\x00\x00\x00\x50\x53\x81\xEC\x88\x0A\x00",
            "xxxxxxxxxxxxxxxxxxxxxxxxx??xxxx?xxxxxxxxxx");
    if (o_client_kick)
        MH_CreateAndEnableHook((LPVOID)o_client_kick,
            &h_client_kick, (LPVOID*)&o_client_kick);

    // ── local_kick ────────────────────────────────────────────────────────
    o_local_kick = (local_kick_t)
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\x9D\x7F\x3E\x10\x64\xA1\x00\x00\x00\x00\x50\x81",
            "xxxxxx?xxxxx?xxxxx");
    if (o_local_kick)
        MH_CreateAndEnableHook((LPVOID)o_local_kick,
            &h_local_kick, (LPVOID*)&o_local_kick);

    // ── client_ban ────────────────────────────────────────────────────────
    o_client_ban = (client_ban_t)
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x55\x8B\xEC\x6A\xFF\x68\xB5\xE2\x3C\x10\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x34",
            "xxxxxx?xxxxx?xxxxxxx");
    if (o_client_ban)
        MH_CreateAndEnableHook((LPVOID)o_client_ban,
            &h_client_ban, (LPVOID*)&o_client_ban);

    // ── send_packet ───────────────────────────────────────────────────────
    o_send_packet = reinterpret_cast<send_packet_t>(
        utilities::c_pattern::find_pattern(
            "netc.dll",
            "\x53\x8B\xDC\x83\xEC\x08\x83\xE4\xF0\x83\xC4\x04\x55\x8B\x6B\x04\x89\x6C\x24\x04"
            "\x8B\xEC\x6A\xFF\x68\x05\x0B\x3D",
            "xxxxxxxxxxxxxxxxxxxxxxxxx?xx"));
    if (o_send_packet)
        MH_CreateAndEnableHook((LPVOID)o_send_packet,
            &h_send_packet, (LPVOID*)&o_send_packet);

    // ── deobfuscate_script ────────────────────────────────────────────────
    // prologue: 55 8B EC 50 B8 <10 junk B8 bytes> 58 8B 0D
    o_deobfuscate_script = (deobfuscate_script_t)
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\xEC\x50\xB8\x8E\xBF\x5C\xBA\xB8\x42\x22",
            xorstr_("xxxxx?x?x??x"));
    if (o_deobfuscate_script)
        MH_CreateAndEnableHook((LPVOID)o_deobfuscate_script,
            &h_deobfuscate_script, (LPVOID*)&o_deobfuscate_script);
    else
        console->error(xorstr_("deobfuscate_script not found\n"));

    // ════════════════════════════════════════════════════════════════════
    //  NEW BYPASSES
    // ════════════════════════════════════════════════════════════════════

    // ── DisconnectWithReason ──────────────────────────────────────────────
    // EC 48 alloc; 35 08 3E 10 SEH handler; 56 57 regs saved
    // Prevents crash/disconnect triggered by Lua injection detection
    o_disconnect_with_reason = (disconnect_with_reason_t)
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\xEC\x6A\xFF\x68\x35\x08\x3E\x10\x64\xA1",
            xorstr_("xxxxxx?xxxxx"));
    if (o_disconnect_with_reason) {
        MH_CreateAndEnableHook((LPVOID)o_disconnect_with_reason,
            &h_disconnect_with_reason, (LPVOID*)&o_disconnect_with_reason);
        console->success(xorstr_("DisconnectWithReason hooked\n"));
    } else {
        console->error(xorstr_("DisconnectWithReason not found\n"));
    }

    // ── UpdateDisconnectWithReason ────────────────────────────────────────
    // 6A FE cookie (note: FE not FF); EC BC alloc; two embedded handler addrs
    // Blocks deferred kick-reason updates that happen after injection
    o_update_disconnect_with_reason = (update_disconnect_with_reason_t)
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\xEC\x6A\xFE\x68\x38\xA8\x5E\x10\x68\x16\x3B\x39\x10\x64\xA1\x00\x00\x00\x00\x50\x81\xEC",
            xorstr_("xxxxxx?xxxx?xxxxx?xxxxxx"));
    if (o_update_disconnect_with_reason) {
        MH_CreateAndEnableHook((LPVOID)o_update_disconnect_with_reason,
            &h_update_disconnect_with_reason, (LPVOID*)&o_update_disconnect_with_reason);
        console->success(xorstr_("UpdateDisconnectWithReason hooked\n"));
    } else {
        console->error(xorstr_("UpdateDisconnectWithReason not found\n"));
    }

    // ── CChecker99::Pulse (memory integrity / COW anticheat) ──────────────
    // EC 170h alloc; 6A FF cookie; CCommsRouter::Send called from inside
    // Bypasses the memory-integrity checker that detects DLL injection
    o_checker99_pulse = (checker99_pulse_t)
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\xEC\x6A\xFF\x68\x80\x28\x3D\x10\x64\xA1\x00\x00\x00\x00\x50\x81",
            xorstr_("xxxxxx?xxxxx?xxxxx"));
    if (o_checker99_pulse) {
        MH_CreateAndEnableHook((LPVOID)o_checker99_pulse,
            &h_checker99_pulse, (LPVOID*)&o_checker99_pulse);
        console->success(xorstr_("CChecker99::Pulse hooked\n"));
    } else {
        console->error(xorstr_("CChecker99::Pulse not found\n"));
    }

    // ── GetSerial (CPU/HW serial reporting) ───────────────────────────────
    // EC 6C alloc; 6A FF cookie; 8D CE 3C 10 handler; CpuToken flow inside
    // Zeroes out the serial before it reaches the master server
    o_get_serial = (get_serial_t)
        utilities::c_pattern::find_pattern(
            xorstr_("netc.dll"),
            "\x55\x8B\xEC\x6A\xFF\x68\x8D\xCE\x3C\x10\x64\xA1\x00\x00\x00\x00\x50\x83",
            xorstr_("xxxxxx?xxxxx?xxxxx"));
    if (o_get_serial) {
        MH_CreateAndEnableHook((LPVOID)o_get_serial,
            &h_get_serial, (LPVOID*)&o_get_serial);
        console->success(xorstr_("GetSerial hooked\n"));
    } else {
        console->error(xorstr_("GetSerial not found\n"));
    }

    MH_EnableHook(MH_ALL_HOOKS);
    return true;
}
