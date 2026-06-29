cmake_minimum_required(VERSION 3.16)

project(now2play-lite VERSION 1.0.0 LANGUAGES CXX)

add_compile_definitions(APP_VERSION="${PROJECT_VERSION}")
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- now2sdk SDK Konfigürasyonu ---
set(SDK_DIR "${CMAKE_SOURCE_DIR}/SDK")
set(LML_LIB "${SDK_DIR}/libnow2sdk.so")

if(NOT EXISTS ${LML_LIB})
    message(FATAL_ERROR "libnow2sdk.so bulunamadı: ${LML_LIB}")
endif()

# FFmpeg (LFile.h tarafından kullanılır)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC  REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL   REQUIRED libavutil)
pkg_check_modules(SWSCALE  REQUIRED libswscale)

# --- Bağımlılıklar (Qt) ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Widgets
    OpenGLWidgets
    Concurrent
    OpenGL
    Multimedia
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Widgets
    OpenGLWidgets
    Concurrent
    Multimedia
)

# --- Proje Dosyaları ---
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    filedialog.cpp
    filedialog.h
    filedialog.ui

    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui
    trimdialog.cpp
    trimdialog.h
    trimdialog.ui
    rangeslider.cpp
    rangeslider.h

    rowcolordelegate.h
    globalproperties.h
    resources.qrc
    splashwidget.cpp
    splashwidget.h
    splashwidget.ui
)

# --- Executable Oluşturma ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(now2play-lite
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(now2play-lite
        ${PROJECT_SOURCES}
    )
endif()

# --- Include ve Link Ayarları ---
target_include_directories(now2play-lite PRIVATE
    ${SDK_DIR}/header/now2sdk
    ${SDK_DIR}/header/ndi/include
    ${SDK_DIR}/header/decklink
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(now2play-lite PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Multimedia
    ${LML_LIB}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
)

# RPATH'i tüm alt bağımlılıklar (dependencies of dependencies) için aktif kılmak adına RUNPATH yerine eski RPATH kullan
target_link_options(now2play-lite PRIVATE "-Wl,--disable-new-dtags")

# --- RPATH Ayarları: Kendi yanındaki kütüphaneleri yüklemesini sağlar ---
set_target_properties(now2play-lite PROPERTIES
    OUTPUT_NAME "now2play-lite.bin"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# --- Native Launcher ---
# .sh yerine direkt calistirilabilir C++ tabanli launcher (LD_LIBRARY_PATH ayarlar)
add_executable(now2play_launcher launcher.cpp)
set_target_properties(now2play_launcher PROPERTIES OUTPUT_NAME "now2play-lite")

# --- Desktop Dosyasını Dinamik Yollarla Oluştur ---
configure_file(
    ${CMAKE_SOURCE_DIR}/now2play-lite.desktop.in
    ${CMAKE_BINARY_DIR}/now2play-lite.desktop
    @ONLY
)

# --- Post-build: SDK'daki tüm kütüphaneleri uygulamanın yanına kopyala ---
add_custom_command(TARGET now2play-lite POST_BUILD
    COMMAND sh -c "cp -Lf ${SDK_DIR}/*.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf ${CMAKE_SOURCE_DIR}/now2play.png $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -rLf /usr/lib/qt6/plugins/platforms $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -rLf /usr/lib/qt6/plugins/imageformats $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -rLf /usr/lib/qt6/plugins/iconengines $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -rLf /usr/lib/qt6/plugins/xcbglintegrations $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -rLf /usr/lib/qt6/plugins/wayland-graphics-integration-client $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libxcb-cursor.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6XcbQpa.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6Wayland*.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6EglFSDeviceIntegration.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6Multimedia.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6Network.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6Svg*.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND sh -c "cp -Lf /usr/lib/libQt6PrintSupport.so* $<TARGET_FILE_DIR:now2play-lite>/"
    COMMAND ${CMAKE_SOURCE_DIR}/collect_all_libs.sh $<TARGET_FILE:now2play-lite> $<TARGET_FILE_DIR:now2play-lite>
    COMMAND ${CMAKE_SOURCE_DIR}/deploy_libs.sh $<TARGET_FILE_DIR:now2play-lite>
)

# --- Yükleme (Install) ---
include(GNUInstallDirs)
install(TARGETS now2play-lite
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(now2play-lite)
endif()