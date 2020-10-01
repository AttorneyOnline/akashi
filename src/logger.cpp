#include "include/logger.h"

Logger::Logger()
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    buffer_length = config.value("buffer_length", "500").toInt();
}

void addLogEntry(QDateTime time, QString char_name, QString user_name,
                 QString msg_type, QString msg_subtype, QString ipid,
                 QString area, QString message)
{
    QString final_entry =
        ('[' + time::toString() + "][" + area + "][" + msg_type + "][" +
         msg_subtype + "] " + char_name + '/' + user_name + " (" + ipid +
         "): " + message + "\n");
    message_list[message_index % buffer_length] = final_entry;
    message_index++;
}

void saveLogBuffer(QDateTime time, QString reason)
{
    QFileInfo logs_dir_info("logs/");
    if (!logs_dir_info.exists() || !logs_dir_info.isDir()) {
        qInfo() << "Logs directory doesn't exist, creating it";
        if (!QDir::mkpath("logs/")) {
            qCritical()
                << "Couldn't create logs directory! Aborting buffer dump";
            return;
        }
    }
    QFile buffer_dump("logs/buffer-" + time::toString().replace(':', '-'));
    if (buffer_dump.open(QIODevice::ReadWrite)) {
        QTextStream stream(&buffer_dump);
        stream << "akashi dumped the message buffer to this file...\nAt: "
               << time::toString() << "\nBecause: " << reason
               << "\n===BEGIN MESSAGE BUFFER===\n"
               << message_list;
    }
    else:
        qCritical() << "Unable to open log file! Aborting buffer dump";
}