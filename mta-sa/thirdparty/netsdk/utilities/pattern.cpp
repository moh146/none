\
        #include "pattern.h"
        #include <Psapi.h>
        #include <cstring>

        #pragma comment(lib, "psapi.lib")

        namespace utilities {

            static MODULEINFO GetModuleInfoSafe(const char* module_name) {
                MODULEINFO modinfo = { 0 };
                HMODULE hMod = GetModuleHandleA(module_name);
                if (!hMod) return modinfo;
                if (!GetModuleInformation(GetCurrentProcess(), hMod, &modinfo, sizeof(modinfo))) {
                    ZeroMemory(&modinfo, sizeof(modinfo));
                }
                return modinfo;
            }

            void* c_pattern::find_pattern(const char* module_name, const char* pattern, const char* mask) noexcept {
                if (!module_name || !pattern || !mask) return nullptr;

                size_t len = strlen(mask);
                if (len == 0) return nullptr;

                MODULEINFO mi = GetModuleInfoSafe(module_name);
                if (!mi.lpBaseOfDll || mi.SizeOfImage == 0) return nullptr;

                unsigned char* base = reinterpret_cast<unsigned char*>(mi.lpBaseOfDll);
                size_t size = static_cast<size_t>(mi.SizeOfImage);

                for (size_t i = 0; i + len <= size; ++i) {
                    bool ok = true;
                    for (size_t j = 0; j < len; ++j) {
                        char m = mask[j];
                        unsigned char pb = static_cast<unsigned char>(pattern[j]);
                        if (m == '?') continue;
                        if (base[i + j] != pb) { ok = false; break; }
                    }
                    if (ok) {
                        return reinterpret_cast<void*>(base + i);
                    }
                }
                return nullptr;
            }

        } // namespace utilities
