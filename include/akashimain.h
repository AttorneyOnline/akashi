#ifndef AKASHIMAIN_H
#define AKASHIMAIN_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class AkashiMain; }
QT_END_NAMESPACE

class AkashiMain : public QMainWindow
{
    Q_OBJECT

public:
    AkashiMain(QWidget *parent = nullptr);
    ~AkashiMain();

private:
    Ui::AkashiMain *ui;
};
#endif // AKASHIMAIN_H
