#include "music_manager.h"

#include "config_manager.h"
#include "packet/packet_factory.h"

#include <QUrl>

MusicManager::MusicManager(QStringList f_cdns, MusicList f_root_list, QStringList f_root_ordered, QObject *parent) :
    QObject(parent),
    m_root_list(f_root_list),
    m_root_ordered(f_root_ordered)
{
    m_custom_lists = new QHash<int, QMap<QString, QPair<QString, int>>>;
    if (!f_cdns.isEmpty()) {
        m_cdns = f_cdns;
    }
}

MusicManager::~MusicManager()
{
}

QStringList MusicManager::musiclist(int f_area_id)
{
    if (m_global_enabled.value(f_area_id)) {
        QStringList l_combined_list = m_root_ordered;
        l_combined_list.append(m_customs_ordered.value(f_area_id));
        return l_combined_list;
    }
    return m_custom_lists->value(f_area_id).keys();
}

QStringList MusicManager::rootMusiclist()
{
    return m_root_ordered;
}

bool MusicManager::registerArea(int f_area_id)
{
    if (m_custom_lists->contains(f_area_id)) {
        // This area is already registered. We can't add it.
        return false;
    }
    m_custom_lists->insert(f_area_id, {});
    m_global_enabled.insert(f_area_id, true);
    return true;
}

