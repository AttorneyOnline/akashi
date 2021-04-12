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

void AOClient::addStatement(QStringList packet)
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    if (c_statement >= 0) {
        if (area->test_rec == AreaData::TestimonyRecording::RECORDING) {
            if (c_statement <= 50) { //Make this configurable once Mangos ConfigManager changes get merged
                if (c_statement == 0)
                    packet[14] = "3";
                else
                    packet[14] = "1";
                area->testimony.append(packet);
                area->statement = c_statement + 1;
                return;
            }
            else {
                sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
            }
        }
        else if (area->test_rec == AreaData::TestimonyRecording::ADD) {
            if (c_statement < 50) { //Make this configurable once Mangos ConfigManager changes get merged
               area->testimony.insert(c_statement,packet);
               area->test_rec = AreaData::TestimonyRecording::PLAYBACK;
            }
            else {
                sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
                area->test_rec = AreaData::TestimonyRecording::PLAYBACK;
            }
        }
    }


}

QStringList AOClient::updateStatement(QStringList packet)
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    area->test_rec = AreaData::TestimonyRecording::PLAYBACK;
    if (c_statement <= 0 || area->testimony[c_statement].empty())
        sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
    else {
        packet[14] = "1";
        area->testimony.replace(c_statement, packet);
        sendServerMessage("Updated current statement.");
        return area->testimony[c_statement];
    }
    return packet;
}

void AOClient::deleteStatement()
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    if ((c_statement > 0 && !(area->testimony[c_statement].isEmpty()))) {
        area->testimony.remove(c_statement);
        sendServerMessage("The statement with id " + QString::number(c_statement) + " has been deleted from the testimony.");
    }
    server->areas[current_area]->test_rec = AreaData::TestimonyRecording::PLAYBACK;
}

void AOClient::clearTestimony()
{
    AreaData* area = server->areas[current_area];
    area->test_rec = AreaData::TestimonyRecording::STOPPED;
    area->testimony.clear(); //!< Empty out the QVector
    area->testimony.squeeze(); //!< Release memory. Good idea? God knows, I do not.
}

QStringList AOClient::playTestimony()
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->statement;
    if (c_statement > area->testimony.size() - 1) {
        sendServerMessageArea("Last statement reached. Looping to first statement.");
        area->statement = 1;
        return area->testimony[area->statement];
    }
    if (c_statement <= 0) {
        sendServerMessage("First statement reached.");
        area->statement = 1;
        return area->testimony[area->statement = 1];
    }
    else {
        return area->testimony[c_statement];
    }
}

void AOClient::pauseTestimony()
{
    AreaData* area = server->areas[current_area];
    area->test_rec = AreaData::TestimonyRecording::STOPPED;
    sendServerMessage("Testimony has been stopped.");
}
