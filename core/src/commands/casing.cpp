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

// This file is for commands under the casing category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdDoc(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    if (argc == 0) {
        sendServerMessage("Document: " + area->document());
    }
    else {
        area->changeDoc(argv.join(" "));
        sendServerMessageArea(sender_name + " changed the document.");
    }
}

void AOClient::cmdClearDoc(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QString sender_name = ooc_name;
    AreaData* area = server->areas[current_area];
    area->changeDoc("No document.");
    sendServerMessageArea(sender_name + " cleared the document.");
}

void AOClient::cmdEvidenceMod(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData* area = server->areas[current_area];
    argv[0] = argv[0].toLower();
    if (argv[0] == "cm")
        area->setEviMod(AreaData::EvidenceMod::CM);
    else if (argv[0] == "mod")
        area->setEviMod(AreaData::EvidenceMod::MOD);
    else if (argv[0] == "hiddencm")
        area->setEviMod(AreaData::EvidenceMod::HIDDEN_CM);
    else if (argv[0] == "ffa")
        area->setEviMod(AreaData::EvidenceMod::FFA);
    else {
        sendServerMessage("Invalid evidence mod.");
        return;
    }
    sendServerMessage("Changed evidence mod.");

    // Resend evidence lists to everyone in the area
    sendEvidenceList(area);
}

void AOClient::cmdEvidence_Swap(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData* area = server->areas[current_area];
    int ev_size = area->evidence().size() -1;

    if (ev_size < 0) {
        sendServerMessage("No evidence in area.");
        return;
    }

    bool ok, ok2;
    int ev_id1 = argv[0].toInt(&ok), ev_id2 = argv[1].toInt(&ok2);

    if ((!ok || !ok2)) {
        sendServerMessage("Invalid evidence ID.");
        return;
    }
    if ((ev_id1 < 0) || (ev_id2 < 0)) {
        sendServerMessage("Evidence ID can't be negative.");
        return;
    }
    if ((ev_id2 <= ev_size) && (ev_id1 <= ev_size)) {
        area->swapEvidence(ev_id1, ev_id2);
        sendEvidenceList(area);
        sendServerMessage("The evidence " + QString::number(ev_id1) + " and " + QString::number(ev_id2) + " have been swapped.");
    }
    else {
        sendServerMessage("Unable to swap evidence. Evidence ID out of range.");
    }
}

void AOClient::cmdTestify(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData* area = server->areas[current_area];
    if (area->testimonyRecording() == AreaData::TestimonyRecording::RECORDING) {
        sendServerMessage("Testimony recording is already in progress. Please stop it before starting a new one.");
    }
    else {
        clearTestimony();
        area->setTestimonyRecording(AreaData::TestimonyRecording::RECORDING);
        sendServerMessage("Started testimony recording.");
    }
}

void AOClient::cmdExamine(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData* area = server->areas[current_area];
    if (area->testimony().size() -1 > 0)
    {
        area->restartTestimony();
        server->broadcast(AOPacket("RT",{"testimony2"}), current_area);
        server->broadcast(AOPacket("MS", {area->testimony()[0]}), current_area);
        return;
    }
    if (area->testimonyRecording() == AreaData::TestimonyRecording::PLAYBACK)
        sendServerMessage("Unable to examine while another examination is running");
    else
        sendServerMessage("Unable to start replay without prior examination.");
}

void AOClient::cmdTestimony(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

   AreaData* area = server->areas[current_area];
   if (area->testimony().size() -1 < 1) {
       sendServerMessage("Unable to display empty testimony.");
       return;
   }

   QString ooc_message;
   for (int i = 1; i <= area->testimony().size() -1; i++)
   {
       QStringList packet = area->testimony().at(i);
       QString ic_message = packet[4];
       ooc_message.append( "[" + QString::number(i) + "]" + ic_message + "\n");
   }
   sendServerMessage(ooc_message);
}

void AOClient::cmdDeleteStatement(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData* area = server->areas[current_area];
    int c_statement = area->statement();
    if (area->testimony().size() - 1 == 0) {
        sendServerMessage("Unable to delete statement. No statements saved in this area.");
    }
    if (c_statement > 0 && area->testimony().size() > 2) {
        area->removeStatement(c_statement);
        sendServerMessage("The statement with id " + QString::number(c_statement) + " has been deleted from the testimony.");
    }
}

void AOClient::cmdUpdateStatement(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    server->areas[current_area]->setTestimonyRecording(AreaData::TestimonyRecording::UPDATE);
    sendServerMessage("The next IC-Message will replace the last displayed replay message.");
}

void AOClient::cmdPauseTestimony(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData* area = server->areas[current_area];
    area->setTestimonyRecording(AreaData::TestimonyRecording::STOPPED);
    server->broadcast(AOPacket("RT",{"testimony1", "1"}), current_area);
    sendServerMessage("Testimony has been stopped.");
}

void AOClient::cmdAddStatement(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    if (server->areas[current_area]->statement() < ConfigManager::maxStatements()) {
        server->areas[current_area]->setTestimonyRecording(AreaData::TestimonyRecording::ADD);
        sendServerMessage("The next IC-Message will be inserted into the testimony.");
    }
    else
        sendServerMessage("Unable to add anymore statements. Please remove any unused ones.");
}

void AOClient::cmdSaveTestimony(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    bool permission_found = false;
    if (checkAuth(ACLFlags.value("SAVETEST")))
        permission_found = true;

    if (testimony_saving == true)
        permission_found = true;

    if (permission_found) {
        AreaData* area = server->areas[current_area];
        if (area->testimony().size() -1 <= 0) {
            sendServerMessage("Can't save an empty testimony.");
            return;
        }

        QDir dir_testimony("storage/testimony");
            if (!dir_testimony.exists()) {
                dir_testimony.mkpath(".");
            }

        QString testimony_name = argv[0].trimmed().toLower().replace("..",""); // :)
        QFile file("storage/testimony/" + testimony_name + ".txt");
        if (file.exists()) {
            sendServerMessage("Unable to save testimony. Testimony name already exists.");
            return;
        }

        QTextStream out(&file);
        out.setCodec("UTF-8");
        if(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            for (int i = 0; i <= area->testimony().size() -1; i++)
            {
                out << area->testimony().at(i).join("#") << "\n";
            }
            sendServerMessage("Testimony saved. To load it use /loadtestimony " + testimony_name);
            testimony_saving = false;
        }
    }
    else {
        sendServerMessage("You don't have permission to save a testimony. Please contact a moderator for permission.");
        return;
    }
}

void AOClient::cmdLoadTestimony(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData* area = server->areas[current_area];
    QDir dir_testimony("storage/testimony");
    if (!dir_testimony.exists()) {
        sendServerMessage("Unable to load testimonies. Testimony storage not found.");
        return;
    }

    QString testimony_name = argv[0].trimmed().toLower().replace("..",""); // :)
    QFile file("storage/testimony/" + testimony_name + ".txt");
    if (!file.exists()) {
        sendServerMessage("Unable to load testimony. Testimony name not found.");
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sendServerMessage("Unable to load testimony. Permission denied.");
        return;
    }

    clearTestimony();
    int testimony_lines = 0;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        if (testimony_lines <= ConfigManager::maxStatements()) {
            QString line = in.readLine();
            QStringList packet = line.split("#");
            area->addStatement(area->testimony().size(), packet);
            testimony_lines = testimony_lines + 1;
        }
        else {
            sendServerMessage("Testimony too large to be loaded.");
            clearTestimony();
            return;
        }
    }
    sendServerMessage("Testimony loaded successfully. Use /examine to start playback.");
}
