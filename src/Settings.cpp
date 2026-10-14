#include "Settings.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QtLogging>
#include <fstream>

namespace Kalorite {

Settings::Settings() {
    m_data = nlohmann::json::object();
    std::ifstream file(filePath().toStdString());
    if (file.is_open()) {
        try {
            file >> m_data;
        } catch (const std::exception& e) {
            qWarning() << "Failed to parse settings, using defaults:" << e.what();
            m_data = nlohmann::json::object();
        }
    }
    if (!m_data.is_object()) m_data = nlohmann::json::object();
}

void Settings::set(const char* key, const nlohmann::json& value) {
    m_data[key] = value;
    save();
}

void Settings::save() const {
    std::ofstream file(filePath().toStdString());
    if (file.is_open()) file << m_data.dump(2);
}

QString Settings::configDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.config/kalorite";
    return dir;
}

QString Settings::filePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + "/.config/kalorite";
    QDir().mkpath(dir);
    return dir + "/settings.json";
}

} // namespace Kalorite
