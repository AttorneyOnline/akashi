#pragma once

#include "akashi/log_writer.h"

#include <QHash>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

#include <memory>

namespace akashi {

class WriterSql : public ILogWriter
{
  public:
    explicit WriterSql(const QString &f_db_path, const QString &f_connection_name = {});
    ~WriterSql() override;

    QString writerId() const override;
    void write(const LogEvent &f_event) override;
    void flush() override;
    void maintenance() override;

  private:
    bool ensureOpen();
    void beginIfNeeded();
    void migrate(QSqlDatabase &f_db);
    void loadCaches(QSqlDatabase &f_db);
    int typeId(const QString &f_name);
    int identityId(const QString &f_ipid, const QString &f_hwid);

    void writeConnection(const LogEvent &f_event);
    void writeEvent(const LogEvent &f_event);

    QString m_db_path;
    QString m_connection_name;
    bool m_opened = false;
    bool m_in_transaction = false;

    QHash<QString, int> m_type_cache;
    QHash<QPair<QString, QString>, int> m_identity_cache;

    std::unique_ptr<QSqlQuery> m_insert_type;
    std::unique_ptr<QSqlQuery> m_select_type;
    std::unique_ptr<QSqlQuery> m_insert_identity;
    std::unique_ptr<QSqlQuery> m_select_identity;
    std::unique_ptr<QSqlQuery> m_insert_connection;
    std::unique_ptr<QSqlQuery> m_insert_event;
};

} // namespace akashi
