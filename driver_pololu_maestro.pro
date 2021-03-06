TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += \
    /opt/ros/melodic/include \
    $$PWD/../../devel/include

SOURCES += \
    src/driver.cpp \
    src/main.cpp \
    src/ros_node.cpp

DISTFILES += \
    CMakeLists.txt \
    README.md \
    package.xml

HEADERS += \
    src/driver.h \
    src/ros_node.h
