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
        // TODO: start the server here
        // TODO: send signal server starting

        // Validate some of the config before passing it on
        // TODO: Move this logic into config_manager.cpp
        bool port_conversion_success;
        bool ws_port_conversion_success;
        bool local_port_conversion_success;
        config.beginGroup("Options");
            QString ms_ip = config.value("ms_ip", "master.aceattorneyonline.com").toString();
            int port = config.value("ms_port", "27016").toInt(&port_conversion_success);
            int ws_port = config.value("webao_port", "27017").toInt(&ws_port_conversion_success);
            int local_port = config.value("port", "27016").toInt(&local_port_conversion_success);
            QString name = config.value("server_name", "My First Server").toString();
            QString description = config.value("server_description", "This is my flashy new server").toString();
        config.endGroup();
        if(!port_conversion_success || !ws_port_conversion_success || !local_port_conversion_success) {
            // TODO: send signal invalid conf due to bad port number
        } else {
            if(config.value("advertise", "true").toString() != "true")
                ws_port = -1;
            advertiser = new Advertiser(ms_ip, port, ws_port, local_port, name, description);
            advertiser->contactMasterServer();
        }
    }
}

AkashiMain::~AkashiMain()
{
    delete ui;
}

