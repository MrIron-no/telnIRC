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

telnirc_CPPFLAGS = -Iinclude @OPENSSL_CFLAGS@ @NCURSES_CFLAGS@
telnirc_CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -g
telnirc_LDADD = @OPENSSL_LIBS@ @NCURSES_LIBS@ -lpthread
