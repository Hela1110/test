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
# 让 AUTOUIC 能在 ui 目录中查找 .ui 文件
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")

# 编译器由外部环境/生成器决定，不在此强制指定，避免与 Qt 工具链不匹配

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
    src/mainwindow/mainwindow.cpp
    src/login/loginwindow.cpp
    src/register/registerdialog.cpp
    src/shopping/shoppingcart.cpp
    src/chat/chatwindow.cpp
)

set(PROJECT_HEADERS
    include/login/loginwindow.h
    include/register/registerdialog.h
    include/mainwindow/mainwindow.h
    include/shopping/shoppingcart.h
    include/chatwindow.h
)

set(PROJECT_UIS
    ui/loginwindow.ui
    ui/registerdialog.ui
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

# 避免 mainwindow.cpp 因手写的 ui_mainwindow.h 触发 AUTOUIC 解析
set_source_files_properties(src/mainwindow/mainwindow.cpp PROPERTIES SKIP_AUTOUIC ON)

# ---- Packaging (Windows) ----
if (WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if (WINDEPLOYQT_EXECUTABLE)
        set(DIST_DIR "${CMAKE_BINARY_DIR}/dist")
        add_custom_command(
            TARGET shopping_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:shopping_client>" "${DIST_DIR}/"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --release --dir "${DIST_DIR}" "$<TARGET_FILE:shopping_client>"
            COMMENT "Deploying Qt runtime with windeployqt to ${DIST_DIR}"
        )

        add_custom_target(package_app
            DEPENDS shopping_client
            COMMENT "Package app to dist directory using windeployqt")
    else()
        message(WARNING "windeployqt not found; 'package_app' target will not be available")
    endif()
endif()