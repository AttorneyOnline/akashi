#include "akashi/filesystem_service.h"

#include <QFileInfo>
#include <QSaveFile>
#include <QStorageInfo>

namespace akashi {

FileSystemService::FileSystemService(const QString &f_app_root) :
    m_app_root(QDir::cleanPath(QDir(f_app_root).absolutePath())),
    m_storage_root(QDir::cleanPath(m_app_root + "/storage"))
{}

QString FileSystemService::serviceId() const
{
    return QStringLiteral("akashi.filesystem");
}

ServiceVersion FileSystemService::serviceVersion() const
{
    return {1, 0, 0};
}

QString FileSystemService::root(Scope f_scope) const
{
    return f_scope == Scope::Storage ? m_storage_root : m_app_root;
}

std::optional<QString> FileSystemService::resolve(Scope f_scope, const QString &f_relative_path) const
{
    const QString l_boundary = root(f_scope);
    const QString l_candidate = QDir::cleanPath(QDir(l_boundary).absoluteFilePath(f_relative_path));
    if (l_candidate == l_boundary || l_candidate.startsWith(l_boundary + "/")) {
        return l_candidate;
    }
    return std::nullopt;
}

QString FileSystemService::configRoot() const
{
    return QDir::cleanPath(m_app_root + "/config");
}

QString FileSystemService::dataRoot() const
{
    return QDir::cleanPath(m_app_root + "/data");
}

QString FileSystemService::storageRoot() const
{
    return m_storage_root;
}

QString FileSystemService::pluginsRoot() const
{
    return QDir::cleanPath(m_app_root + "/data/plugins");
}

QString FileSystemService::pluginDataDir(const QString &f_plugin_id)
{
    const QString l_dir = QDir::cleanPath(pluginsRoot() + "/" + f_plugin_id);
    QDir().mkpath(l_dir);
    return l_dir;
}

std::optional<QString> FileSystemService::pluginResolve(const QString &f_plugin_id, const QString &f_relative_path) const
{
    const QString l_boundary = QDir::cleanPath(pluginsRoot() + "/" + f_plugin_id);
    const QString l_candidate = QDir::cleanPath(QDir(l_boundary).absoluteFilePath(f_relative_path));
    if (l_candidate == l_boundary || l_candidate.startsWith(l_boundary + "/")) {
        return l_candidate;
    }
    return std::nullopt;
}

bool FileSystemService::hasSpaceFor(const QString &f_path, qint64 f_bytes) const
{
    // The target may not exist yet; the volume is that of the nearest
    // existing ancestor.
    QString l_probe = QFileInfo(f_path).absolutePath();
    while (!QDir(l_probe).exists()) {
        const QString l_parent = QFileInfo(l_probe).absolutePath();
        if (l_parent == l_probe) {
            break;
        }
        l_probe = l_parent;
    }
    const QStorageInfo l_volume(l_probe);
    if (!l_volume.isValid() || l_volume.bytesTotal() <= 0) {
        return false;
    }
    return l_volume.bytesAvailable() - f_bytes >= l_volume.bytesTotal() * s_free_margin_percent / 100;
}

std::optional<QString> FileSystemService::writeFile(const QString &f_absolute_path, const QByteArray &f_data)
{
    QDir().mkpath(QFileInfo(f_absolute_path).absolutePath());
    if (!hasSpaceFor(f_absolute_path, f_data.size())) {
        return QStringLiteral("Refusing to write %1: the disk would drop under its %2% free-space margin.")
            .arg(f_absolute_path)
            .arg(s_free_margin_percent);
    }
    QSaveFile l_file(f_absolute_path);
    if (!l_file.open(QIODevice::WriteOnly)) {
        return QStringLiteral("Could not open %1 for writing: %2").arg(f_absolute_path, l_file.errorString());
    }
    l_file.write(f_data);
    if (!l_file.commit()) {
        return QStringLiteral("Could not finish writing %1: %2").arg(f_absolute_path, l_file.errorString());
    }
    return std::nullopt;
}

} // namespace akashi
