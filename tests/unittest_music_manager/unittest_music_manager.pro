QT -= gui

include(../tests_common.pri)

coverage
{
    LIBS += -lgcov
}

SOURCES +=  tst_unittest_music_manager.cpp