bool MusicManager::validateSong(QString f_song_name, QStringList f_approved_cdns)
{
    const QStringList l_extensions = {".opus", ".ogg", ".mp3", ".wav"};

    // For a plain (non-URL) song name the whole string is the path; for a URL we
    // use QUrl::path() so the query string/fragment don't reach the extension check.
    QString l_path = f_song_name;

    // Check if URL formatted.
    if (f_song_name.contains("/")) {
        const QUrl l_url(f_song_name);

        // Only allow HTTPS/HTTP sources.
        if (l_url.scheme() != "https" && l_url.scheme() != "http") {
            return false;
        }

        bool l_cdn_approved = false;
        for (const QString &l_cdn : qAsConst(f_approved_cdns)) {
            // Let QUrl extract the host so operators can write the entry with or
            // without a scheme/trailing slash (e.g. "https://cdn.discord.com/").
            // fromUserInput() is required here: the plain QUrl() ctor parses a
            // bare "cdn.discord.com" as a path and returns an empty host().
            const QString l_domain = QUrl::fromUserInput(l_cdn.trimmed()).host();
            if (!l_domain.isEmpty() && l_url.host().compare(l_domain, Qt::CaseInsensitive) == 0) {
                l_cdn_approved = true;
                break;
            }
        }
        if (!l_cdn_approved) {
            return false;
        }

        l_path = l_url.path();
    }

    for (const QString &l_suffix : qAsConst(l_extensions)) {
        if (l_path.endsWith(l_suffix, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

bool MusicManager::addCustomSong(QString f_song_name, QString f_real_name, int f_duration, int f_area_id)
{
    // Validate if simple name.
    QString l_song_name = f_song_name;
    if (f_song_name.split(".").size() == 1) {
        l_song_name = l_song_name + ".opus";
    }

    QString l_real_name = f_real_name;
    if (f_real_name.split(".").size() == 1) {
        l_real_name = l_real_name + ".opus";
    }

    if (!(validateSong(l_song_name, m_cdns) && validateSong(l_real_name, m_cdns))) {
        return false;
    }

    // Avoid conflicts by checking if it exists.
    if (m_root_list.contains(l_song_name) && m_global_enabled[f_area_id]) {
        return false;
    }

    if (m_custom_lists->value(f_area_id).contains(f_song_name)) {
        return false;
    }

    if (m_customs_ordered.value(f_area_id).contains(l_song_name)) {
        return false;
    }

    // There should be a way to directly insert into the QMap. Too bad!
    MusicList l_custom_list = m_custom_lists->value(f_area_id);
    l_custom_list.insert(l_song_name, {l_real_name, f_duration});
    m_custom_lists->insert(f_area_id, l_custom_list);
    m_customs_ordered.insert(f_area_id, (QStringList{m_customs_ordered.value(f_area_id)} << l_song_name));
    emit sendAreaFMPacket(PacketFactory::createPacket("FM", musiclist(f_area_id)), f_area_id);
    return true;
}

bool MusicManager::addCustomCategory(QString f_category_name, int f_area_id)
{
    if (f_category_name.split(".").size() > 1) {
        return false;
    }

    QString l_category_name = f_category_name;
    if (!f_category_name.startsWith("==")) {
        l_category_name = "==" + l_category_name + "==";
    }

    // Avoid conflicts by checking if it exists.
    if (m_root_list.contains(l_category_name) && m_global_enabled.value(f_area_id)) {
        return false;
    }

    if (m_custom_lists->value(f_area_id).contains(l_category_name)) {
        return false;
    }

    QMap<QString, QPair<QString, int>> l_custom_list = m_custom_lists->value(f_area_id);
    l_custom_list.insert(l_category_name, {l_category_name, 0});
    m_custom_lists->insert(f_area_id, l_custom_list);
    m_customs_ordered.insert(f_area_id, (QStringList{m_customs_ordered.value(f_area_id)} << l_category_name));
    emit sendAreaFMPacket(PacketFactory::createPacket("FM", musiclist(f_area_id)), f_area_id);
    return true;
}

bool MusicManager::removeCustomMusic(QString f_songcategory_name, int f_area_id)
{
    if (!m_root_list.contains(f_songcategory_name)) {
        MusicList l_custom_list = m_custom_lists->value(f_area_id);
        if (l_custom_list.contains(f_songcategory_name)) {
            l_custom_list.remove(f_songcategory_name);
            m_custom_lists->insert(f_area_id, l_custom_list);

            // Updating the list alias too.
            QStringList l_customs_ordered = m_customs_ordered.value(f_area_id);
            l_customs_ordered.removeAll(f_songcategory_name);
            m_customs_ordered.insert(f_area_id, l_customs_ordered);

            emit sendAreaFMPacket(PacketFactory::createPacket("FM", musiclist(f_area_id)), f_area_id);
            return true;
        } // Fallthrough
    }
    return false;
}

bool MusicManager::toggleCustomMusicEnabled(int f_area_id)
{
    m_global_enabled.insert(f_area_id, !m_global_enabled.value(f_area_id));
    if (m_global_enabled.value(f_area_id)) {
        sanitiseCustomMusicList(f_area_id);
    }
    emit sendAreaFMPacket(PacketFactory::createPacket("FM", musiclist(f_area_id)), f_area_id);
    return m_global_enabled.value(f_area_id);
}

void MusicManager::sanitiseCustomMusicList(int f_area_id)
{
    MusicList l_sanitised_list;
    QStringList l_sanitised_ordered = m_customs_ordered.value(f_area_id);
    for (auto iterator = m_custom_lists->value(f_area_id).keyBegin(),
              end = m_custom_lists->value(f_area_id).keyEnd();
         iterator != end; ++iterator) {
        QString l_key = iterator.operator*();
        if (!m_root_list.contains(l_key)) {
            l_sanitised_list.insert(l_key, m_custom_lists->value(f_area_id).value(l_key));
        }
        else {
            l_sanitised_ordered.removeAll(l_key);
        }
    }
    m_custom_lists->insert(f_area_id, l_sanitised_list);
    m_customs_ordered.insert(f_area_id, l_sanitised_ordered);
}

void MusicManager::clearCustomMusicList(int f_area_id)
{
    m_custom_lists->remove(f_area_id);
    m_custom_lists->insert(f_area_id, {});
    m_customs_ordered.remove(f_area_id);
    m_customs_ordered.insert(f_area_id, {});

    emit sendAreaFMPacket(PacketFactory::createPacket("FM", musiclist(f_area_id)), f_area_id);
}

QPair<QString, int> MusicManager::songInformation(QString f_song_name, int f_area_id)
{
    if (m_root_list.contains(f_song_name)) {
        return m_root_list.value(f_song_name);
    }
    return m_custom_lists->value(f_area_id).value(f_song_name);
}

bool MusicManager::isCustom(int f_area_id, QString f_song_name)
{
    if (m_customs_ordered.value(f_area_id).contains(f_song_name, Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

void MusicManager::reloadRequest()
{
    m_root_list = ConfigManager::musiclist();
    m_root_ordered = ConfigManager::ordered_songs();
    m_cdns = ConfigManager::cdnList();
}

void MusicManager::userJoinedArea(int f_area_index, int f_user_id)
{
    emit sendFMPacket(PacketFactory::createPacket("FM", musiclist(f_area_index)), f_user_id);
}
