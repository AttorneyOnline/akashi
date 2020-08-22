#include "akashimain.h"
#include "ui_akashimain.h"

AkashiMain::AkashiMain(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AkashiMain)
{
    ui->setupUi(this);
}

AkashiMain::~AkashiMain()
{
    delete ui;
}

