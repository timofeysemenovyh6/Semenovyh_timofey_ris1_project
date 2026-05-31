cmake_minimum_required(VERSION 3.16)
project(car_picker_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

qt_add_executable(car_picker_qt
  src/main.cpp
  src/MainWindow.h
  src/MainWindow.cpp
)

set(CAR_PICKER_STATIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../static")
set(CAR_PICKER_QT_ASSETS "")
foreach(_asset IN ITEMS landing-bg.png logo.png)
  set(_path "${CAR_PICKER_STATIC_DIR}/${_asset}")
  if(EXISTS "${_path}")
    list(APPEND CAR_PICKER_QT_ASSETS "${_path}")
  endif()
endforeach()
if(CAR_PICKER_QT_ASSETS)
  qt_add_resources(car_picker_qt "qt_assets"
    PREFIX "/assets"
    FILES ${CAR_PICKER_QT_ASSETS})
endif()

target_link_libraries(car_picker_qt PRIVATE Qt6::Widgets Qt6::Network)

