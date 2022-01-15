#include "include/music_manager.h"

MusicManager::MusicManager(QMap<QString, QPair<QString, float> > f_root_list)
{
    m_custom_lists = new QHash<int,QMap<QString,QPair<QString,float>>>;
    m_root_list = f_root_list;
}

MusicManager::~MusicManager()
{

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
