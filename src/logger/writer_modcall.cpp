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
#include "logger/writer_modcall.h"

WriterModcall::WriterModcall(QObject *parent) :
    QObject(parent)
{
    l_dir.setPath("logs/");
    if (!l_dir.exists()) {
        l_dir.mkpath(".");
    }

    l_dir.setPath("logs/modcall");
    if (!l_dir.exists()) {
        l_dir.mkpath(".");
    }
}

void WriterModcall::flush(const QString f_area_name, QQueue<QString> f_buffer)
{
    l_logfile.setFileName(QString("logs/modcall/report_%1_%2.log").arg(f_area_name, (QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"))));

    if (l_logfile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream file_stream(&l_logfile);
        file_stream.setGenerateByteOrderMark(true);

        while (!f_buffer.isEmpty())
            file_stream << f_buffer.dequeue();
    }

    l_logfile.close();
};
