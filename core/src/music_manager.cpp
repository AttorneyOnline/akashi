#include "include/music_manager.h"

MusicManager::MusicManager(QMap<QString, QPair<QString, float>> f_root_list)
{
    m_custom_lists = new QHash<int,QMap<QString,QPair<QString,float>>>;
    m_root_list = f_root_list;
}

MusicManager::~MusicManager()
{

}

QStringList MusicManager::musiclist(int f_area_id)
{
    if (m_global_enabled[f_area_id]) {
        QStringList l_combined_list = m_root_list.keys();

        for (auto iterator = m_custom_lists->value(f_area_id).keyBegin(),
             end = m_custom_lists->value(f_area_id).keyEnd(); iterator != end; ++iterator)
        {
            l_combined_list.append(iterator.operator*());
        }
        return l_combined_list;

    }
    return m_custom_lists->value(f_area_id).keys();
}

QStringList MusicManager::rootMusiclist()
{
    return m_root_list.keys();
}

bool MusicManager::registerArea(int f_area_id)
{
    if(m_custom_lists->contains(f_area_id)) {
        //This area is already registered. We can't add it.
        return false;
    }
    m_custom_lists->insert(f_area_id,{});
    m_global_enabled.insert(f_area_id,true);
    return true;
}

bool MusicManager::validateSong(QString f_song_name, QStringList f_approved_cdns)
{
    QStringList l_extensions = {".opus", ".ogg", ".mp3", ".wav" };

    bool l_cdn_approved = false;
    //Check if URL formatted.
    if (f_song_name.contains("/")) {
        //Only allow HTTPS/HTTP sources.
        if (f_song_name.startsWith("https://") | f_song_name.startsWith("http://")) {
            for (const QString &l_cdn : qAsConst(f_approved_cdns)) {
                //Iterate trough all available CDNs to find an approved match
                if (f_song_name.startsWith("https://" + l_cdn + "/", Qt::CaseInsensitive)
                        | f_song_name.startsWith("http://" + l_cdn + "/", Qt::CaseInsensitive)) {
                    l_cdn_approved = true;
                    break;
                }
            }
            if (!l_cdn_approved) {
                return false;
            }
        }
        else {
            return false;
        }
    }

    bool l_suffix_found = false;;
    for (const QString &suffix : qAsConst(l_extensions)) {
        if (f_song_name.endsWith(suffix)){
            l_suffix_found = true;
            break;
        }
    }

    if (!l_suffix_found) {
        return false;
    }

    return true;
}

bool MusicManager::addCustomSong(QString f_song_name, QString f_real_name, float f_duration, int f_area_id)
{
    //Validate if simple name.
    QString l_song_name = f_song_name;
    if (f_song_name.split(".").size() == 1) {
        l_song_name = l_song_name + ".opus";
    }

    if (!(validateSong(l_song_name, m_cdns) && validateSong(f_real_name, m_cdns))) {
        return false;
    }

    //Avoid conflicts by checking if it exits.
    if (m_root_list.contains(l_song_name) && m_global_enabled[f_area_id]) {
        return false;
    }

    if (m_custom_lists->value(f_area_id).contains(f_song_name)) {
        return false;
    }

    //There should be a way to directly insert into the Hash.
    QMap<QString,QPair<QString,float>> l_custom_list = m_custom_lists->value(f_area_id);
    l_custom_list.insert(l_song_name,{f_real_name,f_duration});
    m_custom_lists->insert(f_area_id,l_custom_list);
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

    //Avoid conflicts by checking if it exits.
    if (m_root_list.contains(l_category_name) && m_global_enabled[f_area_id]) {
        return false;
    }

    if (m_custom_lists->value(f_area_id).contains(l_category_name)) {
        return false;
    }

    QMap<QString,QPair<QString,float>> l_custom_list = m_custom_lists->value(f_area_id);
    l_custom_list.insert(l_category_name,{l_category_name,0});
    m_custom_lists->insert(f_area_id,l_custom_list);
    return true;
}

bool MusicManager::toggleRootEnabled(int f_area_id)
{
    m_global_enabled[f_area_id] = !m_global_enabled[f_area_id];
    if (m_global_enabled.value(f_area_id)) {
        sanitiseCustomList(f_area_id);
    }
    return m_global_enabled.value(f_area_id);
}

void MusicManager::sanitiseCustomList(int f_area_id)
{
    QMap<QString,QPair<QString,float>> l_sanitised_list;
    for (auto iterator = m_custom_lists->value(f_area_id).keyBegin(),
         end = m_custom_lists->value(f_area_id).keyEnd(); iterator != end; ++iterator)
    {
        QString l_key = iterator.operator*();
        if (!m_root_list.contains(l_key)) {
            l_sanitised_list.insert(l_key, m_custom_lists->value(f_area_id).value(l_key));
        }
    }
    m_custom_lists->insert(f_area_id, l_sanitised_list);
}

void MusicManager::reloadRequest()
{
    m_root_list = ConfigManager::musiclist();
    m_cdns = ConfigManager::cdnList();
}
