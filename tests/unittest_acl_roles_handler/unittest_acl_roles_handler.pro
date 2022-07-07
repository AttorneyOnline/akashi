QT -= gui

include(../tests_common.pri)

coverage
{
    LIBS += -lgcov
}

SOURCES +=  tst_unittest_acl_roles_handler.cpp
