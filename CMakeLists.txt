cmake_minimum_required(VERSION 3.24)

project(atoll
    VERSION 0.3.0
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

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Multimedia Gui GuiPrivate Qml Quick QuickControls2 DBus Svg Network ShaderTools)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons I18n)
find_package(LayerShellQt REQUIRED)

# Global shortcuts for the island's actions (docs/controls.md). Without
# KGlobalAccel the settings window hides the section and `atollctl action`
# is the way to bind keys.
find_package(KF6GlobalAccel 6.0 QUIET)
add_feature_info(GlobalShortcuts KF6GlobalAccel_FOUND "system-wide key combinations for the island's actions")

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

# Raising the application behind a clicked notification is an xdg-activation
# token away: Atoll mints one and hands it to whatever it launches or asks to
# activate, so the compositor lets that window come forward.
find_package(Qt6 QUIET COMPONENTS WaylandClient)
if(ATOLL_LOCKSCREEN_OVERLAY AND TARGET Qt6::WaylandClient)
    set(ATOLL_XDG_ACTIVATION ON)
else()
    message(STATUS "wayland-scanner or Qt6::WaylandClient missing: clicking a notification will not be able to hand new windows focus")
endif()
add_feature_info(NotificationActivation ATOLL_XDG_ACTIVATION "focus the app behind a clicked notification")

# The privacy dots read the PipeWire graph - which application is capturing
# from the camera or microphone, and which is receiving your screen. Without
# libpipewire everything else still builds; the indicators simply stay dark.
pkg_check_modules(PipeWire IMPORTED_TARGET libpipewire-0.3)
add_feature_info(PrivacyIndicators PipeWire_FOUND "camera, microphone and screen-share indicators on the island")

qt_standard_project_setup(REQUIRES 6.7)

# Atoll keeps its QML in views/ and components/ subdirectories but exposes them
# as one flat "Atoll" module, which is the pre-QTP0004 behaviour.
if(POLICY CMP0000)
    qt_policy(SET QTP0004 OLD)
endif()
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

