TEMPLATE = subdirs

SUBDIRS += \
  core \
  akashi \
  tests

# Just like how "CONFIG += ordered" is considered harmful a practice for handling
# internal dependecies, so is qmake considered harmful a tool for handling projects
# as Qt expects you to handle them.
#
# Too bad.
CONFIG += ordered
