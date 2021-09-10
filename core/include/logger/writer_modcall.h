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
#ifndef WRITER_MODCALL_H
#define WRITER_MODCALL_H
#include <QObject>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QQueue>


/**
 * @brief A class to handle file interaction when writing the modcall buffer.
 */
class WriterModcall : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for modcall logwriter
     *
     * @param QObject pointer to the parent object.
     */
    WriterModcall(QObject* parent = nullptr);;

    /**
     * @brief Deconstructor for modcall logwriter.
     *
     * @details Doesn't really do anything, but its here for completeness sake.
     */
    virtual ~WriterModcall() {}

    /**
     * @brief Function to write area buffer into a logfile.
     * @param QQueue of the area that will be written into the logfile.
     * @param Name of the area for the filename.
     */
    void flush(const QString f_area_name, QQueue<QString> f_buffer);

private:
    /**
     * @brief Filename of the logfile used.
     */
    QFile l_logfile;

    /**
     * @brief Directory where logfiles will be stored.
     */
    QDir l_dir;
};

#endif //WRITER_MODCALL_H
