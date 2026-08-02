\
        netsdk_utilities - minimal replacement for missing netsdk pieces

        Files included:
         - netsdk/utilities/SString.h      : lightweight wrapper around std::string (SString replacement)
         - netsdk/utilities/pattern.h/.cpp: find_pattern(module, pattern, mask) implementation (uses Psapi)
         - netsdk/utilities/device.h/.cpp : simple c_device::set_device/get_address helper for vtable lookups

        Usage:
         1. Copy the 'netsdk' folder to your project's include path or add its parent to Additional Include Directories.
         2. Include with: #include <netsdk/utilities/SString.h>
         3. Linker: pattern.cpp uses Psapi; ensure 'Psapi.lib' is linked. pattern.cpp already contains #pragma comment(lib, "psapi.lib").
         4. After you have a valid IDirect3DDevice9* (or any object with a vtable), call:
                utilities::c_device::set_device(device_ptr);
            Then you can call:
                void* addr = reinterpret_cast<void*>(utilities::c_device::get_address(17));
        5. find_pattern expects pattern bytes and a mask string using '?' as wildcard. The pattern length must match strlen(mask).

        Notes:
         - This is a minimal, local replacement. If the original netsdk had extra utilities, extend these files accordingly.
