#pragma once

#include <includes.h>
#include <windows.h>

// ════════════════════════════════════════════════════════════════════════════
//  c_core — hooks and bypasses targeting core.dll
//
//  Strategy:
//    Exported symbols  → resolved via GetProcAddress (zero-overhead, version-safe)
//    Internal symbols  → resolved via pattern scan (utilities::c_pattern)
//
//  Bypass map:
//    _IsFatalException       → always return false   (no fatal-exception kills)
//    _StartWatchdogThread    → return immediately    (no watchdog thread spawned)
//    _UpdateWatchdogHeartbeat→ return immediately    (heartbeat never ticks)
//    _EnableSehExceptionHandler → return false       (SEH crash handler blocked)
//    _ConfigureWerForFailFast   → return immediately (WER fail-fast disabled)
//    _SetCrashHandlerFilter     → return immediately (no crash filter installed)
//    IsDebuggerPresent       → always return 0       (anti-debug bypass)
//    OutputDebugStringA/W    → swallowed             (suppress debug-log noise)
//    screenshot enforcement  → swallowed             (screen-upload AC bypass)
// ════════════════════════════════════════════════════════════════════════════

class c_core {
private:
    // ── exported function typedefs ───────────────────────────────────────────
    typedef int  (__cdecl* is_fatal_exception_t)      (unsigned int exceptionCode);
    typedef void (__cdecl* start_watchdog_t)           (DWORD dwTimeoutMs, HANDLE hThread);
    typedef void (__cdecl* stop_watchdog_t)            ();
    typedef void (__cdecl* update_watchdog_heartbeat_t)();
    typedef bool (__cdecl* enable_seh_handler_t)       ();
    typedef void (__cdecl* configure_wer_t)            ();
    typedef void (__cdecl* set_crash_filter_t)         (void* pfnFilter);

    // ── internal (pattern-scan) typedefs ────────────────────────────────────
    // screenshot enforcement: void __thiscall CScreenshot::EnforceScreenshotRule(...)
    typedef void (__thiscall* screenshot_enforce_t)(void* ecx);

    // anti-debug: kernel32 imports hooked directly
    typedef BOOL (__stdcall* is_debugger_present_t)    ();
    typedef void (__stdcall* output_debug_string_a_t)  (LPCSTR  lpOutputString);
    typedef void (__stdcall* output_debug_string_w_t)  (LPCWSTR lpOutputString);

public:
    // originals (kept for clean trampoline calls if needed)
    is_fatal_exception_t       o_is_fatal_exception       = nullptr;
    start_watchdog_t           o_start_watchdog            = nullptr;
    stop_watchdog_t            o_stop_watchdog             = nullptr;
    update_watchdog_heartbeat_t o_update_watchdog_heartbeat = nullptr;
    enable_seh_handler_t       o_enable_seh_handler        = nullptr;
    configure_wer_t            o_configure_wer             = nullptr;
    set_crash_filter_t         o_set_crash_filter          = nullptr;
    screenshot_enforce_t       o_screenshot_enforce        = nullptr;
    is_debugger_present_t      o_is_debugger_present       = nullptr;
    output_debug_string_a_t    o_output_debug_string_a     = nullptr;
    output_debug_string_w_t    o_output_debug_string_w     = nullptr;

    bool release();
};

inline c_core* core_bypass = new c_core();