qt_add_executable(atoll
    src/main.cpp
    src/focus/focusservice.cpp src/focus/focusservice.h

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
    src/ai/openrouterprovider.cpp  src/ai/openrouterprovider.h
    src/ai/permissionbroker.cpp    src/ai/permissionbroker.h
    src/ai/permissionhook.cpp      src/ai/permissionhook.h
    src/ai/screencapture.cpp       src/ai/screencapture.h

    src/config/config.cpp          src/config/config.h

    src/office/claudeask.cpp       src/office/claudeask.h
    src/office/claudehook.cpp      src/office/claudehook.h
    src/office/officeevent.cpp     src/office/officeevent.h
    src/office/officeservice.cpp   src/office/officeservice.h
    src/office/systemmonitor.cpp   src/office/systemmonitor.h

    src/dbus/dbusmonitor.cpp       src/dbus/dbusmonitor.h
    src/dbus/dbusvariant.cpp       src/dbus/dbusvariant.h
    src/dbus/osdmonitor.cpp        src/dbus/osdmonitor.h
    src/dbus/notificationmonitor.cpp src/dbus/notificationmonitor.h
    src/dbus/notificationmodel.cpp src/dbus/notificationmodel.h
    src/dbus/mprisplayer.cpp       src/dbus/mprisplayer.h
    src/dbus/mprismanager.cpp      src/dbus/mprismanager.h

    src/system/appactivator.cpp       src/system/appactivator.h
    src/system/battery.cpp         src/system/battery.h
    src/system/bluetooth.cpp       src/system/bluetooth.h
    src/system/clock.cpp           src/system/clock.h
    src/system/lockmonitor.cpp     src/system/lockmonitor.h
    src/system/privacymonitor.cpp  src/system/privacymonitor.h
    src/system/visualizer.cpp      src/system/visualizer.h

    src/calendar/calendarservice.cpp src/calendar/calendarservice.h

    src/input/shortcutservice.cpp  src/input/shortcutservice.h

    src/ipc/ipcservice.cpp         src/ipc/ipcservice.h

    src/media/lyricsservice.cpp    src/media/lyricsservice.h
    src/media/audiooutputs.cpp     src/media/audiooutputs.h

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

if(ATOLL_XDG_ACTIVATION)
    set(ATOLL_ACTIVATION_PROTOCOL ${CMAKE_CURRENT_SOURCE_DIR}/protocols/xdg-activation-v1.xml)
    set(ATOLL_ACTIVATION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/xdg-activation-v1-client-protocol.h)
    set(ATOLL_ACTIVATION_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/xdg-activation-v1-protocol.c)

    add_custom_command(
        OUTPUT ${ATOLL_ACTIVATION_HEADER}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${ATOLL_ACTIVATION_PROTOCOL} ${ATOLL_ACTIVATION_HEADER}
        DEPENDS ${ATOLL_ACTIVATION_PROTOCOL}
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${ATOLL_ACTIVATION_SOURCE}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${ATOLL_ACTIVATION_PROTOCOL} ${ATOLL_ACTIVATION_SOURCE}
        DEPENDS ${ATOLL_ACTIVATION_PROTOCOL}
        VERBATIM
    )

    target_sources(atoll PRIVATE
        ${ATOLL_ACTIVATION_HEADER}
        ${ATOLL_ACTIVATION_SOURCE}
    )
    target_compile_definitions(atoll PRIVATE ATOLL_XDG_ACTIVATION)
    target_link_libraries(atoll PRIVATE PkgConfig::Wayland Qt6::WaylandClient)
endif()

if(PipeWire_FOUND)
    target_compile_definitions(atoll PRIVATE ATOLL_PIPEWIRE)
endif()

if(KF6GlobalAccel_FOUND)
    target_compile_definitions(atoll PRIVATE ATOLL_HAVE_GLOBALACCEL)
    target_link_libraries(atoll PRIVATE KF6::GlobalAccel)
endif()

# The generated QML type registration includes headers by base name, so every
# directory holding a QML-exposed type has to be reachable directly.
target_include_directories(atoll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/calendar
    ${CMAKE_CURRENT_SOURCE_DIR}/src/focus
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dbus
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/media
    ${CMAKE_CURRENT_SOURCE_DIR}/src/office
    ${CMAKE_CURRENT_SOURCE_DIR}/src/share
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system
    ${CMAKE_CURRENT_SOURCE_DIR}/src/wayland
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(atoll PRIVATE
    ATOLL_VERSION="${PROJECT_VERSION}"
)

# Theme and Cfg are singletons; Qt needs that recorded on the files themselves.
set_source_files_properties(qml/Theme.qml qml/Cfg.qml qml/Actions.qml qml/settings/Skin.qml qml/office/OfficeArt.qml
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
        qml/Actions.qml
        qml/Stage.qml
        qml/views/FocusView.qml
        qml/settings/FocusPage.qml
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
        qml/office/OfficeArt.qml
        qml/office/OfficeView.qml
        qml/office/OfficeNamePlate.qml
        qml/office/OfficeHeader.qml
        qml/office/OfficeProp.qml
        qml/office/OfficeAgent.qml
        qml/office/OfficeCat.qml
        qml/office/Sprite.qml
        qml/office/FanSprite.qml
        qml/office/NineSlice.qml
        qml/office/PixelText.qml
        qml/office/PixelButton.qml
        qml/office/NameTag.qml
        qml/office/DustMotes.qml
        qml/office/EmptySign.qml
        qml/office/MonitorZoom.qml
        qml/office/QuestionPanel.qml
        qml/office/SystemPanel.qml
        qml/office/SystemHud.qml
        qml/components/Blob.qml
        qml/components/GooLayer.qml
        qml/components/IconImage.qml
        qml/components/Marquee.qml
        qml/components/LevelBar.qml
        qml/components/ProgressRing.qml
        qml/components/Spectrum.qml
        qml/components/FocusTile.qml
        qml/components/TransportButton.qml
        qml/views/MediaHoverView.qml
        qml/components/AudioOutputPicker.qml
        qml/components/AlbumArt.qml
        qml/components/RoundButton.qml
        qml/components/QuickToggle.qml
        qml/components/PillButton.qml
        qml/components/Ripple.qml
        qml/components/BloubBot.qml
        qml/components/EdgeGlow.qml
        qml/components/WeatherWidget.qml
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
        qml/settings/ControlsPage.qml
        qml/settings/OfficePage.qml
        qml/settings/IconTile.qml
        qml/settings/MenuSelect.qml
        qml/settings/ShortcutField.qml
        qml/settings/TokenToggles.qml
        qml/settings/SharingPage.qml
        qml/settings/AiPage.qml
        qml/settings/CalendarPage.qml
        qml/settings/SecretField.qml
        qml/settings/AboutPage.qml
    RESOURCES
        data/icons/atoll.svg
        data/icons/atoll-mark.svg
        data/icons/settings/placement.svg
        data/icons/settings/appearance.svg
        data/icons/settings/content.svg
        data/icons/settings/media.svg
        data/icons/settings/notifications.svg
        data/icons/settings/calendar.svg
        data/icons/settings/focus.svg
        data/icons/settings/controls.svg
        data/icons/settings/assistant.svg
        data/icons/settings/office.svg
        data/icons/settings/sharing.svg
        data/icons/settings/about.svg
        data/icons/settings/chevrons.svg
        data/icons/settings/search.svg
        data/icons/settings/check.svg
        data/icons/settings/clear.svg
        data/office/armchair.png
        data/office/bookshelf.png
        data/office/bubble.png
        data/office/bubble_tail.png
        data/office/carpet_work.png
        data/office/cat.png
        data/office/cat_bed.png
        data/office/chair_back.png
        data/office/chair_front.png
        data/office/characters.png
        data/office/clock_face.png
        data/office/clock_hour.png
        data/office/clock_minute.png
        data/office/clutter.png
        data/office/coat_rack.png
        data/office/coffee_machine.png
        data/office/coffee_table.png
        data/office/counter.png
        data/office/desk.png
        data/office/door_glow.png
        data/office/door_lamp.png
        data/office/door_spill.png
        data/office/dust.png
        data/office/emote.png
        data/office/exit_sign.png
        data/office/fan.png
        data/office/floor_lamp.png
        data/office/fridge.png
        data/office/halo_warm.png
        data/office/icons.png
        data/office/julius_door.png
        data/office/kanban.png
        data/office/layout.json
        data/office/lead_chair.png
        data/office/lead_desk.png
        data/office/manifest.json
        data/office/mat_entrance.png
        data/office/mat_julius.png
        data/office/monitor_back.png
        data/office/monitor_big.png
        data/office/monitor_big_glass.png
        data/office/monitor_front.png
        data/office/monitor_glow.png
        data/office/net_cabinet.png
        data/office/painting.png
        data/office/plant_cactus.png
        data/office/plant_ficus.png
        data/office/plant_hanging.png
        data/office/plant_monstera.png
        data/office/plant_snake.png
        data/office/pool_small.png
        data/office/pool_warm.png
        data/office/poster.png
        data/office/printer.png
        data/office/rack_glow.png
        data/office/record_player.png
        data/office/room.png
        data/office/rug_lead.png
        data/office/rug_lounge.png
        data/office/server_rack.png
        data/office/sh_chair.png
        data/office/sh_counter.png
        data/office/sh_desk_pod.png
        data/office/sh_fridge.png
        data/office/sh_lead_desk.png
        data/office/sh_medium.png
        data/office/sh_plant.png
        data/office/sh_rack.png
        data/office/sh_shelf.png
        data/office/sh_small.png
        data/office/sh_sofa.png
        data/office/sh_table.png
        data/office/shadow_cat.png
        data/office/shadow_person.png
        data/office/sofa.png
        data/office/steam.png
        data/office/strip_glow.png
        data/office/sysroom.png
        data/office/trash_bin.png
        data/office/ui_bar_bg.png
        data/office/ui_bar_fill.png
        data/office/ui_button.png
        data/office/ui_button_accent.png
        data/office/ui_caret.png
        data/office/ui_check.png
        data/office/ui_led.png
        data/office/ui_led_dot.png
        data/office/ui_panel_dark.png
        data/office/ui_panel_light.png
        data/office/ui_row_highlight.png
        data/office/ui_scroll_thumb.png
        data/office/ui_scroll_track.png
        data/office/ui_tab.png
        data/office/ui_tag.png
        data/office/ui_textfield.png
        data/office/umbrella_stand.png
        data/office/upper_cabinets.png
        data/office/vignette.png
        data/office/wall_shelf.png
        data/office/water_cooler.png
        data/office/whiteboard.png
        data/office/window_beam.png
        data/office/window_frame.png
        data/office/window_sky.png
        data/fonts/monogram-extended.ttf
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
    Qt6::Multimedia
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
    $<TARGET_NAME_IF_EXISTS:PkgConfig::PipeWire>
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
install(DIRECTORY data/kwin/scripts/atoll-dodge
        DESTINATION ${CMAKE_INSTALL_DATADIR}/kwin/scripts)
install(PROGRAMS scripts/claude-office-hooks.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME atoll-claude-hooks)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(atoll-audio-test tests/audiooutputstest.cpp
        src/media/audiooutputs.cpp src/media/audiooutputs.h
        src/config/config.cpp src/config/config.h)
    target_include_directories(atoll-audio-test PRIVATE src)
    target_link_libraries(atoll-audio-test PRIVATE Qt6::Core Qt6::Qml Qt6::DBus Qt6::Test)
    add_test(NAME audio-outputs COMMAND atoll-audio-test)
    qt_add_executable(atoll-focus-test tests/focustest.cpp
        src/focus/focusservice.cpp src/focus/focusservice.h
        src/config/config.cpp src/config/config.h)
    target_include_directories(atoll-focus-test PRIVATE src)
    target_link_libraries(atoll-focus-test PRIVATE Qt6::Core Qt6::Qml Qt6::Multimedia Qt6::Test)
    add_test(NAME focus-lifecycle COMMAND atoll-focus-test)

    qt_add_executable(atoll-office-test tests/officetest.cpp
        src/office/claudeask.cpp      src/office/claudeask.h
        src/office/officeevent.cpp    src/office/officeevent.h
        src/office/officeservice.cpp  src/office/officeservice.h
        src/office/systemmonitor.cpp  src/office/systemmonitor.h
        src/config/config.cpp         src/config/config.h)
    target_include_directories(atoll-office-test PRIVATE src)
    target_link_libraries(atoll-office-test PRIVATE Qt6::Core Qt6::Qml Qt6::Network Qt6::Test)
    add_test(NAME office-model COMMAND atoll-office-test)

    qt_add_executable(atoll-config-test tests/configtest.cpp
        src/config/config.cpp src/config/config.h)
    target_include_directories(atoll-config-test PRIVATE src)
    target_link_libraries(atoll-config-test PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
    add_test(NAME config-migration COMMAND atoll-config-test)
endif()
