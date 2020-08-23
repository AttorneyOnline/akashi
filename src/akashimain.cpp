#include "include/akashimain.h"
#include "ui_akashimain.h"

AkashiMain::AkashiMain(QWidget *parent)
    : QMainWindow(parent)
    , config("config.ini", QSettings::IniFormat)
    , config_manager(&config)
    , ui(new Ui::AkashiMain)
{
    ui->setupUi(this);
    qDebug("Main application started");

    if(config_manager.initConfig()) {
        // Config is sound, so proceed with starting the server
        // Validate some of the config before passing it on
        QString ms_ip, name, description;
        int port, ws_port, local_port;
        bool advertise_server;
        bool config_valid = config_manager.loadServerSettings(&ms_ip, &port, &ws_port, &local_port, &name, &description, &advertise_server);

        if(!config_valid) {
            // TODO: send signal config invalid
            config_manager.generateDefaultConfig(true);
        } else {
            if(advertise_server) {
                advertiser = new Advertiser(ms_ip, port, ws_port, local_port, name, description);
                advertiser->contactMasterServer();
            }

            // TODO: start the server here
            // TODO: send signal server starting.
            server = new Server(port, ws_port);
            server->start();
        }
    }
}

AkashiMain::~AkashiMain()
{
    delete ui;
}

