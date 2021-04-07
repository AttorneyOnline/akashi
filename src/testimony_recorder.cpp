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
#include "include/aoclient.h"

//

void AOClient::addStatement(QString packet)
{
 // This one inserts to the array in order to add new statements inbetwee. Might pull double duty to populate the testimony.
}

void AOClient::updateStatement(QString packet)
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    if ((c_statement >= 0 && !(area->testimony[c_statement].isEmpty()))) {
        sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
    }
    //Insert code to replace packet here
}

void AOClient::deleteStatement()
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    if ((c_statement > 0 && !(area->testimony[c_statement].isEmpty()))) {
        area->testimony.remove(c_statement);
        sendServerMessage("The statement with id " + QString::number(c_statement) + " has been deleted from the testimony.");
    }
    return;
}

void AOClient::clearTestimony()
{
    AreaData* area = server->areas[current_area];
    area->test_rec = AreaData::TestimonyRecording::STOPPED;
    area->testimony.clear(); //!< Empty out the QVector
    area->testimony.squeeze(); //!< Release memory. Good idea? God knows, I do not.
}

void AOClient::playTestimony()
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    server->broadcast(AOPacket("MS",area->testimony[c_statement]), current_area);
    //Send Message when end is reached and testimony loops?
}

void AOClient::pauseTestimony()
{
    AreaData* area = server->areas[current_area];
    area->test_rec = AreaData::TestimonyRecording::STOPPED;
}
