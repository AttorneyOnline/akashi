#include "include/akashimain.h"
#include "ui_akashimain.h"

#include <QDebug>

AkashiMain::AkashiMain(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AkashiMain)
{
    ui->setupUi(this);
    qDebug("Main application started");
}

AkashiMain::~AkashiMain()
{
    delete ui;
}

