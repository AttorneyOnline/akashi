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

// This file is for functions used by various commands, defined in the command helper function category in aoclient.h
// Be sure to register the command in the header before adding it here!

void AOClient::cmdDefault(int argc, QStringList argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    sendServerMessage("Invalid command.");
    return;
}

QStringList AOClient::buildAreaList(int area_idx)
{
    QStringList entries;
    QString area_name = server->area_names[area_idx];
    AreaData* area = server->areas[area_idx];
    entries.append("=== " + area_name + " ===");
    switch (area->lockStatus()) {
        case AreaData::LockStatus::LOCKED:
            entries.append("[LOCKED]");
            break;
        case AreaData::LockStatus::SPECTATABLE:
            entries.append("[SPECTATABLE]");
            break;
        case AreaData::LockStatus::FREE:
        default:
            break;
    }
    entries.append("[" + QString::number(area->playerCount()) + " users][" + QVariant::fromValue(area->status()).toString().replace("_", "-") + "]");
    for (AOClient* client : qAsConst(server->clients)) {
        if (client->current_area == area_idx && client->joined) {
            QString char_entry = "[" + QString::number(client->id) + "] " + client->current_char;
            if (client->current_char == "")
                char_entry += "Spectator";
            if (client->showname != "")
                char_entry += " (" + client->showname + ")";
            if (area->owners().contains(client->id))
                char_entry.insert(0, "[CM] ");
            if (authenticated)
                char_entry += " (" + client->getIpid() + "): " + client->ooc_name;
            if (client->is_afk)
                char_entry += " [AFK]";
            entries.append(char_entry);
        }
    }
    return entries;
}

int AOClient::genRand(int min, int max)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    qsrand(QDateTime::currentMSecsSinceEpoch());
    quint32 random_number = (qrand() % (max - min + 1)) + min;
    return random_number;

#else
    quint32 random_number = QRandomGenerator::system()->bounded(min, max + 1);
    return random_number;
#endif
}

void AOClient::diceThrower(int argc, QStringList argv, bool p_roll)
{
    int sides = 6;
    int dice = 1;
    QStringList results;
    if (argc >= 1)
        sides = qBound(1, argv[0].toInt(), ConfigManager::diceMaxValue());
    if (argc == 2)
        dice = qBound(1, argv[1].toInt(), ConfigManager::diceMaxDice());
    for (int i = 1; i <= dice; i++) {
        results.append(QString::number(AOClient::genRand(1, sides)));
    }
    QString total_results = results.join(" ");
    if (p_roll) {
        sendServerMessage("You rolled a " + QString::number(dice) + "d" + QString::number(sides) + ". Results: " + total_results);
        return;
    }
    sendServerMessageArea(ooc_name + " rolled a " + QString::number(dice) + "d" + QString::number(sides) + ". Results: " + total_results);
}

QString AOClient::getAreaTimer(int area_idx, int timer_idx)
{
    AreaData* area = server->areas[area_idx];
    QTimer* timer;
    QString timer_name = (timer_idx == 0) ? "Global timer" : "Timer " + QString::number(timer_idx);

    if (timer_idx == 0)
        timer = server->timer;
    else if (timer_idx > 0 && timer_idx <= 4)
        timer = area->timers().at(timer_idx - 1);
    else
        return "Invalid timer ID.";

    if (timer->isActive()) {
        QTime current_time = QTime(0,0).addMSecs(timer->remainingTime());

        return timer_name + " is at " + current_time.toString("hh:mm:ss.zzz");
    }
    else {
        return timer_name + " is inactive.";
    }
}

long long AOClient::parseTime(QString input)
{
    QRegularExpression regex("(?:(?:(?<year>.*?)y)*(?:(?<week>.*?)w)*(?:(?<day>.*?)d)*(?:(?<hr>.*?)h)*(?:(?<min>.*?)m)*(?:(?<sec>.*?)s)*)");
    QRegularExpressionMatch match = regex.match(input);
    QString str_year, str_week, str_hour, str_day, str_minute, str_second;
    int year, week, day, hour, minute, second;

    str_year = match.captured("year");
    str_week = match.captured("week");
    str_day = match.captured("day");
    str_hour = match.captured("hr");
    str_minute = match.captured("min");
    str_second = match.captured("sec");

    bool is_well_formed = false;
    QString concat_str(str_year + str_week + str_day + str_hour + str_minute + str_second);
    concat_str.toInt(&is_well_formed);

    if (!is_well_formed) {
        return -1;
    }

    year = str_year.toInt();
    week = str_week.toInt();
    day = str_day.toInt();
    hour = str_hour.toInt();
    minute = str_minute.toInt();
    second = str_second.toInt();

    long long total = 0;
    total += 31622400 * year;
    total += 604800 * week;
    total += 86400 * day;
    total += 3600 * hour;
    total += 60 * minute;
    total += second;

    if (total < 0)
        return -1;

    return total;
}

QString AOClient::getReprimand(bool positive)
{
    if (positive) {
        return ConfigManager::praiseList().at(genRand(0, ConfigManager::praiseList().size() - 1));
        }
    else {
        return ConfigManager::reprimandsList().at(genRand(0, ConfigManager::reprimandsList().size() - 1));
        }
}

bool AOClient::checkPasswordRequirements(QString username, QString password)
{
    QString decoded_password = decodeMessage(password);
    if (!ConfigManager::passwordRequirements())
        return true;

    if (ConfigManager::passwordMinLength() > decoded_password.length())
        return false;

    if (ConfigManager::passwordMaxLength() < decoded_password.length() && ConfigManager::passwordMaxLength() != 0)
        return false;

    else if (ConfigManager::passwordRequireMixCase()) {
        if (decoded_password.toLower() == decoded_password)
            return false;
        if (decoded_password.toUpper() == decoded_password)
            return false;
    }
    else if (ConfigManager::passwordRequireNumbers()) {
        QRegularExpression regex("[0123456789]");
        QRegularExpressionMatch match = regex.match(decoded_password);
        if (!match.hasMatch())
            return false;
    }
    else if (ConfigManager::passwordRequireSpecialCharacters()) {
        QRegularExpression regex("[~!@#$%^&*_-+=`|\\(){}\[]:;\"'<>,.?/]");
        QRegularExpressionMatch match = regex.match(decoded_password);
        if (!match.hasMatch())
            return false;
    }
    else if (!ConfigManager::passwordCanContainUsername()) {
        if (decoded_password.contains(username))
            return false;
    }
    return true;
}

void AOClient::sendNotice(QString notice, bool global)
{
    QString message = "A moderator sent this ";
    if (global)
        message += "server-wide ";
    message += "notice:\n\n" + notice;
    sendServerMessageArea(message);
    AOPacket packet("BB", {message});
    if (global)
        server->broadcast(packet);
    else
        server->broadcast(packet, current_area);
}
