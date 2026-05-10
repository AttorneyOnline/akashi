#include "commands/roleplay_commands.h"

#include "akashi/permissions.h"
#include "aoclient.h"
#include "area_data.h"
#include "config_manager.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "proto/packet.h"
#include "server.h"

#include <QTime>

namespace akashi::commands {

static void diceThrower(CommandContext &f_context, int f_sides, int f_dice, bool f_private, int f_modifier = 0)
{
    if (f_sides < 0 || f_dice < 0 || f_sides > ConfigManager::diceMaxValue() || f_dice > ConfigManager::diceMaxDice()) {
        f_context.reply("Dice or side number out of bounds.");
        return;
    }
    QStringList l_results;
    for (int i = 1; i <= f_dice; i++) {
        l_results.append(QString::number(CommandContext::genRand(1, f_sides) + f_modifier));
    }
    QString l_total = l_results.join(" ");
    if (f_private) {
        if (f_modifier)
            f_context.reply("You rolled a " + QString::number(f_dice) + "d" + QString::number(f_sides) + "+" + QString::number(f_modifier) + ". Results: " + l_total);
        else
            f_context.reply("You rolled a " + QString::number(f_dice) + "d" + QString::number(f_sides) + ". Results: " + l_total);
        return;
    }
    if (f_modifier)
        f_context.replyToArea(f_context.name() + " rolled a " + QString::number(f_dice) + "d" + QString::number(f_sides) + "+" + QString::number(f_modifier) + ". Results: " + l_total);
    else
        f_context.replyToArea(f_context.name() + " rolled a " + QString::number(f_dice) + "d" + QString::number(f_sides) + ". Results: " + l_total);
}

static QString areaTimer(CommandContext &f_context, int f_area_idx, int f_timer_idx)
{
    AreaData *l_area = f_context.server()->areaById(f_area_idx);
    QTimer *l_timer;
    QString l_name = (f_timer_idx == 0) ? "Global timer" : "Timer " + QString::number(f_timer_idx);
    if (f_timer_idx == 0)
        l_timer = f_context.server()->timer;
    else if (f_timer_idx > 0 && f_timer_idx <= 4)
        l_timer = l_area->timers().at(f_timer_idx - 1);
    else
        return "Invalid timer ID.";
    if (l_timer->isActive()) {
        QTime l_current = QTime(0, 0).addMSecs(l_timer->remainingTime());
        return l_name + " is at " + l_current.toString("hh:mm:ss.zzz");
    }
    return l_name + " is inactive.";
}

static void handleFlip(CommandContext &f_context)
{
    QString l_sender_name = f_context.name();
    QStringList l_faces = {"heads", "tails"};
    QString l_face = l_faces[CommandContext::genRand(0, 1)];
    f_context.replyToArea(l_sender_name + " flipped a coin and got " + l_face + ".");
}

static void handleRoll(CommandContext &f_context)
{
    int l_sides = 6;
    int l_dice = 1;

    if (f_context.argc() >= 1) {
        QString l_arg = f_context.argument(0);
        if (l_arg.contains('d')) {
            QStringList l_arguments = l_arg.split('d');

            bool l_dice_ok;
            bool l_sides_ok;
            l_dice = l_arguments[0].toInt(&l_dice_ok);
            l_sides = l_arguments[1].toInt(&l_sides_ok);

            if (l_arg.contains('+')) {
                bool l_mod_ok;
                QStringList l_modifier = l_arguments[1].split('+');
                if (l_modifier.size() < 2) {
                    f_context.reply("Invalid dice notation.");
                    return;
                }
                int modifier = l_modifier[1].toInt(&l_mod_ok);
                l_sides = l_modifier[0].toInt(&l_sides_ok);

                if (l_mod_ok && l_dice_ok && l_sides_ok)
                    diceThrower(f_context, l_sides, l_dice, false, modifier);
                else
                    f_context.reply("Invalid dice notation.");
                return;
            }
            else if (l_arg.contains('-')) {
                bool l_mod_ok;
                QStringList l_modifier = l_arguments[1].split('-');
                if (l_modifier.size() < 2) {
                    f_context.reply("Invalid dice notation.");
                    return;
                }
                int modifier = l_modifier[1].toInt(&l_mod_ok);
                l_sides = l_modifier[0].toInt(&l_sides_ok);

                if (l_mod_ok && l_dice_ok && l_sides_ok)
                    diceThrower(f_context, l_sides, l_dice, false, -modifier);
                else
                    f_context.reply("Invalid dice notation.");
                return;
            }
            else if (l_dice_ok && l_sides_ok) {
                diceThrower(f_context, l_sides, l_dice, false);
                return;
            }
            else {
                f_context.reply("Invalid dice notation.");
                return;
            }
        }
        else
            l_sides = qBound(1, l_arg.toInt(), ConfigManager::diceMaxValue());
    }
    if (f_context.argc() == 2)
        l_dice = qBound(1, f_context.argument(1).toInt(), ConfigManager::diceMaxDice());
    diceThrower(f_context, l_sides, l_dice, false);
}

static void handleRollA(CommandContext &f_context)
{
    QString l_dice_name = f_context.arguments().join(" ");

    if (ConfigManager::diceFaces(l_dice_name).isEmpty()) {
        qWarning() << "Unknown dice.";
        f_context.reply("Unknown dice.");
    }
    else {
        QString l_response = ConfigManager::diceFaces(l_dice_name).at(CommandContext::genRand(0, ConfigManager::diceFaces(l_dice_name).size() - 1));
        QString l_sender_name = f_context.name();
        f_context.replyToArea(l_sender_name + " rolled from the \"" + l_dice_name + "\" set and got: " + l_response);
    }
}

static void handleRollP(CommandContext &f_context)
{
    int l_sides = 6;
    int l_dice = 1;

    if (f_context.argc() >= 1) {
        QString l_arg = f_context.argument(0);
        if (l_arg.contains('d')) {
            QStringList l_arguments = l_arg.split('d');

            bool l_dice_ok;
            bool l_sides_ok;
            l_dice = l_arguments[0].toInt(&l_dice_ok);
            l_sides = l_arguments[1].toInt(&l_sides_ok);

            if (l_arg.contains('+')) {
                bool l_mod_ok;
                QStringList l_modifier = l_arguments[1].split('+');
                if (l_modifier.size() < 2) {
                    f_context.reply("Invalid dice notation.");
                    return;
                }
                int modifier = l_modifier[1].toInt(&l_mod_ok);
                l_sides = l_modifier[0].toInt(&l_sides_ok);

                if (l_mod_ok && l_dice_ok && l_sides_ok)
                    diceThrower(f_context, l_sides, l_dice, true, modifier);
                else
                    f_context.reply("Invalid dice notation.");
                return;
            }
            else if (l_arg.contains('-')) {
                bool l_mod_ok;
                QStringList l_modifier = l_arguments[1].split('-');
                if (l_modifier.size() < 2) {
                    f_context.reply("Invalid dice notation.");
                    return;
                }
                int modifier = l_modifier[1].toInt(&l_mod_ok);
                l_sides = l_modifier[0].toInt(&l_sides_ok);

                if (l_mod_ok && l_dice_ok && l_sides_ok)
                    diceThrower(f_context, l_sides, l_dice, true, -modifier);
                else
                    f_context.reply("Invalid dice notation.");
                return;
            }
            else if (l_dice_ok && l_sides_ok) {
                diceThrower(f_context, l_sides, l_dice, true);
                return;
            }
            else {
                f_context.reply("Invalid dice notation.");
                return;
            }
        }
        else
            l_sides = qBound(1, l_arg.toInt(), ConfigManager::diceMaxValue());
    }
    if (f_context.argc() == 2)
        l_dice = qBound(1, f_context.argument(1).toInt(), ConfigManager::diceMaxDice());
    diceThrower(f_context, l_sides, l_dice, true);
}

static void handleTimer(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());

