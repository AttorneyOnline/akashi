#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QDir>
#include <QString>

#include <optional>

namespace akashi {

class AKASHI_CORE_EXPORT FileSystemService : public IService
{
  public:
    enum class Scope
    {
        Storage,
        System,
    };

    explicit FileSystemService(const QString &f_app_root = QDir::currentPath());

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    QString root(Scope f_scope) const;
    std::optional<QString> resolve(Scope f_scope, const QString &f_relative_path) const;

    QString configRoot() const;
    QString dataRoot() const;
    QString storageRoot() const;
    QString pluginsRoot() const;

    QString pluginDataDir(const QString &f_plugin_id);
    std::optional<QString> pluginResolve(const QString &f_plugin_id, const QString &f_relative_path) const;

  private:
    QString m_app_root;
    QString m_storage_root;
};

} // namespace akashi

