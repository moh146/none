\
        #pragma once
        #include <string>

        class SString {
        public:
            SString() = default;
            SString(const char* s) : m_str(s ? s : "") {}
            SString(const std::string& s) : m_str(s) {}
            SString(const SString& o) : m_str(o.m_str) {}
            SString(SString&& o) noexcept : m_str(std::move(o.m_str)) {}

            SString& operator=(const char* s) { m_str = (s ? s : ""); return *this; }
            SString& operator=(const std::string& s) { m_str = s; return *this; }
            SString& operator=(const SString& o) { m_str = o.m_str; return *this; }

            const char* c_str() const noexcept { return m_str.c_str(); }
            std::string& str() noexcept { return m_str; }
            const std::string& str() const noexcept { return m_str; }

            operator const char*() const noexcept { return m_str.c_str(); }
            operator std::string() const noexcept { return m_str; }

            void clear() noexcept { m_str.clear(); }
            bool empty() const noexcept { return m_str.empty(); }

        private:
            std::string m_str;
        };
