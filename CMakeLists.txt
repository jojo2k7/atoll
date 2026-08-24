cmake_minimum_required(VERSION 3.24)

project(atoll
    VERSION 0.1.0
    DESCRIPTION "A dynamic island for KDE Plasma"
    HOMEPAGE_URL "https://github.com/atoll-shell/atoll"
    # C is only here for the Wayland protocol code wayland-scanner generates.
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(FeatureSummary)

find_package(ECM 6.0 QUIET)
if(ECM_FOUND)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
endif()

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui GuiPrivate Qml Quick QuickControls2 DBus Svg Network ShaderTools)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n)
find_package(LayerShellQt REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DBus1 REQUIRED IMPORTED_TARGET dbus-1)

# Staying visible while the session is locked needs one KDE protocol, generated
# from its description at build time. Without wayland-scanner the island still
# builds; it just vanishes with the rest of the session when the screen locks.
pkg_check_modules(Wayland IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
if(Wayland_FOUND AND WAYLAND_SCANNER_EXECUTABLE)
    set(ATOLL_LOCKSCREEN_OVERLAY ON)
else()
    set(ATOLL_LOCKSCREEN_OVERLAY OFF)
    message(STATUS "wayland-scanner or wayland-client missing: the island will not be shown on the lock screen")
endif()
add_feature_info(LockScreenOverlay ATOLL_LOCKSCREEN_OVERLAY "keep the island visible while the screen is locked")

qt_standard_project_setup(REQUIRES 6.7)

# Atoll keeps its QML in views/ and components/ subdirectories but exposes them
# as one flat "Atoll" module, which is the pre-QTP0004 behaviour.
if(POLICY CMP0000)
    qt_policy(SET QTP0004 OLD)
endif()
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

qt_add_executable(atoll
    src/main.cpp

    src/app/application.cpp        src/app/application.h
    src/app/shellwindow.cpp        src/app/shellwindow.h
    src/app/imagestore.cpp         src/app/imagestore.h

    src/ai/aiprovider.cpp          src/ai/aiprovider.h
    src/ai/aiservice.cpp           src/ai/aiservice.h
    src/ai/aitools.cpp             src/ai/aitools.h
    src/ai/aitypes.h
    src/ai/anthropicprovider.cpp   src/ai/anthropicprovider.h
    src/ai/claudecliprovider.cpp   src/ai/claudecliprovider.h
    src/ai/commandrunner.cpp       src/ai/commandrunner.h
    src/ai/credentialstore.cpp     src/ai/credentialstore.h
    src/ai/geminiprovider.cpp      src/ai/geminiprovider.h
    src/ai/permissionbroker.cpp    src/ai/permissionbroker.h
    src/ai/permissionhook.cpp      src/ai/permissionhook.h
    src/ai/screencapture.cpp       src/ai/screencapture.h

    src/config/config.cpp          src/config/config.h

    src/dbus/dbusmonitor.cpp       src/dbus/dbusmonitor.h
    src/dbus/dbusvariant.cpp       src/dbus/dbusvariant.h
    src/dbus/osdmonitor.cpp        src/dbus/osdmonitor.h
    src/dbus/notificationmonitor.cpp src/dbus/notificationmonitor.h
    src/dbus/notificationmodel.cpp src/dbus/notificationmodel.h
    src/dbus/mprisplayer.cpp       src/dbus/mprisplayer.h
    src/dbus/mprismanager.cpp      src/dbus/mprismanager.h

    src/system/battery.cpp         src/system/battery.h
    src/system/clock.cpp           src/system/clock.h
    src/system/lockmonitor.cpp     src/system/lockmonitor.h
    src/system/visualizer.cpp      src/system/visualizer.h

    src/calendar/calendarservice.cpp src/calendar/calendarservice.h

    src/ipc/ipcservice.cpp         src/ipc/ipcservice.h

    src/media/lyricsservice.cpp    src/media/lyricsservice.h

    src/share/sharecredentials.cpp src/share/sharecredentials.h
    src/share/sharediscovery.cpp   src/share/sharediscovery.h
    src/share/shareserver.cpp      src/share/shareserver.h
    src/share/sharesender.cpp      src/share/sharesender.h
    src/share/shareservice.cpp     src/share/shareservice.h
    src/share/sharetypes.h
)

if(ATOLL_LOCKSCREEN_OVERLAY)
    set(ATOLL_PROTOCOL ${CMAKE_CURRENT_SOURCE_DIR}/protocols/kde-lockscreen-overlay-v1.xml)
    set(ATOLL_PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/kde-lockscreen-overlay-v1-client-protocol.h)
    set(ATOLL_PROTOCOL_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/kde-lockscreen-overlay-v1-protocol.c)

    add_custom_command(
        OUTPUT ${ATOLL_PROTOCOL_HEADER}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${ATOLL_PROTOCOL} ${ATOLL_PROTOCOL_HEADER}
        DEPENDS ${ATOLL_PROTOCOL}
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${ATOLL_PROTOCOL_SOURCE}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${ATOLL_PROTOCOL} ${ATOLL_PROTOCOL_SOURCE}
        DEPENDS ${ATOLL_PROTOCOL}
        VERBATIM
    )

    target_sources(atoll PRIVATE
        ${ATOLL_PROTOCOL_HEADER}
        ${ATOLL_PROTOCOL_SOURCE}
        src/wayland/lockscreenoverlay.cpp  src/wayland/lockscreenoverlay.h
    )
    target_compile_definitions(atoll PRIVATE ATOLL_LOCKSCREEN_OVERLAY)
    target_link_libraries(atoll PRIVATE PkgConfig::Wayland Qt6::GuiPrivate)
endif()

# The generated QML type registration includes headers by base name, so every
# directory holding a QML-exposed type has to be reachable directly.
target_include_directories(atoll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/calendar
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbus
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/media
    ${CMAKE_CURRENT_SOURCE_DIR}/src/share
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wayland
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(atoll PRIVATE
    ATOLL_VERSION="${PROJECT_VERSION}"
)

# Theme and Cfg are singletons; Qt needs that recorded on the files themselves.
set_source_files_properties(qml/Theme.qml qml/Cfg.qml qml/settings/Skin.qml
                            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(atoll
    URI Atoll
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/IslandWindow.qml
        qml/Island.qml
        qml/Theme.qml
        qml/Cfg.qml
        qml/Stage.qml
        qml/views/IdleView.qml
        qml/views/OsdView.qml
        qml/views/NotificationView.qml
        qml/views/MediaView.qml
        qml/views/ExpandedView.qml
        qml/views/CallToActionView.qml
        qml/views/LyricsView.qml
        qml/views/ShareView.qml
        qml/views/AiView.qml
        qml/AiGlowWindow.qml
        qml/components/Blob.qml
        qml/components/GooLayer.qml
        qml/components/IconImage.qml
        qml/components/Marquee.qml
        qml/components/LevelBar.qml
        qml/components/ProgressRing.qml
        qml/components/Spectrum.qml
        qml/components/AlbumArt.qml
        qml/components/RoundButton.qml
        qml/components/QuickToggle.qml
        qml/components/PillButton.qml
        qml/components/Ripple.qml
        qml/components/BloubBot.qml
        qml/components/EdgeGlow.qml
        qml/settings/SettingsWindow.qml
        qml/settings/Skin.qml
        qml/settings/SettingsPage.qml
        qml/settings/SettingsGroup.qml
        qml/settings/SettingRow.qml
        qml/settings/BoolSetting.qml
        qml/settings/ChoiceSetting.qml
        qml/settings/ColorSetting.qml
        qml/settings/ListSetting.qml
        qml/settings/NumberSetting.qml
        qml/settings/SliderSetting.qml
        qml/settings/TextSetting.qml
        qml/settings/ColorField.qml
        qml/settings/InputField.qml
        qml/settings/NumberField.qml
        qml/settings/PositionPicker.qml
        qml/settings/PushButton.qml
        qml/settings/ScreenPicker.qml
        qml/settings/Segmented.qml
        qml/settings/SliderControl.qml
        qml/settings/StepButton.qml
        qml/settings/ToggleSwitch.qml
        qml/settings/PlacementPage.qml
        qml/settings/AppearancePage.qml
        qml/settings/ContentPage.qml
        qml/settings/MediaPage.qml
        qml/settings/NotificationsPage.qml
        qml/settings/BehaviorPage.qml
        qml/settings/SharingPage.qml
        qml/settings/AiPage.qml
        qml/settings/CalendarPage.qml
        qml/settings/SecretField.qml
        qml/settings/AboutPage.qml
    RESOURCES
        data/icons/atoll.svg
)

qt6_add_shaders(atoll "atoll_shaders"
    BATCHABLE
    PREFIX "/qt/qml/Atoll/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    FILES
        shaders/gooey.frag
        shaders/edgeglow.frag
)

target_link_libraries(atoll PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::DBus
    Qt6::Network
    Qt6::Svg
    KF6::CoreAddons
    KF6::I18n
    LayerShellQtInterface
    PkgConfig::DBus1
)

install(TARGETS atoll RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# The desktop file carries the absolute path of the installed binary: KWin
# matches it against the running process to decide whether Atoll may ask to
# stay up on the lock screen.
set(ATOLL_BINARY ${CMAKE_INSTALL_FULL_BINDIR}/atoll)
configure_file(data/io.github.atoll.Atoll.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/io.github.atoll.Atoll.desktop @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.atoll.Atoll.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES data/icons/atoll.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME io.github.atoll.Atoll.svg)
install(FILES packaging/atoll.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user)
install(PROGRAMS packaging/atollctl
        DESTINATION ${CMAKE_INSTALL_BINDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
