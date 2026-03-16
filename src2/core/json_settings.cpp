#include "core/json_settings.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

// Turns nested JSON objects into flat QSettings keys like "Options/port".
static void flattenObject(const QString &prefix, const QJsonObject &object, QSettings::SettingsMap &map)
{
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        const QString key = prefix.isEmpty() ? iterator.key() : prefix + "/" + iterator.key();
        const QJsonValue value = iterator.value();
        if (value.isObject()) {
            flattenObject(key, value.toObject(), map);
        }
        else if (value.isArray()) {
            map.insert(key, value.toArray().toVariantList());
        }
        else {
            map.insert(key, value.toVariant());
        }
    }
}

// Turns a flat QSettings key back into nested JSON objects.
static void insertNested(QJsonObject &object, QStringList path, const QVariant &value)
{
    const QString key = path.takeFirst();
    if (path.isEmpty()) {
        object.insert(key, QJsonValue::fromVariant(value));
        return;
    }
    QJsonObject child = object.value(key).toObject();
    insertNested(child, path, value);
    object.insert(key, child);
}

bool JsonSettings::readJsonFile(QIODevice &device, QSettings::SettingsMap &map)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(device.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    flattenObject(QString(), document.object(), map);
    return true;
}

bool JsonSettings::writeJsonFile(QIODevice &device, const QSettings::SettingsMap &map)
{
    QJsonObject root;
    for (auto iterator = map.begin(); iterator != map.end(); ++iterator) {
        insertNested(root, iterator.key().split(QLatin1Char('/')), iterator.value());
    }
    return device.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) != -1;
}

QSettings::Format JsonSettings::format()
{
    static const QSettings::Format format = QSettings::registerFormat("json", readJsonFile, writeJsonFile);
    return format;
}
