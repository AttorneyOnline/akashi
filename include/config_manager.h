#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#define CONFIG_VERSION 1

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

class ConfigManager{
public:
    ConfigManager(QSettings*);
    bool initConfig();
    void generateDefaultConfig(bool backup_old);
    void updateConfig(int current_version);

    bool loadServerSettings(QString* ms_ip, int* port, int* ws_port, int* local_port, QString* name, QString* description, bool* advertise_server);

private:
    QSettings* config;
};

#endif // CONFIG_MANAGER_H
