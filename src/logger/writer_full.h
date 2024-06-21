//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#ifndef WRITER_FULL_H
#define WRITER_FULL_H
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QTextStream>

/**
 * @brief A class to handle file interaction when writing in full log mode.
 */
class WriterFull : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief Constructor for full logwriter
     *
     * @param QObject pointer to the parent object.
     */
    WriterFull(QObject *parent = nullptr);
    ;

    /**
     * @brief Deconstructor for full logwriter.
     *
     * @details Doesn't really do anything, but its here for completeness sake.
     */
    virtual ~WriterFull() {}

    /**
     * @brief Function to write log entry into a logfile.
     * @param Preformatted QString which will be written into the logfile.
     */
    void flush(const QString f_entry);

    /**
     * @brief Writes log entry into area seperated logfiles.
     * @param Preformatted QString which will be written into the logfile
     * @param Area name of the target logfile.
     */
    void flush(const QString f_entry, const QString f_area_name);

  private:
    /**
     * @brief Filename of the logfile used. This will always be the time the server starts up.
     */
    QFile l_logfile;

    /**
     * @brief Directory where logfiles will be stored.
     */
    QDir l_dir;
};

#endif // WRITER_FULL_H
