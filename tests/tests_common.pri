QT += network websockets core sql testlib

CONFIG += qt console warn_on depend_includepath testcase no_testcase_installs
CONFIG -= app_bundle
unix: CONFIG += c++1z
win32: CONFIG += c++2a

coverage {
    LIBS += -lgcov
}

DESTDIR = $$PWD/../bin_tests

win32: LIBS += -L$$PWD/../bin/ -lcore
else:unix: LIBS += -L$$PWD/../bin/ -lcore

INCLUDEPATH += $$PWD/../core
DEPENDPATH += $$PWD/../core
