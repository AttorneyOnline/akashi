TEMPLATE = subdirs

SUBDIRS += \
  core \
  akashi \
  tests

akashi.subdirs = akashi
core.subdirs = core
core.tests = tests

# How to make subdirs not suck. Simple, use depends.
akashi.depends = core
