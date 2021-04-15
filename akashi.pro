QT       += network websockets core sql
QT       -= gui
TEMPLATE = app

CONFIG += c++11 console

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
OBJECTS_DIR = $$PWD/build
MOC_DIR = $$PWD/build

RC_ICONS = resource/icon/akashi.ico

# Enable this to print network messages tothe console
#DEFINES += NET_DEBUG

SOURCES += src/advertiser.cpp \
    src/aoclient.cpp \
    src/aopacket.cpp \
    src/area_data.cpp \
    src/commands.cpp \
    src/commands/areas.cpp \
    src/commands/authentication.cpp \
    src/config_manager.cpp \
    src/db_manager.cpp \
    src/logger.cpp \
    src/main.cpp \
    src/packets.cpp \
    src/server.cpp \
    src/testimony_recorder.cpp \
    src/ws_client.cpp \
    src/ws_proxy.cpp


HEADERS += include/advertiser.h \
    include/aoclient.h \
    include/aopacket.h \
    include/area_data.h \
    include/config_manager.h \
    include/db_manager.h \
    include/logger.h \
    include/server.h \
    include/ws_client.h \
    include/ws_proxy.h
