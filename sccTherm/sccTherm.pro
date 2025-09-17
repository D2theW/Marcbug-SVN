TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccGui/include/ \
    ../sccCommon/include/ \
    ../sccApi/include/
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core \
    network
HEADERS += ../sccCommon/include/sccKitVersion.h \
    include/sccTherm.h \
    ../sccCommon/include/coreMask.h \
    ../sccCommon/include/messageHandler.h \
    ../sccApi/include/sccApi.h
SOURCES +=     src/sccTherm.cpp \
    src/main.cpp \
    ../sccCommon/src/coreMask.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccApi/src/sccApi.cpp
    
