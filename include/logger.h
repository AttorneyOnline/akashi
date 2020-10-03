#ifndef LOGGER_H
#define LOGGER_H

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>


class Logger {
  public:
    Logger();
    void addLogEntry(QDateTime msg_time, QString char_name, QString user_name,
                     QString msg_type, QString msg_subtype, QString ipid,
                     QString area_name, QString message);
    void saveLogBuffer(QDateTime msg_time, QString reason);

  private:
    bool fileExists(QFileInfo* file);
    int buffer_length;
    QList<QString> message_list;
    int message_index = 0;
};

#endif // LOGGER_H
