TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccGui/include/ \
    ../sccCommon/include/ \
    ../sccApi/include/
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing -DBMC_IS_HOST
QT += core \
    network
HEADERS += ../sccCommon/include/sccKitVersion.h \
    include/sccBmc.h \
    ../sccCommon/include/coreMask.h \
    ../sccCommon/include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccApi/include/sccApi.h \
    ../sccGui/include/sccExtApi.h
SOURCES +=     src/sccBmc.cpp \
    src/main.cpp \
    ../sccCommon/src/coreMask.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp \
    ../sccApi/src/sccApi.cpp \
    ../sccGui/src/sccExtApi.cpp
    
