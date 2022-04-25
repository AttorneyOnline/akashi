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

#include "include/aopacket.h"
#include "include/area_data.h"
#include "include/config_manager.h"
#include "include/server.h"

// This file is for commands under the roleplay category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdFlip(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QString l_sender_name = m_ooc_name;
    QStringList l_faces = {"heads", "tails"};
    QString l_face = l_faces[AOClient::genRand(0, 1)];
    sendServerMessageArea(l_sender_name + " flipped a coin and got " + l_face + ".");
}

void AOClient::cmdRoll(int argc, QStringList argv)
{
    diceThrower(argc, argv, false);
}

void AOClient::cmdRollP(int argc, QStringList argv)
{
    diceThrower(argc, argv, true);
}

void AOClient::cmdTimer(int argc, QStringList argv)
{
    AreaData *l_area = server->getAreaById(m_current_area);

    // Called without arguments
    // Shows a brief of all timers
    if (argc == 0) {
        QStringList l_timers;
        l_timers.append("Currently active timers:");
        for (int i = 0; i <= 4; i++) {
            l_timers.append(getAreaTimer(l_area->index(), i));
        }
        sendServerMessage(l_timers.join("\n"));
        return;
    }

    // Called with more than one argument
    bool ok;
    int l_timer_id = argv[0].toInt(&ok);
    if (!ok || l_timer_id < 0 || l_timer_id > 4) {
        sendServerMessage("Invalid timer ID. Timer ID must be a whole number between 0 and 4.");
        return;
    }

    // Called with one argument
    // Shows the status of one timer
    if (argc == 1) {
        sendServerMessage(getAreaTimer(l_area->index(), l_timer_id));
        return;
    }

    // Called with more than one argument
    // Updates the state of a timer

    // Select the proper timer
    // Check against permissions if global timer is selected
    QTimer *l_requested_timer;
    if (l_timer_id == 0) {
        if (!checkAuth(ACLFlags.value("GLOBAL_TIMER"))) {
            sendServerMessage("You are not authorized to alter the global timer.");
            return;
        }
        l_requested_timer = server->timer;
    }
    else
        l_requested_timer = l_area->timers().at(l_timer_id - 1);

    AOPacket l_show_timer("TI", {QString::number(l_timer_id), "2"});
    AOPacket l_hide_timer("TI", {QString::number(l_timer_id), "3"});
    bool l_is_global = l_timer_id == 0;

    // Set the timer's time remaining if the second
    // argument is a valid time
    QTime l_requested_time = QTime::fromString(argv[1], "hh:mm:ss");
    if (l_requested_time.isValid()) {
        l_requested_timer->setInterval(QTime(0, 0).msecsTo(l_requested_time));
        l_requested_timer->start();
        sendServerMessage("Set timer " + QString::number(l_timer_id) + " to " + argv[1] + ".");
        AOPacket l_update_timer("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(l_requested_time))});
        l_is_global ? server->broadcast(l_show_timer) : server->broadcast(l_show_timer, m_current_area); // Show the timer
        l_is_global ? server->broadcast(l_update_timer) : server->broadcast(l_update_timer, m_current_area);
        return;
    }
    // Otherwise, update the state of the timer
    else {
        if (argv[1] == "start") {
            l_requested_timer->start();
            sendServerMessage("Started timer " + QString::number(l_timer_id) + ".");
            AOPacket l_update_timer("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_requested_timer->remainingTime())))});
            l_is_global ? server->broadcast(l_show_timer) : server->broadcast(l_show_timer, m_current_area);
            l_is_global ? server->broadcast(l_update_timer) : server->broadcast(l_update_timer, m_current_area);
        }
        else if (argv[1] == "pause" || argv[1] == "stop") {
            l_requested_timer->setInterval(l_requested_timer->remainingTime());
            l_requested_timer->stop();
            sendServerMessage("Stopped timer " + QString::number(l_timer_id) + ".");
            AOPacket l_update_timer("TI", {QString::number(l_timer_id), "1", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_requested_timer->interval())))});
            l_is_global ? server->broadcast(l_update_timer) : server->broadcast(l_update_timer, m_current_area);
        }
        else if (argv[1] == "hide" || argv[1] == "unset") {
            l_requested_timer->setInterval(0);
            l_requested_timer->stop();
            sendServerMessage("Hid timer " + QString::number(l_timer_id) + ".");
            // Hide the timer
            l_is_global ? server->broadcast(l_hide_timer) : server->broadcast(l_hide_timer, m_current_area);
        }
    }
}

void AOClient::cmdNoteCard(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    AreaData *l_area = server->getAreaById(m_current_area);
    QString l_notecard = argv.join(" ");
    l_area->addNotecard(m_current_char, l_notecard);
    sendServerMessageArea(m_current_char + " wrote a note card.");
}

void AOClient::cmdNoteCardClear(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    if (!l_area->addNotecard(m_current_char, QString())) {
        sendServerMessageArea(m_current_char + " erased their note card.");
    }
}

void AOClient::cmdNoteCardReveal(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    AreaData *l_area = server->getAreaById(m_current_area);
    const QStringList l_notecards = l_area->getNotecards();

    if (l_notecards.isEmpty()) {
        sendServerMessage("There are no cards to reveal in this area.");
        return;
    }

    QString l_message("Note cards have been revealed.\n");
    l_message.append(l_notecards.join("\n") + "\n");

    sendServerMessageArea(l_message);
}

void AOClient::cmd8Ball(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    if (ConfigManager::magic8BallAnswers().isEmpty()) {
        qWarning() << "8ball.txt is empty!";
        sendServerMessage("8ball.txt is empty.");
    }
    else {
        QString l_response = ConfigManager::magic8BallAnswers().at((genRand(1, ConfigManager::magic8BallAnswers().size() - 1)));
        QString l_sender_name = m_ooc_name;
        QString l_sender_message = argv.join(" ");

        sendServerMessageArea(l_sender_name + " asked the magic 8-ball, \"" + l_sender_message + "\" and the answer is: " + l_response);
    }
}

void AOClient::cmdSubTheme(int argc, QStringList argv)
{
    Q_UNUSED(argc);

    QString l_subtheme = argv.join(" ");
    const QVector<AOClient *> l_clients = server->getClients();
    for (AOClient *l_client : l_clients) {
        if (l_client->m_current_area == m_current_area)
            l_client->sendPacket("ST", {l_subtheme, "1"});
    }
    sendServerMessageArea("Subtheme was set to " + l_subtheme);
}
