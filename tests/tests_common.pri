QT += network websockets core sql testlib

CONFIG += qt console warn_on depend_includepath testcase no_testcase_installs
CONFIG -= app_bundle
unix: CONFIG += c++1z
win32: CONFIG += c++2a

coverage {
    LIBS += -lgcov
}

DESTDIR = $$PWD/../bin_tests

INCLUDEPATH += $$PWD/../src

LIBS += -L$$PWD/../bin -lcore
