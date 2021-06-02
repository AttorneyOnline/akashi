#ifndef LOGGER2_H
#define LOGGER2_H

#include <QObject>
#include <QQueue>
#include "include/server.h"
#include "include/area_data.h"


/**
 * @brief The logger2 class A more advanced version of the per-area logger, unifying its operation in one class that is independant of each area.
 */
class logger2 : public QObject
{
    Q_OBJECT
public:

    /**
     * @brief logger2 Constructor for the logger2 class.
     * @param parent Pointer to the server.
     */
    explicit logger2(QObject *parent = nullptr);

    /**
     * @brief The logLevel enum Asthetic explanation of the log level used by the server.
     *        Determined what write the logger connects to during server startup and makes it look pretty.
     *        The higher the enum, the more data is typically logged.
     */
    enum logLevel {
        AREA, //!< The logger only writes the logs inside the area buffer to disk.
        FULL, //!< The logger writes all events into a single log file.
        SORTED, //!< The logger writes all events into multiple log files sorted by type.
        SQL //!< Tsu3 compatability mode (I am so not gonna implement this.)
    };

    /// Exposes the metadata of the logLevel enum.
    Q_ENUM(logLevel);

signals:

public slots:

    /**
     * @brief writeAreaLog Triggers the writing of the supplied area buffer into an area log file.
     * @param f_areaName Name of the area the modcall is happening in.
     * @param f_buffer Copy of the message buffer of that area.
     */
    void writeAreaLog(QString f_areaName, QQueue<QString> f_buffer);

    /**
     * @brief writeFullFileLog Writes any event in order into a single log file.
     * @param f_buffer Logable events to be written in the file.
     */
    void writeFullFileLog(QQueue<QString> f_buffer);

    /**
     * @brief writeSortedFileLog The bane of my fucking existence.
     */
    void writeSortedFileLog();

    /**
     * @brief writeSQL (I am so not gonna implement this.) Essentially the Tsu3 compatability mode writer wise. Tries to write in a Tsu3DB compatible format.
     *                 Let the records show that everyone was against it and I leave it here for someone else to implement it.
     */
    void writeSQL();

    /**
     * @brief Logs an IC message.
     *
     * @param f_charName_r The character name of the client who sent the IC message.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_message_r The text of the IC message.
     */
    void logIC(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_message_r);

    /**
     * @brief Logs an OOC message.
     *
     * @param f_areaName_r The name of the area where the event happened.
     * @param f_charName_r The character name of the client who sent the OOC message.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_message_r The text of the OOC message.
     */
    void logOOC(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_message_r);

    /**
     * @brief Logs a mod call message.
     *
     * @param f_charName_r The character name of the client who sent the mod call.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_modcallReason_r The reason for the modcall.
     */
    void logModcall(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_modcallReason_r);

    /**
     * @brief Logs a command called in OOC.
     *
     * @details If the command is not one of any of the 'special' ones, it defaults to logOOC().
     * The only thing that makes a command 'special' if it is handled differently in here.
     *
     * @param f_charName_r The character name of the client who sent the command.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param f_oocMessage_r The text of the OOC message. Passed to logOOC() if the command is not 'special' (see details).
     */
    void logCmd(const QString& f_charName_r, const QString& f_ipid_r, const QString& f_oocMessage_r);

    /**
     * @brief Logs a login attempt.
     *
     * @param f_charName_r The character name of the client that attempted to login.
     * @param f_ipid_r The IPID of the aforementioned client.
     * @param success True if the client successfully authenticated as a mod.
     * @param f_modname_r If the client logged in with a modname, then this is it. Otherwise, it's `"moderator"`.
     */
    void logLogin(const QString& f_charName_r, const QString& f_ipid_r, bool success, const QString& f_modname_r);

private:

    void writeToAreaBuffer(AreaData* f_area, const QString& f_charName_r, const QString& f_ipid_r,
                           bool success, const QString& f_modname_r);

};

#endif // LOGGER2_H
