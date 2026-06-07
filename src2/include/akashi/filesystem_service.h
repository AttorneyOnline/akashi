#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QByteArray>
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

    /**
     * @brief True when writing f_bytes at f_path still leaves the volume
     * its safety margin of free space. An unrecognizable volume counts
     * as no room.
     */
    bool hasSpaceFor(const QString &f_path, qint64 f_bytes) const;

    /**
     * @brief Writes the whole file atomically (temp file, then rename),
     * refusing when the disk would drop under the safety margin.
     *
     * @return The reason the write was refused, or nothing on success.
     */
    std::optional<QString> writeFile(const QString &f_absolute_path, const QByteArray &f_data);

  private:
    // A write may never push the volume under this much free space.
    static constexpr int s_free_margin_percent = 15;

    QString m_app_root;
    QString m_storage_root;
};

} // namespace akashi

