# Desktop harness — links the static core (the shared lib is exercised by
# tests/abi_c_compile, so both artifacts get consumed).
set(SUMI_DESKTOP_SOURCES src/main.cpp src/midi_harness.cpp)
if(APPLE)
    list(APPEND SUMI_DESKTOP_SOURCES src/metal_layer_glue.mm)
elseif(WIN32)
    # Gives midi-sink.exe its Explorer/taskbar icon (the live window icon is
    # set at runtime via glfwSetWindowIcon on both Windows and X11).
    list(APPEND SUMI_DESKTOP_SOURCES midi-sink.rc)
endif()

# The app is midi-sink; only the core library carries the sumi name.
add_executable(midi-sink ${SUMI_DESKTOP_SOURCES})
target_link_libraries(midi-sink PRIVATE sumi_static glfw libremidi stb_headers)
# The --field-dump test hooks (sumi_debug.h) are internal, static-link-only —
# the harness reaches them through the core's src dir, same as the unit tests.
target_include_directories(midi-sink PRIVATE ${CMAKE_SOURCE_DIR}/core/src)

if(APPLE)
    set_source_files_properties(src/metal_layer_glue.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(midi-sink PRIVATE "-framework Cocoa" "-framework QuartzCore")
endif()

if(UNIX AND NOT APPLE)
    # Desktop integration. GLFW cannot set a window icon on Wayland — the
    # compositor resolves the icon from the .desktop file matching the
    # window's app_id — so the desktop entry plus the XDG icon theme below
    # ARE the icon on that platform (and give the app a dock/app-grid entry
    # on X11 too). Install with:
    #
    #   cmake --install build --component desktop-integration --prefix ~/.local
    #
    # The COMPONENT is not optional bookkeeping: every FetchContent dependency
    # (GLFW, libremidi, glm, readerwriterqueue) carries its own install rules,
    # so a plain `cmake --install` also copies ~190 third-party headers,
    # static libs and CMake config files into the prefix. Those rules live in
    # the default "Unspecified" component, so naming ours keeps an install to
    # a real prefix down to exactly these nine files.
    include(GNUInstallDirs)
    install(TARGETS midi-sink
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT desktop-integration)
    # The desktop entry is CONFIGURED AT INSTALL TIME, not copied: Exec and
    # TryExec have to be the absolute installed path. GIO drops any entry whose
    # Exec binary is not in PATH, and gnome-shell's PATH has no ~/.local/bin —
    # with a bare "Exec=midi-sink" the shell never loads the file, so the window
    # cannot be matched and shows a generic icon (DECISIONS_2 #39c). Resolving
    # it here rather than at configure time keeps `--prefix` authoritative.
    # (Quoted, not a bracket argument: the SOURCE dir must expand now, while
    # CMAKE_INSTALL_PREFIX must stay unexpanded until install time.)
    install(CODE "
        set(SUMI_BINARY \"\${CMAKE_INSTALL_PREFIX}/bin/midi-sink\")
        set(_sumi_entry \"\${CMAKE_INSTALL_PREFIX}/share/applications/midi-sink.desktop\")
        message(STATUS \"Installing: \${_sumi_entry}\")
        configure_file(\"${CMAKE_SOURCE_DIR}/packaging/linux/midi-sink.desktop.in\"
                       \"\${_sumi_entry}\" @ONLY)
    " COMPONENT desktop-integration)
    foreach(icon_size 16 24 32 48 64 128 256)
        install(FILES
            ${CMAKE_SOURCE_DIR}/packaging/linux/icons/hicolor/${icon_size}x${icon_size}/apps/midi-sink.png
            DESTINATION
            ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${icon_size}x${icon_size}/apps
            COMPONENT desktop-integration)
    endforeach()

    # Refresh the XDG caches, because a STALE one silently wins. GTK treats an
    # icon-theme.cache in a theme directory as authoritative, so if some other
    # app created one there earlier (a Chrome PWA had, three months stale, in
    # this developer's ~/.local/share/icons/hicolor) a newly installed icon is
    # invisible to the running shell even though the PNG is on disk. Both tools
    # are best-effort: absent or failing, the install still succeeds.
    # --ignore-theme-index is required because a per-user hicolor directory has
    # no index.theme of its own (the theme is merged with the system one).
    install(CODE [[
        find_program(SUMI_GTK_ICON_CACHE gtk-update-icon-cache)
        if(SUMI_GTK_ICON_CACHE)
            execute_process(COMMAND ${SUMI_GTK_ICON_CACHE} --force --ignore-theme-index
                                    "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor"
                            RESULT_VARIABLE _sumi_r ERROR_VARIABLE _sumi_e)
            if(NOT _sumi_r EQUAL 0)
                message(STATUS "icon cache refresh skipped: ${_sumi_e}")
            endif()
        endif()
        find_program(SUMI_DESKTOP_DB update-desktop-database)
        if(SUMI_DESKTOP_DB)
            execute_process(COMMAND ${SUMI_DESKTOP_DB}
                                    "${CMAKE_INSTALL_PREFIX}/share/applications"
                            RESULT_VARIABLE _sumi_r ERROR_QUIET)
        endif()
    ]] COMPONENT desktop-integration)
endif()