    if (f_context.argc() == 0) {
        QStringList l_timers;
        l_timers.append("Currently active timers:");
        for (int i = 0; i <= 4; i++) {
            l_timers.append(areaTimer(f_context, l_area->index(), i));
        }
        f_context.reply(l_timers.join("\n"));
        return;
    }

    bool ok;
    int l_timer_id = f_context.argument(0).toInt(&ok);
    if (!ok || l_timer_id < 0 || l_timer_id > 4) {
        f_context.reply("Invalid timer ID. Timer ID must be a whole number between 0 and 4.");
        return;
    }

    if (f_context.argc() == 1) {
        f_context.reply(areaTimer(f_context, l_area->index(), l_timer_id));
        return;
    }

    QTimer *l_requested_timer;
    if (l_timer_id == 0) {
        if (!f_context.canPerform(akashi::permission::global_timer)) {
            f_context.reply("You are not authorized to alter the global timer.");
            return;
        }
        l_requested_timer = f_context.server()->timer;
    }
    else
        l_requested_timer = l_area->timers().at(l_timer_id - 1);

    akashi::Packet l_show_timer("TI", {QString::number(l_timer_id), "2"});
    akashi::Packet l_hide_timer("TI", {QString::number(l_timer_id), "3"});
    bool l_is_global = l_timer_id == 0;

