QT += network websockets core sql

TEMPLATE = app

CONFIG += c++2a console

coverage {
  LIBS += -lgcov
}

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CXXFLAGS_WARN_OFF -= -Wunused-parameter

DESTDIR = $$PWD/bin

RC_ICONS = resource/icon/akashi.ico

INCLUDEPATH += src

SOURCES += \
  src/main.cpp

LIBS += -L$$PWD/bin -lcore
