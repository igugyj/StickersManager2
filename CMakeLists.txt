cmake_minimum_required(VERSION 3.16)
project(StickersManager)

# 单配置生成器必须给出构建类型，留空会让各目标拿到空值。默认 Debug（沿用旧命令行习惯）。
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Debug")
endif ()
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
message(STATUS "compiler = ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# ====================== 全局配置开关 ======================
# 控制台调试输出（ON = 控制台窗口 + qDebug 直达控制台 + LOG_TO_CONSOLE 生效）。
# 默认由构建类型推导：Release = OFF，其它 = ON；可用 -DCONSOLE=ON/OFF 单独覆盖
# （例如在 Release 包上临时开控制台抓用户 bug）。
if (NOT DEFINED CONSOLE)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CONSOLE OFF)
    else ()
        set(CONSOLE ON)
    endif ()
endif ()
message(STATUS "CONSOLE = ${CONSOLE}")
if (CONSOLE)
    add_compile_definitions(CONSOLE)
endif ()

# 启用 Unicode 模式（Windows 平台）
if (WIN32)
    add_compile_definitions(UNICODE _UNICODE)
endif ()

# 测试版（Debug 构建）锁文件用独立名字，可与已安装的正式版同时运行
# （Release 构建不定义该宏，锁名保持 <user>_Stickers Manager.lock 不变）
add_compile_definitions($<$<CONFIG:Debug>:SM_TEST_BUILD>)

# Qt 路径不再硬编码：查找优先级 -DQT_PATH（CI/CLI）> .vscode/settings.json 的
# cmake.configureArgs（VSCode CMake Tools）> 系统默认查找。只在显式给出时设置，
# 否则会清掉 configureArgs 传入的 CMAKE_PREFIX_PATH。
set(QT_PATH "" CACHE PATH "Qt installation root (CI: -DQT_PATH=<Qt6_DIR>; VSCode: .vscode/settings.json)")
if (QT_PATH)
    set(CMAKE_PREFIX_PATH ${QT_PATH})
endif ()
message(STATUS "Qt lookup: QT_PATH='${QT_PATH}' CMAKE_PREFIX_PATH='${CMAKE_PREFIX_PATH}'")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# UTF-8 源码：MSVC 默认按 ANSI 解释，中文注释/字符串会告警或乱码；
# MinGW 显式指定输入/执行字符集，避免中文控制台输出乱码
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
elseif (MSVC)
    add_compile_options(/utf-8)
endif ()
# 设置第三方库路径
set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
        Network
        REQUIRED)
FILE(GLOB_RECURSE SOURCE_FILES "./src/*.cpp")
FILE(GLOB_RECURSE HEADER_FILES "./src/*.h")
FILE(GLOB_RECURSE HPP_FILES "./src/*.hpp")

# 头文件搜索路径
include_directories(
        ./
        src/
        src/core/
        src/ui/
        src/utils/
        thirdparty/stb
)
# 资源文件
set(RESOURCE_FILES
        resources.qrc
)

add_executable(${PROJECT_NAME}
        ${SOURCE_FILES}
        ${HEADER_FILES}
        ${HPP_FILES}
        ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_NAME}
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Concurrent
        Qt::Network
)
# 设置包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
        src
        src/core/
        src/ui/
        src/utils/
        ${CMAKE_CURRENT_BINARY_DIR}
        ${THIRDPARTY_DIR}/stb
)
# 子系统统一由 CONSOLE 变量驱动（原为 add_executable 的 WIN32 标志与图标块内的属性双写）
if (CONSOLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)
else ()
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif ()

# 设置图标（Windows）
if (WIN32)
    # 1. 定义图标文件路径和rc脚本路径
    set(APP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/st.ico")
    set(APP_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.rc")

    # 2. 检查图标和rc脚本是否存在
    if (EXISTS ${APP_ICON_FILE} AND EXISTS ${APP_RC_FILE})
        # 3. 将rc脚本文件加入项目编译（核心：通过rc脚本绑定图标）
        target_sources(${PROJECT_NAME} PRIVATE ${APP_RC_FILE})
    else ()
        message(WARNING "Program icon file or RC script is missing, the default icon will be used!")
    endif ()
endif ()

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    find_program(WINDEPLOYQT windeployqt PATHS ${CMAKE_PREFIX_PATH}/bin NO_DEFAULT_PATH)
    if(WINDEPLOYQT)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT} --no-translations $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Deploying Qt runtime libraries via windeployqt..."
        )
    else()
        message(WARNING "windeployqt not found, skipping Qt auto-deployment")
    endif()
endif()