    QTime l_requested_time = QTime::fromString(f_context.argument(1), "hh:mm:ss");
    if (l_requested_time.isValid()) {
        l_requested_timer->setInterval(QTime(0, 0).msecsTo(l_requested_time));
        l_requested_timer->start();
        f_context.reply("Set timer " + QString::number(l_timer_id) + " to " + f_context.argument(1) + ".");
        akashi::Packet l_update_timer("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(l_requested_time))});
        l_is_global ? f_context.server()->broadcast(l_show_timer) : f_context.server()->broadcast(l_show_timer, f_context.areaId());
        l_is_global ? f_context.server()->broadcast(l_update_timer) : f_context.server()->broadcast(l_update_timer, f_context.areaId());
        return;
    }
    else {
        QString l_action = f_context.argument(1);
        if (l_action == "start") {
            l_requested_timer->start();
            f_context.reply("Started timer " + QString::number(l_timer_id) + ".");
            akashi::Packet l_update_timer("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_requested_timer->remainingTime())))});
            l_is_global ? f_context.server()->broadcast(l_show_timer) : f_context.server()->broadcast(l_show_timer, f_context.areaId());
            l_is_global ? f_context.server()->broadcast(l_update_timer) : f_context.server()->broadcast(l_update_timer, f_context.areaId());
        }
        else if (l_action == "pause" || l_action == "stop") {
            l_requested_timer->setInterval(l_requested_timer->remainingTime());
            l_requested_timer->stop();
            f_context.reply("Stopped timer " + QString::number(l_timer_id) + ".");
            akashi::Packet l_update_timer("TI", {QString::number(l_timer_id), "1", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_requested_timer->interval())))});
            l_is_global ? f_context.server()->broadcast(l_update_timer) : f_context.server()->broadcast(l_update_timer, f_context.areaId());
        }
        else if (l_action == "hide" || l_action == "unset") {
            l_requested_timer->setInterval(0);
            l_requested_timer->stop();
            f_context.reply("Hid timer " + QString::number(l_timer_id) + ".");
            l_is_global ? f_context.server()->broadcast(l_hide_timer) : f_context.server()->broadcast(l_hide_timer, f_context.areaId());
        }
    }
}

static void handleNotecard(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    QString l_notecard = f_context.arguments().join(" ");
    l_area->addNotecard(f_context.character(), l_notecard);
    f_context.replyToArea(f_context.character() + " wrote a note card.");
}

static void handleNotecardClear(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    if (!l_area->addNotecard(f_context.character(), QString())) {
        f_context.replyToArea(f_context.character() + " erased their note card.");
    }
}

static void handleNotecardReveal(CommandContext &f_context)
{
    AreaData *l_area = f_context.server()->areaById(f_context.areaId());
    const QStringList l_notecards = l_area->notecards();

    if (l_notecards.isEmpty()) {
        f_context.reply("There are no cards to reveal in this area.");
        return;
    }

    QString l_message("Note cards have been revealed.\n");
    l_message.append(l_notecards.join(""));

    f_context.replyToArea(l_message);
}

static void handle8Ball(CommandContext &f_context)
{
    if (ConfigManager::magic8BallAnswers().isEmpty()) {
        qWarning() << "8ball.txt is empty!";
        f_context.reply("8ball.txt is empty.");
    }
    else {
        QString l_response = ConfigManager::magic8BallAnswers().at(CommandContext::genRand(1, ConfigManager::magic8BallAnswers().size() - 1));
        QString l_sender_name = f_context.name();
        QString l_sender_message = f_context.arguments().join(" ");
        f_context.replyToArea(l_sender_name + " asked the magic 8-ball, \"" + l_sender_message + "\" and the answer is: " + l_response);
    }
}

static void handleSubtheme(CommandContext &f_context)
{
    QString l_subtheme = f_context.arguments().join(" ");
    const QVector<AOClient *> l_clients = f_context.server()->clients();
    for (AOClient *l_client : l_clients) {
        if (l_client->areaId() == f_context.areaId())
            l_client->sendPacket("ST", {l_subtheme, "1"});
    }
    f_context.replyToArea("Subtheme was set to " + l_subtheme);
}

void registerRoleplayCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand({"coinflip", {}, {}, 0}, handleFlip, "core");
    f_registry.registerCommand({"roll", {}, {}, 0}, handleRoll, "core");
    f_registry.registerCommand({"rolla", {}, {}, 0}, handleRollA, "core");
    f_registry.registerCommand({"rollp", {}, {}, 0}, handleRollP, "core");
    f_registry.registerCommand({"timer", {}, {permission::gamemaster}, 0}, handleTimer, "core");
    f_registry.registerCommand({"notecard", {}, {}, 1}, handleNotecard, "core");
    f_registry.registerCommand({"notecard_clear", {}, {}, 0}, handleNotecardClear, "core");
    f_registry.registerCommand({"notecard_reveal", {}, {permission::gamemaster}, 0}, handleNotecardReveal, "core");
    f_registry.registerCommand({"8ball", {}, {}, 1}, handle8Ball, "core");
    f_registry.registerCommand({"subtheme", {}, {permission::gamemaster}, 1}, handleSubtheme, "core");
}

} // namespace akashi::commands
