#include "commands/casing_commands.h"

#include "akashi/filesystem_service.h"
#include "akashi/permissions.h"
#include "core/client_session.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/server_context.h"
#include "core/server_settings.h"
#include "proto/packet.h"
#include "world/area.h"

namespace akashi::commands {

static void handleDoc(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    if (f_context.argc() == 0) {
        f_context.reply("Document: " + l_area->document());
    }
    else {
        l_area->changeDoc(f_context.arguments().join(" "));
        f_context.replyToArea(f_context.name() + " changed the document.");
    }
}

static void handleClearDoc(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    l_area->changeDoc("No document.");
    f_context.replyToArea(f_context.name() + " cleared the document.");
}

static void handleEvidenceMod(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    QString l_mod = f_context.argument(0).toLower();
    if (l_mod == "cm")
        l_area->setEvidenceAccess(akashi::EvidenceStore::Access::Cm);
    else if (l_mod == "mod")
        l_area->setEvidenceAccess(akashi::EvidenceStore::Access::Mod);
    else if (l_mod == "hidden_cm" || l_mod == "hiddencm")
        l_area->setEvidenceAccess(akashi::EvidenceStore::Access::HiddenCm);
    else if (l_mod == "ffa")
        l_area->setEvidenceAccess(akashi::EvidenceStore::Access::FreeForAll);
    else {
        f_context.reply("Invalid evidence mod.");
        return;
    }
    f_context.reply("Changed evidence mod.");

    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    l_self->sendEvidenceList(l_area);
}

static void handleEvidenceSwap(CommandContext &f_context)
{
    akashi::Area *l_area = f_context.server()->areaById(f_context.areaId());
    int l_ev_size = l_area->evidence().size() - 1;

    if (l_ev_size < 0) {
        f_context.reply("No evidence in area.");
        return;
    }

    bool ok, ok2;
    int l_ev_id1 = f_context.argument(0).toInt(&ok);
    int l_ev_id2 = f_context.argument(1).toInt(&ok2);

    if (!ok || !ok2) {
        f_context.reply("Invalid evidence ID.");
        return;
    }
    if (l_ev_id1 < 0 || l_ev_id2 < 0) {
        f_context.reply("Evidence ID can't be negative.");
        return;
    }
    if (l_ev_id2 <= l_ev_size && l_ev_id1 <= l_ev_size) {
        l_area->swapEvidence(l_ev_id1, l_ev_id2);
        akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
        l_self->sendEvidenceList(l_area);
        f_context.reply("The evidence " + QString::number(l_ev_id1) + " and " + QString::number(l_ev_id2) + " have been swapped.");
    }
    else {
        f_context.reply("Unable to swap evidence. Evidence ID out of range.");
    }
}

static void handleTestify(CommandContext &f_context)
{
    akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
    if (l_recorder->state() == akashi::TestimonyRecorder::State::Recording) {
        f_context.reply("Testimony recording is already in progress. Please stop it with /pause before starting a new one.");
    }
    else {
        l_recorder->clear();
        l_recorder->setState(akashi::TestimonyRecorder::State::Recording);
        f_context.reply("Started testimony recording. The next IC message will be a title. Use /pause to stop recording.");
    }
}

static void handleExamine(CommandContext &f_context)
{
    akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
    if (l_recorder->statementCount() - 1 > 0) {
        if (l_recorder->state() == akashi::TestimonyRecorder::State::Playback) {
            f_context.reply("An examination is already running. Use /testimony to view the testimony.");
        }
        else {
            l_recorder->restart();
            f_context.server()->broadcast(akashi::Packet("RT", {"testimony2", "0"}), f_context.areaId());
            // The title replays through the server's IC broadcast.
            f_context.server()->broadcastIc(l_recorder->statementAt(0)->playbackFields(), f_context.areaId());
            return;
        }
    }
    else {
        f_context.reply("Unable to start replay without prior testimony. Use /testify to start or load a testimony with the command: /loadtestimony.");
    }
}

static void handleTestimony(CommandContext &f_context)
{
    akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
    if (l_recorder->statementCount() - 1 < 1) {
        f_context.reply("Unable to display empty testimony.");
        return;
    }

    QString l_ooc_message;
    for (int i = 1; i < l_recorder->statementCount(); i++) {
        l_ooc_message.append("[" + QString::number(i) + "]" + l_recorder->statementAt(i)->message() + "\n");
    }
    f_context.reply(l_ooc_message);
}

static void handleDeleteStatement(CommandContext &f_context)
{
    akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
    const int l_c_statement = l_recorder->statementIndex();
    if (l_recorder->statementCount() - 1 == 0) {
        f_context.reply("Unable to delete statement. No statements saved in this area.");
    }
    if (l_c_statement > 0 && l_recorder->statementCount() > 2) {
        l_recorder->remove(l_c_statement);
        f_context.reply("The statement with id " + QString::number(l_c_statement) + " has been deleted from the testimony.");
    }
}

static void handleUpdateStatement(CommandContext &f_context)
{
    f_context.server()->areaById(f_context.areaId())->testimonyRecorder()->setState(akashi::TestimonyRecorder::State::Update);
    f_context.reply("The next IC-Message will replace the currently selected testimony line.");
}

static void handlePauseTestimony(CommandContext &f_context)
{
    f_context.server()->areaById(f_context.areaId())->testimonyRecorder()->setState(akashi::TestimonyRecorder::State::Stopped);
    f_context.server()->broadcast(akashi::Packet("RT", {"testimony1", "1"}), f_context.areaId());
    f_context.reply("Testimony has been stopped. Use /examine to begin cross-examination.");
}

static void handleAddStatement(CommandContext &f_context)
{
    akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
    if (l_recorder->statementIndex() < f_context.server()->serverSettings()->maximum_statements()) {
        l_recorder->setState(akashi::TestimonyRecorder::State::Add);
        f_context.reply("The next IC-Message will be inserted into the testimony.");
    }
    else {
        f_context.reply("Unable to add anymore statements. Please remove any unused ones.");
    }
}

static void handleSaveTestimony(CommandContext &f_context)
{
    bool l_permission_found = false;
    if (f_context.canPerform(akashi::permission::save_testimony))
        l_permission_found = true;

    akashi::ClientSession *l_self = f_context.server()->clientById(f_context.clientId());
    if (l_self->isTestimonySaving())
        l_permission_found = true;

    if (l_permission_found) {
        akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
        if (l_recorder->statementCount() - 1 <= 0) {
            f_context.reply("Can't save an empty testimony.");
            return;
        }

        const std::optional<QString> l_testimony_name = akashi::FileSystemService::sanitizedFileName(f_context.argument(0));
        const std::optional<QString> l_path =
            l_testimony_name ? f_context.server()->fileSystem()->resolve(akashi::FileSystemService::Scope::Storage, "testimony/" + *l_testimony_name + ".txt") : std::nullopt;
        if (!l_path) {
            f_context.reply("Invalid testimony name. Use only letters, numbers, dashes and underscores.");
            return;
        }
        if (QFile::exists(*l_path)) {
            f_context.reply("Unable to save testimony. Testimony name already exists.");
            return;
        }

        QByteArray l_data;
        for (int i = 0; i < l_recorder->statementCount(); i++) {
            l_data += l_recorder->statementAt(i)->toSavedLine().toUtf8() + "\n";
        }
        if (auto l_error = f_context.server()->fileSystem()->writeFile(*l_path, l_data)) {
            f_context.reply("Unable to save testimony: " + *l_error);
            return;
        }
        f_context.reply("Testimony saved. To load it use /loadtestimony " + *l_testimony_name);
        l_self->setTestimonySaving(false);
    }
    else {
        f_context.reply("You don't have permission to save a testimony. Please contact a moderator for permission.");
        return;
    }
}

static void handleLoadTestimony(CommandContext &f_context)
{
    QDir l_dir_testimony("storage/testimony");
    if (!l_dir_testimony.exists()) {
        f_context.reply("Unable to load testimonies. Testimony storage not found.");
        return;
    }

    const std::optional<QString> l_testimony_name = akashi::FileSystemService::sanitizedFileName(f_context.argument(0));
    const std::optional<QString> l_path =
        l_testimony_name ? f_context.server()->fileSystem()->resolve(akashi::FileSystemService::Scope::Storage, "testimony/" + *l_testimony_name + ".txt") : std::nullopt;
    if (!l_path) {
        f_context.reply("Invalid testimony name. Use only letters, numbers, dashes and underscores.");
        return;
    }
    QFile l_file(*l_path);
    if (!l_file.exists()) {
        f_context.reply("Unable to load testimony. Testimony name not found.");
        return;
    }
    if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        f_context.reply("Unable to load testimony. Permission denied.");
        return;
    }

