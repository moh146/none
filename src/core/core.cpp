#include "core.h"
#include <utilities/utilities.h>
#include <console/console.h>

// ════════════════════════════════════════════════════════════════════════════
//  Helper — resolve exported symbol from core.dll with console feedback
// ════════════════════════════════════════════════════════════════════════════
static DWORD resolve_export(const char* dll, const char* sym)
{
    HMODULE hMod = GetModuleHandleA(dll);
    if (!hMod) return 0;
    return reinterpret_cast<DWORD>(GetProcAddress(hMod, sym));
}

static void hook(LPVOID target, LPVOID detour, LPVOID* original, const char* tag)
{
    if (!target) {
        console->error(xorstr_("[core] %s — address not found\n"), tag);
        return;
    }
    MH_CreateHook(target, detour, original);
    MH_EnableHook(target);
    console->success(xorstr_("[core] %s hooked\n"), tag);
}

// ════════════════════════════════════════════════════════════════════════════
//  1. _IsFatalException@4
//     MTA calls this to decide whether an exception should terminate the
//     process.  Always returning 0 keeps the game alive on soft exceptions
//     (e.g. the OOM we diagnosed) instead of forcing a hard crash.
// ════════════════════════════════════════════════════════════════════════════
int __cdecl h_is_fatal_exception(unsigned int exceptionCode)
{
    // 0xE03B10C0 = MTA OOM exception — never treat it as fatal
    // BREAKPOINT  = 0x80000003 — non-fatal when no debugger attached
    // All others: let the game recover on its own
    return 0;
}

// ════════════════════════════════════════════════════════════════════════════
//  2. _StartWatchdogThread@8
//     The watchdog spawns a monitor thread that kills the process when
//     _UpdateWatchdogHeartbeat stops being called (e.g. the main thread
//     freezes).  Blocking it prevents false-positive kills when our hooks
//     stall the thread momentarily (heavy script dump, DLL injection, etc.).
// ════════════════════════════════════════════════════════════════════════════
void __cdecl h_start_watchdog(DWORD /*dwTimeoutMs*/, HANDLE /*hThread*/)
{
    // silently drop — no watchdog thread is created
}

// ════════════════════════════════════════════════════════════════════════════
//  3. _UpdateWatchdogHeartbeat@0
//     Even if the watchdog somehow starts, poisoning the heartbeat prevents
//     it from accumulating a timeout counter.
// ════════════════════════════════════════════════════════════════════════════
void __cdecl h_update_watchdog_heartbeat()
{
    // silently drop
}

// ════════════════════════════════════════════════════════════════════════════
//  4. _EnableSehExceptionHandler@0
//     MTA installs its own SEH filter early in startup to intercept all
//     unhandled exceptions and write crash dumps.  Blocking it means our
//     injected code's exceptions won't be caught and reported by MTA.
// ════════════════════════════════════════════════════════════════════════════
bool __cdecl h_enable_seh_handler()
{
    return false; // pretend handler was not installed
}

// ════════════════════════════════════════════════════════════════════════════
//  5. _ConfigureWerForFailFast@0
//     WER (Windows Error Reporting) fail-fast causes the OS to terminate
//     the process instantly on certain exceptions without giving our SEH
//     chain a chance to run.  Blocking it keeps our handlers in control.
// ════════════════════════════════════════════════════════════════════════════
void __cdecl h_configure_wer()
{
    // silently drop
}

// ════════════════════════════════════════════════════════════════════════════
//  6. _SetCrashHandlerFilter@4
//     Prevents MTA from registering a top-level exception filter.  Without
//     the filter, crashes are not intercepted and written to core.log, so
//     MTA's crash reporting pipeline is effectively disabled.
// ════════════════════════════════════════════════════════════════════════════
void __cdecl h_set_crash_filter(void* /*pfnFilter*/)
{
    // silently drop
}

// ════════════════════════════════════════════════════════════════════════════
//  7. Screenshot enforcement bypass (internal, pattern-scanned)
//     Some servers force a screen capture and upload it as anti-cheat
//     evidence.  This hook drops the enforcement call so no screenshot
//     is ever taken or uploaded.
//
//     Pattern (core.dll): prologue of the screenshot-enforce function
//     found by tracing the "Screen upload is required" string xref.
//     Pattern: \x55\x8B\xEC\x83\xEC\x??\x56\x57\x8B\xF9
//     mask:    "xxxxxx?xxx"
// ════════════════════════════════════════════════════════════════════════════
void __fastcall h_screenshot_enforce(void* /*ecx*/, void* /*edx*/)
{
    // silently drop — no screenshot taken, nothing uploaded
}

// ════════════════════════════════════════════════════════════════════════════
//  8. IsDebuggerPresent (kernel32)
//     MTA polls this to detect attached debuggers and refuse to run (or
//     send AC reports) when one is found.  Always returning FALSE hides
//     any attached debugger.
// ════════════════════════════════════════════════════════════════════════════
BOOL __stdcall h_is_debugger_present()
{
    return FALSE;
}

