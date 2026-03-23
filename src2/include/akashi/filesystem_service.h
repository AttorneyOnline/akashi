#ifndef AKASHI_FILESYSTEM_SERVICE_H
#define AKASHI_FILESYSTEM_SERVICE_H

#include "akashi_core_export.h"

#include <QDir>
#include <QString>

#include <optional>

namespace akashi {

// Resolves file paths inside a fixed boundary so a path can never escape it.
class AKASHI_CORE_EXPORT FileSystemService
{
  public:
    enum class Scope
    {
        // User-driven file operations, confined to the storage folder.
        Storage,
        // Application file operations, confined to the application folder.
        System,
    };

    explicit FileSystemService(const QString &f_app_root = QDir::currentPath());

    // The boundary folder of a scope.
    QString root(Scope f_scope) const;

    // The absolute path for a relative path within the scope's boundary.
    // Returns nullopt if the path would escape the boundary.
    std::optional<QString> resolve(Scope f_scope, const QString &f_relative_path) const;

  private:
    QString m_app_root;
    QString m_storage_root;
};

} // namespace akashi

#endif // AKASHI_FILESYSTEM_SERVICE_H
