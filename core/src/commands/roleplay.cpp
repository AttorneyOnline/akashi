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

// This file is for commands under the roleplay category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdFlip(int argc, QStringList argv)
{
    QString sender_name = ooc_name;
    QStringList faces = {"heads","tails"};
    QString face = faces[AOClient::genRand(0,1)];
    sendServerMessage(sender_name + " flipped a coin and got " + face + ".");
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
    AreaData* area = server->areas[current_area];

    // Called without arguments
    // Shows a brief of all timers
    if (argc == 0) {
        QStringList timers;
        timers.append("Currently active timers:");
        for (int i = 0; i <= 4; i++) {
            timers.append(getAreaTimer(area->index(), i));
        }
        sendServerMessage(timers.join("\n"));
        return;
    }

    // Called with more than one argument
    bool ok;
    int timer_id = argv[0].toInt(&ok);
    if (!ok || timer_id < 0 || timer_id > 4) {
        sendServerMessage("Invalid timer ID. Timer ID must be a whole number between 0 and 4.");
        return;
    }

    // Called with one argument
    // Shows the status of one timer
    if (argc == 1) {
        sendServerMessage(getAreaTimer(area->index(), timer_id));
        return;
    }

    // Called with more than one argument
    // Updates the state of a timer

    // Select the proper timer
    // Check against permissions if global timer is selected
    QTimer* requested_timer;
    if (timer_id == 0) {
        if (!checkAuth(ACLFlags.value("GLOBAL_TIMER"))) {
            sendServerMessage("You are not authorized to alter the global timer.");
            return;
        }
        requested_timer = server->timer;
    }
    else
        requested_timer = area->timers().at(timer_id - 1);

    AOPacket show_timer("TI", {QString::number(timer_id), "2"});
    AOPacket hide_timer("TI", {QString::number(timer_id), "3"});
    bool is_global = timer_id == 0;

    // Set the timer's time remaining if the second
    // argument is a valid time
    QTime requested_time = QTime::fromString(argv[1], "hh:mm:ss");
    if (requested_time.isValid()) {
        requested_timer->setInterval(QTime(0,0).msecsTo(requested_time));
        requested_timer->start();
        sendServerMessage("Set timer " + QString::number(timer_id) + " to " + argv[1] + ".");
        AOPacket update_timer("TI", {QString::number(timer_id), "0", QString::number(QTime(0,0).msecsTo(requested_time))});
        is_global ? server->broadcast(show_timer) : server->broadcast(show_timer, current_area); // Show the timer
        is_global ? server->broadcast(update_timer) : server->broadcast(update_timer, current_area);
        return;
    }
    // Otherwise, update the state of the timer
    else {
        if (argv[1] == "start") {
            requested_timer->start();
            sendServerMessage("Started timer " + QString::number(timer_id) + ".");
            AOPacket update_timer("TI", {QString::number(timer_id), "0", QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(requested_timer->remainingTime())))});
            is_global ? server->broadcast(show_timer) : server->broadcast(show_timer, current_area);
            is_global ? server->broadcast(update_timer) : server->broadcast(update_timer, current_area);
        }
        else if (argv[1] == "pause" || argv[1] == "stop") {
            requested_timer->setInterval(requested_timer->remainingTime());
            requested_timer->stop();
            sendServerMessage("Stopped timer " + QString::number(timer_id) + ".");
            AOPacket update_timer("TI", {QString::number(timer_id), "1", QString::number(QTime(0,0).msecsTo(QTime(0,0).addMSecs(requested_timer->interval())))});
            is_global ? server->broadcast(update_timer) : server->broadcast(update_timer, current_area);
        }
        else if (argv[1] == "hide" || argv[1] == "unset") {
            requested_timer->setInterval(0);
            requested_timer->stop();
            sendServerMessage("Hid timer " + QString::number(timer_id) + ".");
            // Hide the timer
            is_global ? server->broadcast(hide_timer) : server->broadcast(hide_timer, current_area);
        }
    }
}

void AOClient::cmdNoteCard(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    QString notecard = argv.join(" ");
    area->addNotecard(current_char, notecard);
    sendServerMessageArea(current_char + " wrote a note card.");
}

void AOClient::cmdNoteCardClear(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    if (!area->addNotecard(current_char, QString())) {
        sendServerMessageArea(current_char + " erased their note card.");
    }
}

void AOClient::cmdNoteCardReveal(int argc, QStringList argv)
{
    AreaData* area = server->areas[current_area];
    const QStringList l_notecards = area->getNotecards();

    if (l_notecards.isEmpty()) {
        sendServerMessage("There are no cards to reveal in this area.");
        return;
    }

    QString message("Note cards have been revealed.\n");
    message.append(l_notecards.join("\n") + "\n");

    sendServerMessageArea(message);
}

void AOClient::cmd8Ball(int argc, QStringList argv)
{
    if (server->magic_8ball_answers.isEmpty()) {
        qWarning() << "8ball.txt is empty!";
        sendServerMessage("8ball.txt is empty.");
        }
    else {
        QString response = server->magic_8ball_answers[(genRand(1, server->magic_8ball_answers.size() - 1))];
        QString sender_name = ooc_name;
        QString sender_message = argv.join(" ");

        sendServerMessageArea(sender_name + " asked the magic 8-ball, \"" + sender_message + "\" and the answer is: " + response);
        }
}

void AOClient::cmdSubTheme(int argc, QStringList argv)
{
    QString subtheme = argv.join(" ");
    for (AOClient* client : server->clients) {
        if (client->current_area == current_area)
            client->sendPacket("ST", {subtheme, "1"});
    }
    sendServerMessageArea("Subtheme was set to " + subtheme);
}