// ════════════════════════════════════════════════════════════════════════════
//  9. OutputDebugStringA / W (kernel32)
//     MTA writes sensitive diagnostic strings (hook names, cheat alerts,
//     memory addresses) to the debug output.  Swallowing them prevents
//     external monitors (DbgView, WinDbg) from passively reading this data.
// ════════════════════════════════════════════════════════════════════════════
void __stdcall h_output_debug_string_a(LPCSTR  /*lpOutputString*/) { }
void __stdcall h_output_debug_string_w(LPCWSTR /*lpOutputString*/) { }

// ════════════════════════════════════════════════════════════════════════════
//  c_core::release() — resolve all targets and install hooks
// ════════════════════════════════════════════════════════════════════════════
bool c_core::release()
{
    // ── 1. _IsFatalException@4 ──────────────────────────────────────────────
    // Exported — resolve directly; mask cookie bytes in pattern fallback
    // Primary:   GetProcAddress
    // Fallback:  pattern \x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\xA1\x??\x??\x??\x??\x33\xC5\x89\x45\xFC\x56\x8B\x75\x08\x81
    //            mask     xxxxxxxxxx????xxxxxxxxxx
    o_is_fatal_exception = reinterpret_cast<is_fatal_exception_t>(
        resolve_export("core.dll", "_IsFatalException@4"));

    if (!o_is_fatal_exception)
        o_is_fatal_exception = reinterpret_cast<is_fatal_exception_t>(
            utilities::c_pattern::find_pattern(
                "core.dll",
                "\x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x56\x8B\x75\x08\x81",
                "xxxxxxxxxx????xxxxxxxxxx"));

    hook(reinterpret_cast<LPVOID>(o_is_fatal_exception),
         reinterpret_cast<LPVOID>(h_is_fatal_exception),
         reinterpret_cast<LPVOID*>(&o_is_fatal_exception),
         "_IsFatalException");

    // ── 2. _StartWatchdogThread@8 ───────────────────────────────────────────
    // Pattern: \x55\x8B\xEC\x83\xEC\x28\xA1\x??\x??\x??\x??\x33\xC5\x89\x45\xFC\x8B\x55\x08\xB1\x01\x56\x57\x8B
    // mask:    xxxxxxx????xxxxxxxxxxxxx
    o_start_watchdog = reinterpret_cast<start_watchdog_t>(
        resolve_export("core.dll", "_StartWatchdogThread@8"));

    if (!o_start_watchdog)
        o_start_watchdog = reinterpret_cast<start_watchdog_t>(
            utilities::c_pattern::find_pattern(
                "core.dll",
                "\x55\x8B\xEC\x83\xEC\x28\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x8B\x55\x08\xB1\x01\x56\x57\x8B",
                "xxxxxxx????xxxxxxxxxxxxx"));

    hook(reinterpret_cast<LPVOID>(o_start_watchdog),
         reinterpret_cast<LPVOID>(h_start_watchdog),
         reinterpret_cast<LPVOID*>(&o_start_watchdog),
         "_StartWatchdogThread");

    // ── 3. _UpdateWatchdogHeartbeat@0 ──────────────────────────────────────
    // Pattern: \x55\x8B\xEC\xA0\x00\xA3\x2C\x10\x83\xEC\x08\x90\x84\xC0\x74\x25
    // mask:    xxxxxxxxxxxxxxxx
    o_update_watchdog_heartbeat = reinterpret_cast<update_watchdog_heartbeat_t>(
        resolve_export("core.dll", "_UpdateWatchdogHeartbeat@0"));

    if (!o_update_watchdog_heartbeat)
        o_update_watchdog_heartbeat = reinterpret_cast<update_watchdog_heartbeat_t>(
            utilities::c_pattern::find_pattern(
                "core.dll",
                "\x55\x8B\xEC\xA0\x00\xA3\x2C\x10\x83\xEC\x08\x90\x84\xC0\x74\x25",
                "xxxxxxxxxxxxxxxx"));

    hook(reinterpret_cast<LPVOID>(o_update_watchdog_heartbeat),
         reinterpret_cast<LPVOID>(h_update_watchdog_heartbeat),
         reinterpret_cast<LPVOID*>(&o_update_watchdog_heartbeat),
         "_UpdateWatchdogHeartbeat");

    // ── 4. _EnableSehExceptionHandler@0 ────────────────────────────────────
    // Exported thunk: E8 CB A1 FF FF → points to real impl at +0x10
    // We hook the actual implementation (offset +16 from the thunk start)
    // Pattern: \x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x56\x90
    // mask:    xxxxxxxxxx????xxxxxxx
    {
        DWORD thunk = resolve_export("core.dll", "_EnableSehExceptionHandler@0");
        DWORD impl  = thunk ? (thunk + 16) : 0; // skip the thunk stub

        if (!impl)
            impl = utilities::c_pattern::find_pattern(
                "core.dll",
                "\x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x56\x90",
                "xxxxxxxxxx????xxxxxxx");

        o_enable_seh_handler = reinterpret_cast<enable_seh_handler_t>(impl);
        hook(reinterpret_cast<LPVOID>(o_enable_seh_handler),
             reinterpret_cast<LPVOID>(h_enable_seh_handler),
             reinterpret_cast<LPVOID*>(&o_enable_seh_handler),
             "_EnableSehExceptionHandler");
    }

    // ── 5. _ConfigureWerForFailFast@0 ──────────────────────────────────────
    // Exported thunk: E8 EB 87 FF FF — the real impl is at +16 as well
    // Pattern (real impl): \x55\x8B\xEC\x81\xEC\x04\x01\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC
    // mask:                xxxxxxxxxx????xxxxx
    {
        DWORD thunk = resolve_export("core.dll", "_ConfigureWerForFailFast@0");
        DWORD impl  = thunk ? (thunk + 16) : 0;

        if (!impl)
            impl = utilities::c_pattern::find_pattern(
                "core.dll",
                "\x55\x8B\xEC\x81\xEC\x04\x01\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC",
                "xxxxxxxxxx????xxxxx");

        o_configure_wer = reinterpret_cast<configure_wer_t>(impl);
        hook(reinterpret_cast<LPVOID>(o_configure_wer),
             reinterpret_cast<LPVOID>(h_configure_wer),
             reinterpret_cast<LPVOID*>(&o_configure_wer),
             "_ConfigureWerForFailFast");
    }

    // ── 6. _SetCrashHandlerFilter@4 ────────────────────────────────────────
    // Pattern: \x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x56\x8B\x75\x08\x85
    // mask:    xxxxxxxxxx????xxxxxxxxxx
    o_set_crash_filter = reinterpret_cast<set_crash_filter_t>(
        resolve_export("core.dll", "_SetCrashHandlerFilter@4"));

    if (!o_set_crash_filter)
        o_set_crash_filter = reinterpret_cast<set_crash_filter_t>(
            utilities::c_pattern::find_pattern(
                "core.dll",
                "\x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\xA1\x00\x00\x00\x00\x33\xC5\x89\x45\xFC\x56\x8B\x75\x08\x85",
                "xxxxxxxxxx????xxxxxxxxxx"));

    hook(reinterpret_cast<LPVOID>(o_set_crash_filter),
         reinterpret_cast<LPVOID>(h_set_crash_filter),
         reinterpret_cast<LPVOID*>(&o_set_crash_filter),
         "_SetCrashHandlerFilter");

    // ── 7. Screenshot enforcement (internal, pattern scan) ──────────────────
    // Xref: "Screen upload is required by some servers for anti-cheat purposes."
    // Prologue of the enforcer function uses a standard alloca frame then
    // loads the screenshot manager pointer.
    // Pattern: \x55\x8B\xEC\x83\xEC\x00\x56\x57\x8B\xF9
    // mask:    "xxxxx?xxxx"
    o_screenshot_enforce = reinterpret_cast<screenshot_enforce_t>(
        utilities::c_pattern::find_pattern(
            "core.dll",
            "\x55\x8B\xEC\x83\xEC\x00\x56\x57\x8B\xF9",
            "xxxxx?xxxx"));

    if (o_screenshot_enforce) {
        hook(reinterpret_cast<LPVOID>(o_screenshot_enforce),
             reinterpret_cast<LPVOID>(h_screenshot_enforce),
             reinterpret_cast<LPVOID*>(&o_screenshot_enforce),
             "ScreenshotEnforce");
    } else {
        console->warn(xorstr_("[core] ScreenshotEnforce — pattern not matched (non-fatal)\n"));
    }

    // ── 8. IsDebuggerPresent (kernel32) ─────────────────────────────────────
    o_is_debugger_present = reinterpret_cast<is_debugger_present_t>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsDebuggerPresent"));

    hook(reinterpret_cast<LPVOID>(o_is_debugger_present),
         reinterpret_cast<LPVOID>(h_is_debugger_present),
         reinterpret_cast<LPVOID*>(&o_is_debugger_present),
         "IsDebuggerPresent");

    // ── 9. OutputDebugStringA / W ────────────────────────────────────────────
    HMODULE hK32 = GetModuleHandleA("kernel32.dll");

    o_output_debug_string_a = reinterpret_cast<output_debug_string_a_t>(
        GetProcAddress(hK32, "OutputDebugStringA"));
    hook(reinterpret_cast<LPVOID>(o_output_debug_string_a),
         reinterpret_cast<LPVOID>(h_output_debug_string_a),
         reinterpret_cast<LPVOID*>(&o_output_debug_string_a),
         "OutputDebugStringA");

    o_output_debug_string_w = reinterpret_cast<output_debug_string_w_t>(
        GetProcAddress(hK32, "OutputDebugStringW"));
    hook(reinterpret_cast<LPVOID>(o_output_debug_string_w),
         reinterpret_cast<LPVOID>(h_output_debug_string_w),
         reinterpret_cast<LPVOID*>(&o_output_debug_string_w),
         "OutputDebugStringW");

    MH_EnableHook(MH_ALL_HOOKS);
    console->success(xorstr_("[core] all core.dll bypasses installed\n"));
    return true;
}
