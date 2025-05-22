AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = telnirc

telnirc_SOURCES = \
    src/main.cpp \
    src/telnirc.cpp \
    src/telnerv.cpp \
    src/config.cpp \
    src/misc.cpp \
    src/connection.cpp \
    src/UIManager.cpp

telnirc_CPPFLAGS = -Iinclude
telnirc_CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -g
telnirc_LDADD = -lncurses

if HAVE_OPENSSL
telnirc_LDADD += -lssl -lcrypto
endif