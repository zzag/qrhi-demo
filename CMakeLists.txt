cmake_minimum_required(VERSION 3.14)

project(rhi-hello-world LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Gui ShaderTools REQUIRED)

add_executable(rhi-hello-world
  main.cpp
  resources.qrc
  window.cpp
)
qt_add_shaders(rhi-hello-world "shaders"
    PREFIX
        "/shaders"
    FILES
        texture.frag
        texture.vert
)
target_link_libraries(rhi-hello-world
    Qt::Core
    Qt::Gui
    Qt::GuiPrivate
)
