\
        #pragma once
        #include <cstdint>
        #include <cstddef>

        namespace utilities {
            class c_device {
            public:
                static void set_device(void* device) noexcept { s_device = reinterpret_cast<uintptr_t>(device); }
                static uintptr_t get_address(std::size_t index) noexcept {
                    if (s_device == 0) return 0;
                    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(reinterpret_cast<void*>(s_device));
                    if (!vtable) return 0;
                    return vtable[index];
                }
            private:
                static uintptr_t s_device;
            };
        }
