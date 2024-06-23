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
#ifndef AKASHI_UTILS_H
#define AKASHI_UTILS_H

#include <QVariant>
#include <math.h>

class AkashiUtils
{
  private:
    AkashiUtils(){};

  public:
    template <typename T>
    static inline bool checkArgType(QString arg)
    {
        QVariant qvar = arg;
        if (!qvar.canConvert<T>())
            return false;

        if (std::is_same<T, int>()) {
            bool ok;
            qvar.toInt(&ok);
            return ok;
        }
        else if (std::is_same<T, float>()) {
            bool ok;
            float f = qvar.toFloat(&ok);
            return ok && !isnan((float)f);
        }
        else if (std::is_same<T, double>()) {
            bool ok;
            double d = qvar.toDouble(&ok);
            return ok && !isnan((double)d);
        }

        return true;
    };
};

#endif // AKASHI_UTILS_H
