QT += network websockets core sql testlib

CONFIG += qt console warn_on depend_includepath testcase no_testcase_installs
CONFIG -= app_bundle

DESTDIR = $$PWD/../bin_tests

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../lib/release/ -llib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../lib/debug/ -llib
else:unix: LIBS += -L$$OUT_PWD/../../lib/ -llib

INCLUDEPATH += $$PWD/../lib
DEPENDPATH += $$PWD/../lib
