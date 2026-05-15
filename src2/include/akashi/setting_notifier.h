#ifndef AKASHI_SETTING_NOTIFIER_H
#define AKASHI_SETTING_NOTIFIER_H

#include "akashi_core_export.h"

#include <QObject>

namespace akashi {

// Emits changed() when a config setting is modified on disk and reloaded.
// Subscribers connect to changed() and read the new value through the
// Setting<T> they already hold. One notifier per setting, created lazily
// by Setting<T>::notifier() and owned by the ConfigStore.
class AKASHI_CORE_EXPORT SettingNotifier : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;

  Q_SIGNALS:
    void changed();
};

} // namespace akashi

#endif // AKASHI_SETTING_NOTIFIER_H
