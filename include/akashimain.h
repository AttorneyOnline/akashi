#ifndef AKASHIMAIN_H
#define AKASHIMAIN_H

#include "include/advertiser.h"
#include "include/config_manager.h"
#include "include/server.h"

#include <QDebug>
#include <QMainWindow>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui {
class AkashiMain;
}
QT_END_NAMESPACE

class AkashiMain : public QMainWindow {
    Q_OBJECT

  public:
    AkashiMain(QWidget* parent = nullptr);
    ~AkashiMain();

    ConfigManager config_manager;

    void generateDefaultConfig(bool backup_old);
    void updateConfig(int current_version);

  private:
    Ui::AkashiMain* ui;
    Advertiser* advertiser;
    Server* server;
};
#endif // AKASHIMAIN_H
