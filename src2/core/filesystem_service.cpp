#include "akashi/filesystem_service.h"

namespace akashi {

FileSystemService::FileSystemService(const QString &f_app_root) :
    m_app_root(QDir::cleanPath(QDir(f_app_root).absolutePath())),
    m_storage_root(QDir::cleanPath(m_app_root + "/storage"))
{}

QString FileSystemService::root(Scope f_scope) const
{
    return f_scope == Scope::Storage ? m_storage_root : m_app_root;
}

std::optional<QString> FileSystemService::resolve(Scope f_scope, const QString &f_relative_path) const
{
    const QString l_boundary = root(f_scope);
    // absoluteFilePath ignores the boundary for absolute inputs, so they are caught by the check below.
    const QString l_candidate = QDir::cleanPath(QDir(l_boundary).absoluteFilePath(f_relative_path));
    if (l_candidate == l_boundary || l_candidate.startsWith(l_boundary + "/")) {
        return l_candidate;
    }
    return std::nullopt;
}

} // namespace akashi
