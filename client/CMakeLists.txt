cmake_minimum_required(VERSION 3.14)
project(shopping_client)

# 设置 Qt6 的安装路径
set(Qt6_DIR "D:/Qt/6.8.3/mingw_64/lib/cmake/Qt6")
set(CMAKE_PREFIX_PATH "D:/Qt/6.8.3/mingw_64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 确保使用正确的编译器
set(CMAKE_C_COMPILER "C:/msys64/ucrt64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/ucrt64/bin/g++.exe")

find_package(Qt6 COMPONENTS 
    Core
    Gui
    Widgets
    Network
    Sql
    REQUIRED
)

set(PROJECT_SOURCES
    src/main.cpp
    src/login/loginwindow.cpp
    src/shopping/shoppingcart.cpp
    src/chat/chatwindow.cpp
)

set(PROJECT_HEADERS
    include/loginwindow.h
    include/mainwindow.h
    include/shoppingcart.h
    include/chatwindow.h
)

set(PROJECT_UIS
    ui/loginwindow.ui
    ui/mainwindow.ui
    ui/shoppingcart.ui
    ui/chatwindow.ui
)

add_executable(shopping_client
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_UIS}
    resources/resources.qrc
)

target_include_directories(shopping_client PRIVATE include)

target_link_libraries(shopping_client PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)