    akashi::TestimonyRecorder *l_recorder = f_context.server()->areaById(f_context.areaId())->testimonyRecorder();
    l_recorder->clear();
    int l_line_number = 0;
    QTextStream l_in(&l_file);
    while (!l_in.atEnd()) {
        const QString l_line = l_in.readLine();
        ++l_line_number;
        if (l_line.isEmpty()) {
            continue;
        }
        if (l_recorder->statementCount() > f_context.server()->serverSettings()->maximum_statements()) {
            f_context.reply("Testimony too large to be loaded.");
            l_recorder->clear();
            return;
        }
        const std::optional<akashi::Statement> l_statement = akashi::Statement::fromSavedLine(l_line);
        if (!l_statement) {
            f_context.reply("Unable to load testimony. Line " + QString::number(l_line_number) + " is not a statement.");
            l_recorder->clear();
            return;
        }
        l_recorder->insert(l_recorder->statementCount(), *l_statement);
    }
    f_context.reply("Testimony loaded successfully. Use /examine to start playback.");
}

void registerCasingCommands(CommandRegistry &f_registry)
{
    f_registry.registerCommand(
        {QStringLiteral("doc"), {}, {}, 0,
         QStringLiteral("/doc [text]"),
         QStringLiteral("Views or sets the document for the area.")},
        handleDoc, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("cleardoc"), {}, {}, 0,
         QStringLiteral("/cleardoc"),
         QStringLiteral("Clears the document in the area.")},
        handleClearDoc, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("evidence_mod"), {}, {akashi::permission::modify_evidence}, 1,
         QStringLiteral("/evidence_mod <mod>"),
         QStringLiteral("Changes the evidence mod in the area.")},
        handleEvidenceMod, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("evidence_swap"), {}, {akashi::permission::gamemaster}, 2,
         QStringLiteral("/evidence_swap <id1> <id2>"),
         QStringLiteral("Swaps two pieces of evidence in the area.")},
        handleEvidenceSwap, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("testify"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/testify"),
         QStringLiteral("Starts testimony recording.")},
        handleTestify, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("examine"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/examine"),
         QStringLiteral("Starts testimony playback.")},
        handleExamine, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("testimony"), {}, {}, 0,
         QStringLiteral("/testimony"),
         QStringLiteral("Lists the statements in the current testimony.")},
        handleTestimony, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("delete"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/delete"),
         QStringLiteral("Deletes the currently selected testimony statement.")},
        handleDeleteStatement, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("update"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/update"),
         QStringLiteral("Replaces the current testimony statement with the next IC message.")},
        handleUpdateStatement, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("pause"), {QStringLiteral("end")}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/pause"),
         QStringLiteral("Pauses testimony recording or playback.")},
        handlePauseTestimony, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("add"), {}, {akashi::permission::gamemaster}, 0,
         QStringLiteral("/add"),
         QStringLiteral("Inserts a new statement after the current one.")},
        handleAddStatement, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("savetestimony"), {}, {}, 1,
         QStringLiteral("/savetestimony <name>"),
         QStringLiteral("Saves the current testimony to a file.")},
        handleSaveTestimony, QStringLiteral("core"));

    f_registry.registerCommand(
        {QStringLiteral("loadtestimony"), {}, {akashi::permission::gamemaster}, 1,
         QStringLiteral("/loadtestimony <name>"),
         QStringLiteral("Loads a saved testimony for playback.")},
        handleLoadTestimony, QStringLiteral("core"));
}

} // namespace akashi::commands
