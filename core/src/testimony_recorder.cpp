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
    int c_statement = area->m_statement;
    if (c_statement >= -1) {
        if (area->m_testimonyRecording == AreaData::TestimonyRecording::RECORDING) {
            if (c_statement <= server->maximum_statements) {
                if (c_statement == -1)
                    packet[14] = "3";
                else
                    packet[14] = "1";
                area->m_statement = c_statement + 1;
                area->m_testimony.append(packet);
                return;
            }
            else {
                sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
            }
        }
        else if (area->m_testimonyRecording == AreaData::TestimonyRecording::ADD) {
               packet[14] = "1";
               area->m_testimony.insert(c_statement,packet);
               area->m_testimonyRecording = AreaData::TestimonyRecording::PLAYBACK;
            }
            else {
                sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
                area->m_testimonyRecording = AreaData::TestimonyRecording::PLAYBACK;
            }
    }
}

QStringList AOClient::updateStatement(QStringList packet)
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->m_statement;
    area->m_testimonyRecording = AreaData::TestimonyRecording::PLAYBACK;
    if (c_statement <= 0 || area->m_testimony[c_statement].empty())
        sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
    else {
        packet[14] = "1";
        area->m_testimony.replace(c_statement, packet);
        sendServerMessage("Updated current statement.");
        return area->m_testimony[c_statement];
    }
    return packet;
}

void AOClient::clearTestimony()
{
    AreaData* area = server->areas[current_area];
    area->m_testimonyRecording = AreaData::TestimonyRecording::STOPPED;
    area->m_statement = -1;
    area->m_testimony.clear(); //!< Empty out the QVector
    area->m_testimony.squeeze(); //!< Release memory. Good idea? God knows, I do not.
}

QStringList AOClient::playTestimony()
{
    AreaData* area = server->areas[current_area];
    int c_statement = area->m_statement;
    if (c_statement > area->m_testimony.size() - 1) {
        sendServerMessageArea("Last statement reached. Looping to first statement.");
        area->m_statement = 1;
        return area->m_testimony[area->m_statement];
    }
    if (c_statement <= 0) {
        sendServerMessage("First statement reached.");
        area->m_statement = 1;
        return area->m_testimony[area->m_statement = 1];
    }
    else {
        return area->m_testimony[c_statement];
    }
}

