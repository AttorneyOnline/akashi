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
#include <QObject>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>

/**
 * @brief A class for handling all log writing in the full logging mode.
 */
class WriterFull : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for full logwriter
     *
     * @details While this could've been a simple function, making it an object allows me to easier check runtime requirements
     *          when reloading the logger. It also helps split the complex stucture of a logger into easier segments.
     *
     * @param QPObject pointer to the parent object.
     */
    WriterFull(QObject* parent = nullptr);;

    /**
     * @brief Deconstructor for full logwriter.
     *
     * @details Doesn't really do anything, but its here for completeness sake.
     */
    virtual ~WriterFull() {}

public:
    /**
     * @brief Slot for u_logger to connect to when full logging is used.
     * @param Preformatted QString which will be written into the logfile.
     */
    void flush(const QString f_entry);

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

#endif //WRITER_FULL_H
