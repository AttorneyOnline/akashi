#pragma once

#include <QObject>
#include <service.h>

#include <QStorageInfo>

class QFile;

class FileSystemSupervisor : public Service
{
    Q_OBJECT
    Q_CLASSINFO("Author", "Salanto");
    Q_CLASSINFO("Version", "1.0.0");
    Q_CLASSINFO("Identifier", "akashi.filesystemsupervisor")

  public:
    enum AccessMode
    {
        USER,  // Limits all access to the storage folder
        SERVER // Allows access to files not in the storage folder
    };
    Q_ENUM(AccessMode);

    explicit FileSystemSupervisor(ServiceRegistry *f_registry, QObject *parent = nullptr);
    bool exists(QString f_path, AccessMode f_mode = USER);

    const QStringList directoryContents(QString f_path, AccessMode f_mode = USER);
    std::optional<QFile *> handle(const QString &f_path, AccessMode f_mode = USER);

  private:
    double freeStorageInPercent();
    std::optional<QString> parsePath(const QString &f_directory_path, const QString &f_relativePath) const;

    ServiceRegistry *m_registry;
    QStorageInfo m_storage_info;
    QString m_storage_root = "";
};
