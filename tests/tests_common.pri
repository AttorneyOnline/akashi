QT += network websockets core sql testlib

CONFIG += qt console warn_on depend_includepath testcase no_testcase_installs
CONFIG -= app_bundle

DESTDIR = $$PWD/../bin_tests

win32: LIBS += -L$$PWD/../bin/ -llib
else:unix: LIBS += -L$$PWD/../bin/ -llib

INCLUDEPATH += $$PWD/../lib
DEPENDPATH += $$PWD/../lib
