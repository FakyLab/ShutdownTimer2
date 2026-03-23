cmake_minimum_required(VERSION 3.20)
project(ShutdownTimer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS minimum deployment target — must be set before find_package(Qt6)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

# Qt6 auto-tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools)
# ── Qt resource file (icons for tray / window) ────────────────────────────────
set(QT_RESOURCES resources.qrc)

# ── Common sources (platform-independent) ─────────────────────────────────────
set(SOURCES_COMMON
    src/app/main.cpp
    src/app/SingleInstanceGuard.cpp

    # Core
    src/core/TimerEngine.cpp
    src/core/LanguageManager.cpp

    # Models
    src/models/TimerModel.cpp
    src/models/MessageModel.cpp
    src/models/AppSettingsModel.cpp

    # Controllers
    src/controllers/TimerController.cpp
    src/controllers/MessageController.cpp
    src/controllers/SettingsController.cpp

    # Views
    src/views/TimerView.cpp
    src/views/MessageView.cpp
    src/views/MainWindow.cpp

    # Services
    src/services/PlatformServiceFactory.cpp
)

set(HEADERS_COMMON
    src/app/SingleInstanceGuard.h
    src/core/TimerEngine.h
    src/core/LanguageManager.h
    src/models/TimerModel.h
    src/models/MessageModel.h
    src/models/AppSettingsModel.h
    src/controllers/TimerController.h
    src/controllers/MessageController.h
    src/controllers/SettingsController.h
    src/views/TimerView.h
    src/views/MessageView.h
    src/views/MainWindow.h
    src/services/interfaces/IShutdownBackend.h
    src/services/interfaces/IMessageBackend.h
    src/services/interfaces/IAutoClearBackend.h
    src/services/PlatformServiceFactory.h
)

# ── Platform-specific sources ─────────────────────────────────────────────────
if(WIN32)
    enable_language(RC)

    set(SOURCES_PLATFORM
        src/services/platform/windows/ShutdownBackendWindows.cpp
        src/services/platform/windows/MessageBackendWindows.cpp
        src/services/platform/windows/AutoClearBackendWindows.cpp
    )
    set(HEADERS_PLATFORM
        src/services/platform/windows/ShutdownBackendWindows.h
        src/services/platform/windows/MessageBackendWindows.h
        src/services/platform/windows/AutoClearBackendWindows.h
    )
    set(WIN_RESOURCES resources.rc)

elseif(UNIX AND NOT APPLE)
    set(SOURCES_PLATFORM
        src/services/platform/linux/ShutdownBackendLinux.cpp
        src/services/platform/linux/MessageBackendLinux.cpp
        src/services/platform/linux/AutoClearBackendLinux.cpp
    )
    set(HEADERS_PLATFORM
        src/services/platform/linux/ShutdownBackendLinux.h
        src/services/platform/linux/MessageBackendLinux.h
        src/services/platform/linux/AutoClearBackendLinux.h
    )

elseif(APPLE)
    set(SOURCES_PLATFORM
        src/services/platform/macos/ShutdownBackendMacOS.cpp
        src/services/platform/macos/MessageBackendMacOS.cpp
        src/services/platform/macos/AutoClearBackendMacOS.cpp
        src/services/platform/macos/MacOSNotifier.mm
    )
    set(HEADERS_PLATFORM
        src/services/platform/macos/ShutdownBackendMacOS.h
        src/services/platform/macos/MessageBackendMacOS.h
        src/services/platform/macos/AutoClearBackendMacOS.h
        src/services/platform/macos/MacOSNotifier.h
    )
endif()

# ── Executable ────────────────────────────────────────────────────────────────
# qt_add_executable is Qt6's recommended way to create executables.
# It automatically sets WIN32 on Windows and MACOSX_BUNDLE on macOS,
# and handles Qt6EntryPoint linkage correctly for all toolchains.
if(WIN32)
    qt_add_executable(ShutdownTimer
        ${SOURCES_COMMON} ${HEADERS_COMMON}
        ${SOURCES_PLATFORM} ${HEADERS_PLATFORM}
        ${WIN_RESOURCES}
        ${QT_RESOURCES}
    )
else()
    qt_add_executable(ShutdownTimer
        ${SOURCES_COMMON} ${HEADERS_COMMON}
        ${SOURCES_PLATFORM} ${HEADERS_PLATFORM}
        ${QT_RESOURCES}
    )
endif()

if(WIN32)
    # Suppress console window
    set_target_properties(ShutdownTimer PROPERTIES WIN32_EXECUTABLE TRUE)
    # We embed our own manifest via resources.rc — tell MSVC not to generate one
    # to avoid "duplicate resource MANIFEST" linker error
    if(MSVC)
        target_link_options(ShutdownTimer PRIVATE /MANIFEST:NO)
    endif()
endif()

if(APPLE)
    # Create a proper .app bundle on macOS with Info.plist
    set_target_properties(ShutdownTimer PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Shutdown Timer"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.fakylab.ShutdownTimer"
        MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "12.0"
    )
endif()

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(ShutdownTimer PRIVATE src)

# ── Translations ──────────────────────────────────────────────────────────────
# Use Qt6's own lrelease executable, resolved automatically by LinguistTools.
# This avoids the Qt5/Qt6 naming conflict on Linux (lrelease vs lrelease6)
# by letting Qt's CMake integration point to the correct binary directly.
get_target_property(LRELEASE_EXECUTABLE Qt6::lrelease IMPORTED_LOCATION)

set(TS_FILES
    i18n/app_ar.ts   i18n/app_ko.ts
    i18n/app_es.ts   i18n/app_fr.ts   i18n/app_de.ts
    i18n/app_pt_BR.ts i18n/app_zh_CN.ts i18n/app_ja.ts
)

foreach(TS_FILE ${TS_FILES})
    get_filename_component(QM_BASE ${TS_FILE} NAME_WE)
    set(QM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/i18n/${QM_BASE}.qm")
    add_custom_command(
        OUTPUT  ${QM_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/i18n"
        COMMAND "${LRELEASE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${TS_FILE}"
                -qm "${QM_OUTPUT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${TS_FILE}"
        COMMENT "Compiling ${TS_FILE} -> ${QM_OUTPUT}"
    )
    list(APPEND QM_FILES ${QM_OUTPUT})
endforeach()

add_custom_target(Translations ALL DEPENDS ${QM_FILES})
add_dependencies(ShutdownTimer Translations)

add_custom_command(TARGET ShutdownTimer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/i18n"
        "$<TARGET_FILE_DIR:ShutdownTimer>/i18n"
    COMMENT "Copying translations to output directory"
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(ShutdownTimer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
)

if(WIN32)
    target_link_libraries(ShutdownTimer PRIVATE
        advapi32 user32 shell32 ole32 oleaut32 taskschd PowrProf
    )
    target_compile_definitions(ShutdownTimer PRIVATE
        UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00
    )

endif()

if(APPLE)
    target_link_libraries(ShutdownTimer PRIVATE
        "-framework CoreServices"
        "-framework UserNotifications"
        "-framework Foundation"
    )
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(ShutdownTimer PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ── Post-build message ────────────────────────────────────────────────────────
add_custom_command(TARGET ShutdownTimer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
        "Build complete: $<TARGET_FILE:ShutdownTimer>"
)

# ── Install rules (Linux) ─────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    install(TARGETS ShutdownTimer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Translations
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/i18n/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/shutdowntimer/i18n
        FILES_MATCHING PATTERN "*.qm")

    # Icons
    foreach(SIZE 16 24 32 48 64 128 256)
        install(FILES assets/shutdown${SIZE}px.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps
            RENAME org.fakylab.ShutdownTimer.png)
    endforeach()

    # Desktop file
    install(FILES packaging/appimage/shutdowntimer.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        RENAME org.fakylab.ShutdownTimer.desktop)

    # AppStream metadata
    install(FILES linux/appstream/org.fakylab.ShutdownTimer.appdata.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

    # Man page
    install(FILES packaging/debian/shutdowntimer.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()
