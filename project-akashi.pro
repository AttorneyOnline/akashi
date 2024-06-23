TEMPLATE = subdirs

SUBDIRS += \
  core \
  akashi \
  tests

core.file = core.pro
core.tests = tests
tests.depends = core
akashi.file = akashi.pro
akashi.depends = core
