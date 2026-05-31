QT += widgets network
CONFIG += c++17

TEMPLATE = app
TARGET = car_picker_qt

SOURCES += \
  src/main.cpp \
  src/MainWindow.cpp

HEADERS += \
  src/MainWindow.h

RESOURCES += landing.qrc

# Qt 6 + новый Apple Clang: внутри Qt в qyieldcpu.h бывает -Wimplicit-function-declaration;
# при -Werror из Kit сборка падает — не считаем это фатальной ошибкой нашего кода.
QMAKE_CXXFLAGS += -Wno-error=implicit-function-declaration
