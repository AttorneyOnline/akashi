#ifndef AKASHIMAIN_H
#define AKASHIMAIN_H

#define CONFIG_VERSION 1

#include <include/advertiser.h>

#include <QMainWindow>
#include <QSettings>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

QT_BEGIN_NAMESPACE
namespace Ui { class AkashiMain; }
QT_END_NAMESPACE

class AkashiMain : public QMainWindow
{
    Q_OBJECT

public:
    AkashiMain(QWidget *parent = nullptr);
    ~AkashiMain();

    QSettings config;

    bool initConfig();
    void generateDefaultConfig(bool backup_old);
    void updateConfig(int current_version);
private:
    Ui::AkashiMain *ui;
};
#endif // AKASHIMAIN_H
