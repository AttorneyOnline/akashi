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

#include "include/area_data.h"
#include "include/config_manager.h"
#include "include/server.h"

void AOClient::addStatement(QStringList packet)
{
    if (checkTestimonySymbols(packet[4])) {
        return;
    }
    AreaData *area = server->getAreaById(m_current_area);
    int c_statement = area->statement();
    if (c_statement >= -1) {
        if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING) {
            if (c_statement <= ConfigManager::maxStatements()) {
                if (c_statement == -1)
                    packet[14] = "3";
                else
                    packet[14] = "1";
                area->recordStatement(packet);
                return;
            }
            else {
                sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
            }
        }
        else if (area->testimonyRecording() == AreaData::TestimonyRecording::ADD) {
            packet[14] = "1";
            area->addStatement(c_statement, packet);
            area->setTestimonyRecording(AreaData::TestimonyRecording::PLAYBACK);
        }
        else {
            sendServerMessage("Unable to add more statements. The maximum amount of statements has been reached.");
            area->setTestimonyRecording(AreaData::TestimonyRecording::PLAYBACK);
        }
    }
}

QStringList AOClient::updateStatement(QStringList packet)
{
    if (checkTestimonySymbols(packet[4])) {
        return packet;
    }
    AreaData *area = server->getAreaById(m_current_area);
    int c_statement = area->statement();
    area->setTestimonyRecording(AreaData::TestimonyRecording::PLAYBACK);
    if (c_statement <= 0 || area->testimony()[c_statement].empty())
        sendServerMessage("Unable to update an empty statement. Please use /addtestimony.");
    else {
        packet[14] = "1";
        area->replaceStatement(c_statement, packet);
        sendServerMessage("Updated current statement.");
        return area->testimony()[c_statement];
    }
    return packet;
}

void AOClient::clearTestimony()
{
    AreaData *area = server->getAreaById(m_current_area);
    area->clearTestimony();
}

bool AOClient::checkTestimonySymbols(const QString &message)
{
    if (message.contains('>') || message.contains('<')) {
        sendServerMessage("Unable to add statements containing '>' or '<'.");
        return true;
    }
    return false;
}
