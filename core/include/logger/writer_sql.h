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
#ifndef WRITER_SQL_H
#define WRITER_SQL_H

#include <QObject>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QFileInfo>
#include <QDebug>

/**
 * @brief A class to handle database interaction when executing SQL statements in SQL mode.
 */
class WriterSQL : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for SQL logwriter
     *
     * @param QObject pointer to the parent object.
     */
    WriterSQL(QObject* parent = nullptr);

    /**
     * @brief Deconstructor for SQL logwriter. Closes the underlying database
     */
    virtual ~WriterSQL();;

    /**
     * @brief Function to execute SQL queries on the log database.
     * @param SQL query execute d on the log database.
     */
    void execLogScript(QSqlQuery query);

private:

    /**
     * @brief The name of the database connection driver.
     */
    const QString DRIVER;

    /**
     * @brief The backing database that stores user details.
     */
    QSqlDatabase log_db;

    /**
     * @brief Directory where logfiles will be stored.
     */
    QDir l_dir;
};

#endif //WRITER_SQL_H
