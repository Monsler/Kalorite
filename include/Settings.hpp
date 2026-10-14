#pragma once

#include <nlohmann/json.hpp>
#include <QString>

namespace Kalorite {

    // Persistent key/value store backed by settings.json in the app config directory.
    class Settings {
    public:
        Settings();

        template <typename T>
        T get(const char* key, const T& fallback) const { return m_data.value(key, fallback); }
        bool contains(const char* key) const { return m_data.contains(key); }
        nlohmann::json& data() { return m_data; }

        void set(const char* key, const nlohmann::json& value);
        void save() const;

        static QString configDir();

    private:
        static QString filePath();

        nlohmann::json m_data;
    };

} // namespace Kalorite
