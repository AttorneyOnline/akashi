#include "filesystemsupervisor.h"
#include "serviceregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {
int FS_WARNING_LOW_STORAGE = 10;
}

FileSystemSupervisor::FileSystemSupervisor(ServiceRegistry *f_registry, QObject *parent) : m_registry{f_registry}, Service{parent}
{
    const QString l_appImagepath = qgetenv("APPIMAGE");
    m_storage_root = l_appImagepath.isEmpty() ? QCoreApplication::applicationDirPath() : QString(QFileInfo(l_appImagepath).absolutePath());

    m_storage_info.setPath(m_storage_root);

    double free_percentage = freeStorageInPercent();

    if (free_percentage < FS_WARNING_LOW_STORAGE) {
        qWarning().noquote() << "WARNING: SYSTEM IS LOW ON STORAGE! FREE SPACE" << QString::number(free_percentage, 'f', 2) + "%";
    }
    m_registry->registerService(this);
}

bool FileSystemSupervisor::exists(QString f_path, AccessMode f_mode)
{
    QString l_directory_path = (f_mode == SERVER) ? m_storage_root : m_storage_root + "/storage/";
    std::optional<QString> l_file_path = parsePath(l_directory_path, f_path);

    if (!l_file_path) {
        return false;
    }
    return QFile::exists(l_file_path.value());
}

const QStringList FileSystemSupervisor::directoryContents(QString f_path, AccessMode f_mode)
{
    QString l_directory_path = (f_mode == SERVER) ? m_storage_root : m_storage_root + "/storage/";
    std::optional<QString> l_file_path = parsePath(l_directory_path, f_path);

    if (!l_file_path) {
        return {};
    }

    const QFileInfoList items = QDir(*l_file_path).entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::NoSort);
    QStringList entries;
    for (const QFileInfo &file : items) {
        entries.append(file.filePath());
    }
    return entries;
}

std::optional<QFile *> FileSystemSupervisor::handle(const QString &f_path, AccessMode f_mode)
{
    QString l_directory_path = (f_mode == SERVER) ? m_storage_root : m_storage_root + "/storage/";
    std::optional<QString> l_file_path = parsePath(l_directory_path, f_path);

    if (!l_file_path || !exists(f_path)) {
        return std::nullopt;
    }

    QFile *l_file = new QFile(*l_file_path);
    return l_file;
}

double FileSystemSupervisor::freeStorageInPercent()
{
    qint64 l_available_space = m_storage_info.bytesAvailable();
    qint64 l_total_space = m_storage_info.bytesTotal();

    return (static_cast<double>(l_available_space) / l_total_space) * 100.0;
}

std::optional<QString> FileSystemSupervisor::parsePath(const QString &f_directory_path, const QString &f_relativePath) const
{
    QString l_file_target(QDir(f_directory_path).absoluteFilePath(f_relativePath));
    QString l_parsed_target = QDir(l_file_target).absolutePath();

    if (!l_parsed_target.startsWith(f_directory_path)) {
        qDebug() << "Attempted escaping of path. Base:" << f_directory_path << "\nAttempted:" << l_file_target;
        return std::nullopt;
    }
    return std::make_optional<QString>(l_parsed_target);
}
