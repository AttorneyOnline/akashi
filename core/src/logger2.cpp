#include "include/logger2.h"

logger2::logger2(QObject *parent) : QObject(parent)
{
    QDir l_dir("logs/");
    if (!l_dir.exists()) {
        l_dir.mkpath(".");
    }
}

void logger2::writeAreaLog(QString f_areaName, QQueue<QString> f_buffer)
{
    QFile l_logfile;
    l_logfile.setFileName(QString("logs/report_%1_%2.log").arg(f_areaName, (QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"))));
    if (l_logfile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream file_stream(&l_logfile);

        while (!f_buffer.isEmpty())
            file_stream << f_buffer.dequeue();
    }

    l_logfile.close();
}

void logger2::writeFullFileLog(QQueue<QString> f_buffer)
{
    QFile l_logfile;
    l_logfile.setFileName(QString("logs/%1.log").arg(QDate::currentDate().toString("yyyy-MM-dd")));
    if (l_logfile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream file_stream(&l_logfile);

        while (!f_buffer.isEmpty())
            file_stream << f_buffer.dequeue();
    }

    l_logfile.close();
}

void logger2::writeSortedFileLog()
{

}

void logger2::writeSQL()
{

}
