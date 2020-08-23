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

private:
    QSettings* config;
};

#endif // CONFIG_MANAGER_H
