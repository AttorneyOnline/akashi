#include "include/logger.h"

Logger::Logger()
{
    QSettings config("config/config.ini", QSettings::IniFormat);
    config.beginGroup("Options");
    buffer_length = config.value("buffer_length", "500").toInt();
}

void Logger::addLogEntry(QDateTime msg_time, QString char_name, QString user_name,
                        QString msg_type, QString msg_subtype, QString ipid,
                        QString area_name, QString message)
{
    QString final_entry =
        QStringLiteral("[%1][%2][%3][%4] %5/%6 (%7): %8\n")
            .arg(msg_time.toString())
            .arg(area_name)
            .arg(msg_type)    // IC or OOC
            .arg(msg_subtype) // local, global, mod/local, mod/global, etc
            .arg(char_name)
            .arg(user_name) // OOC name
            .arg(ipid)
            .arg(message);
    message_list[message_index % buffer_length] = final_entry;
    message_index++;
}

void Logger::saveLogBuffer(QDateTime msg_time, QString reason)
{
    QFileInfo logs_dir_info("logs/");
    if (!logs_dir_info.exists() || !logs_dir_info.isDir()) {
        qInfo() << "Logs directory doesn't exist, creating it";
        QDir log_dir;
        if (!log_dir.mkpath("logs/")) {
            qCritical()
                << "Couldn't create logs directory! Aborting buffer dump";
            return;
        }
    }
    QFile buffer_dump("logs/buffer-" + msg_time.toString().replace(':', '-'));
    if (buffer_dump.open(QIODevice::ReadWrite)) {
        QTextStream stream(&buffer_dump);
        stream << "akashi dumped the message buffer to this file...\nAt: "
               << msg_time.toString() << "\nBecause: " << reason
               << "\n===BEGIN MESSAGE BUFFER===\n";
        for (int i=0; i<message_list.count(); ++i ) {
            stream << message_list[i];
        }
    }
    else
        qCritical() << "Unable to open log file! Aborting buffer dump";
}
