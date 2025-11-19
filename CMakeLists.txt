cmake_minimum_required(VERSION 3.16)

project(FFmpegPlayer2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia)

# FFmpeg 配置
set(FFMPEG_ROOT "D:/z_demo/ffmpeg-master-latest-win64-gpl-shared/ffmpeg-master-latest-win64-gpl-shared")

# 检查 FFmpeg 目录是否存在
if(NOT EXISTS ${FFMPEG_ROOT})
    message(WARNING "FFmpeg directory not found: ${FFMPEG_ROOT}")
endif()

include_directories(${FFMPEG_ROOT}/include)
link_directories(${FFMPEG_ROOT}/lib)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(FFmpegPlayer2  # 修正为 FFmpegPlayer2
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        videoplayer.h videoplayer.cpp
    )
else()
    if(ANDROID)
        add_library(FFmpegPlayer2 SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(FFmpegPlayer2
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 链接库 - 目标名称统一为 FFmpegPlayer2
target_link_libraries(FFmpegPlayer2 PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia  # 添加这行
    avcodec
    avformat
    avutil
    swscale
    swresample
)

# Windows 特定库
if(WIN32)
    target_link_libraries(FFmpegPlayer2 PRIVATE ws2_32 secur32 bcrypt)
endif()

# Windows DLL 复制
if(WIN32)
    # 使用通配符匹配 DLL 版本，避免硬编码版本号
    file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    foreach(DLL ${FFMPEG_DLLS})
        add_custom_command(TARGET FFmpegPlayer2 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL}"
            $<TARGET_FILE_DIR:FFmpegPlayer2>
        )
    endforeach()
endif()

# macOS 配置
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.FFmpegPlayer2)
endif()

set_target_properties(FFmpegPlayer2 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# 安装配置
include(GNUInstallDirs)
install(TARGETS FFmpegPlayer2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt6 最终化
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(FFmpegPlayer2)
endif()
