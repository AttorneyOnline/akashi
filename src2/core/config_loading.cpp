#include "core/config_loading.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace akashi {
namespace config {

QStringList loadTextFile(const QString &f_path)
{
    QStringList l_list;
    QFile l_file(f_path);
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    while (!l_file.atEnd()) {
        l_list.append(l_file.readLine().trimmed());
    }
    l_file.close();
    return l_list;
}

MusicCatalog loadMusicList(const QString &f_path)
{
    MusicCatalog l_catalog;
    QFile l_file(f_path);
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);

    QJsonParseError l_error;
    QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError) {
        qWarning() << "Unable to load musiclist. The following error was encountered:" << l_error.errorString();
        return l_catalog;
    }

    QJsonArray l_root = l_doc.array();
    for (int i = 0; i < l_root.size(); i++) {
        QJsonObject l_child = l_root.at(i).toObject();

        QString l_category = l_child["category"].toString();
        if (!l_category.isEmpty()) {
            l_catalog.songs.insert(l_category, {l_category, 0});
            l_catalog.ordered.append(l_category);
        }
        else {
            qWarning() << "Category name not set. This may cause the musiclist to be displayed incorrectly.";
        }

        QJsonArray l_songs = l_child["songs"].toArray();
        for (int j = 0; j < l_songs.size(); j++) {
            QJsonObject l_song = l_songs.at(j).toObject();
            QString l_name = l_song["name"].toString();
            QString l_real = l_song["realname"].toString();
            if (l_real.isEmpty()) {
                l_real = l_name;
            }
            int l_duration = l_song["length"].toVariant().toInt();
            l_catalog.songs.insert(l_name, {l_real, l_duration});
            l_catalog.ordered.append(l_name);
        }
    }
    l_file.close();
    return l_catalog;
}

QStringList loadIpRangeBans(const QString &f_path)
{
    QFile l_file(f_path);
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);

    QJsonParseError l_error;
    QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll(), &l_error);
    if (l_error.error != QJsonParseError::NoError) {
        qDebug() << "Unable to parse JSON file. Error:" << l_error.errorString();
        return {};
    }

    QJsonObject l_root = l_doc.object();
    QStringList l_bans;
    l_bans.append(l_root["ip_range"].toVariant().toStringList());

    if (QFile::exists("storage/asn.sqlite3")) {
        QSqlDatabase asn_db = QSqlDatabase::contains("ASN") ? QSqlDatabase::database("ASN") : QSqlDatabase::addDatabase("QSQLITE", "ASN");
        asn_db.setDatabaseName("storage/asn.sqlite3");
        asn_db.open();

        const QStringList l_asns = l_root["asn"].toVariant().toStringList();
        QStringList l_placeholders;
        for (int i = 0; i < l_asns.size(); i++) {
            l_placeholders.append("?");
        }
        QSqlQuery query(asn_db);
        query.prepare("SELECT ip FROM maxmind WHERE asn in (" + l_placeholders.join(",") + ")");
        for (const QString &l_asn : l_asns) {
            query.addBindValue(l_asn);
        }
        query.exec();
        while (query.next()) {
            l_bans.append(query.value(0).toString());
        }
        asn_db.close();
    }
    l_bans.removeDuplicates();
    return l_bans;
}

QList<quint32> loadBannedAsns(const QString &f_path)
{
    QFile l_file(f_path);
    l_file.open(QIODevice::ReadOnly | QIODevice::Text);
    const QJsonObject l_root = QJsonDocument::fromJson(l_file.readAll()).object();

    QList<quint32> l_asns;
    const QStringList l_texts = l_root["asn"].toVariant().toStringList();
    for (const QString &l_text : l_texts) {
        l_asns.append(l_text.toUInt());
    }
    return l_asns;
}

} // namespace config
} // namespace akashi
