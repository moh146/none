\
        #pragma once
        #include <Windows.h>

        namespace utilities {
            class c_pattern {
            public:
                // pattern: pointer to raw bytes (may contain '\0'). mask: string with length indicating bytes to match; use '?' as wildcard
                static void* find_pattern(const char* module_name, const char* pattern, const char* mask) noexcept;
            };
        }
