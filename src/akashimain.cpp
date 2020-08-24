#include "include/akashimain.h"
#include "ui_akashimain.h"

AkashiMain::AkashiMain(QWidget *parent)
    : QMainWindow(parent), config_manager(), ui(new Ui::AkashiMain)
{
  ui->setupUi(this);
  qDebug("Main application started");

  if (config_manager.initConfig()) {
    // Config is sound, so proceed with starting the server
    // Validate some of the config before passing it on
    ConfigManager::server_settings settings;
    bool config_valid = config_manager.loadServerSettings(&settings);

    if (!config_valid) {
      // TODO: send signal config invalid
      config_manager.generateDefaultConfig(true);
    }
    else {
      if (settings.advertise_server) {
        advertiser = new Advertiser(settings.ms_ip, settings.port,
                                    settings.ws_port, settings.local_port,
                                    settings.name, settings.description, this);
        advertiser->contactMasterServer();
      }

      // TODO: start the server here
      // TODO: send signal server starting.
      server = new Server(settings.port, settings.ws_port);
      server->start();
    }
  }
}

AkashiMain::~AkashiMain() { delete ui; }
