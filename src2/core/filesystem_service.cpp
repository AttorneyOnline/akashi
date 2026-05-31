#include "akashi/filesystem_service.h"

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

} // namespace akashi
