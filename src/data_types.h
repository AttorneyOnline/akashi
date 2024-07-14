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
#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <QDebug>
#include <QVariant>

/**
 * @brief A class for handling several custom data types.
 */
class DataTypes
{
    Q_GADGET

  public:
    /**
     * @brief Custom type for authorization types.
     */
    enum class AuthType
    {
        SIMPLE,
        ADVANCED
    };
    Q_ENUM(AuthType);

    /**
     * @brief Custom type for logging types.
     */
    enum class LogType
    {
        MODCALL,
        FULL,
        FULLAREA
    };
    Q_ENUM(LogType)
};

template <typename T>
T toDataType(const QString &f_string)
{
    return QVariant(f_string).value<T>();
}

template <typename T>
QString fromDataType(const T &f_t)
{
    return QVariant::fromValue(f_t).toString();
}

#endif // DATA_TYPES_H
