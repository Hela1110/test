cmake_minimum_required(VERSION 3.14)
project(shopping_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS 
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
    src/mainwindow/mainwindow.cpp
    src/shopping/shoppingcart.cpp
    src/chat/chatwindow.cpp
)

set(PROJECT_HEADERS
    include/login/loginwindow.h
    include/mainwindow/mainwindow.h
    include/shopping/shoppingcart.h
    include/chat/chatwindow.h
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
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Sql
)