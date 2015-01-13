TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS_DEBUG = \
    -std=gnu99 -D_GNU_SOURCE -Wpointer-arith -Wcast-align -Wcast-qual -Wstrict-prototypes -Wshadow -Waggregate-return -Wmissing-prototypes -Wnested-externs -Wsign-compare -ggdb3 -DDVBPSI_DIST

QMAKE_CFLAGS_RELEASE = \
    -std=gnu99 -D_GNU_SOURCE -Wpointer-arith -Wcast-align -Wcast-qual -Wstrict-prototypes -Wshadow -Waggregate-return -Wmissing-prototypes -Wnested-externs -Wsign-compare -ggdb3 -DDVBPSI_DIST

LIBS += -ldvbpsi -pthread -lm

SOURCES += main.c \
    decode_sdt.c \
    decode_pat.c \
    decode_pmt.c

HEADERS += \
    streamfiletoip.h \
    config.h \
    dvbcsa_pv.h


unix|win32: LIBS += -ldvbcsa
