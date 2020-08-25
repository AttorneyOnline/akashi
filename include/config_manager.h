#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#define CONFIG_VERSION 1

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

class ConfigManager {
  public:
    ConfigManager();
    bool initConfig();
    void generateDefaultConfig(bool backup_old);
    void updateConfig(int current_version);

    struct server_settings {
        QString ms_ip;
        int port;
        int ws_port;
        int local_port;
        QString name;
        QString description;
        bool advertise_server;
    };

    bool loadServerSettings(server_settings* settings);

  private:
    QSettings* config;
};

#endif // CONFIG_MANAGER_H
