# Linux packaging (Phase 5 §3, ROADMAP_4 Step 30; DECISIONS_4 #44). CPack DEB
# built from the desktop-integration component ALONE — the same files that
# `cmake --install build --component desktop-integration --prefix …` puts in a
# prefix, with the prefix fixed to /usr so the .desktop entry's Exec is the
# absolute /usr/bin/midi-sink (Step 14: GIO silently drops a desktop entry
# whose Exec binary is not in PATH; the install-time configure_file in
# desktop/CMakeLists.txt is what writes it).
#
#   cmake --build build && (cd build && cpack -G DEB)
#     -> build/midi-sink_<version>_amd64.deb
#
# The release TARBALL is deliberately not CPack: the lane tars the bare binary
# plus LICENSE like the Windows portable zip, because a .desktop file with an
# absolute Exec is wrong at whatever prefix a tarball user unpacks to — desktop
# integration is the .deb's job (or `cmake --install … --component`).
#
# Version mapping. A Debian version may not contain '-' without a debian
# revision, and "0.5.0-rc.5" has to sort BEFORE "0.5.0" so the release
# supersedes its candidates: every '-' becomes '~' in the PACKAGE version
# (0.5.0~rc.5; a dev describe 0.5.0-rc.4-2-gabc-dirty becomes
# 0.5.0~rc.4~2~gabc~dirty — still valid, still below the release). The FILE
# keeps the lane interface's midi-sink_<version>_amd64.deb form, so the asset
# name on the release page reads the tag as every other lane's does.
if(NOT SUMI_APP_VERSION)
    message(FATAL_ERROR "LinuxPackaging: SUMI_APP_VERSION must be set before this file")
endif()
string(REGEX REPLACE "-" "~" SUMI_DEB_VERSION "${SUMI_APP_VERSION}")

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "midi-sink")
set(CPACK_PACKAGE_VENDOR "Vibetuned")
set(CPACK_PACKAGE_CONTACT "Pedro Fillastre <info@vibetuned.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://midi-sink.vibetuned.com/")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Suminagashi ink-marbling visualizer driven by expressive MIDI")
set(CPACK_PACKAGE_DESCRIPTION
"midi-sink models a tray of water with ink floating on it. Every MIDI
 gesture is an exact, area-preserving transformation of the whole sheet
 (Aubrey Jaffer's mathematical marbling), so the rings stay crisp after a
 thousand strokes. Plug in an MPE controller (ROLI, Osmose), a wind
 controller or any keyboard over ALSA and it paints; the mouse plays the
 same operators directly. It makes no sound of its own.")
set(CPACK_PACKAGE_VERSION "${SUMI_DEB_VERSION}")
set(CPACK_DEBIAN_PACKAGE_VERSION "${SUMI_DEB_VERSION}")
set(CPACK_DEBIAN_FILE_NAME "midi-sink_${SUMI_APP_VERSION}_amd64.deb")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
# dpkg-shlibdeps reads the ELF: libc6, libstdc++6, libgcc-s1, libopengl0…
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# …but GLFW and libremidi load their platform libraries at RUNTIME (dlopen),
# invisible to shlibdeps, so they are listed by hand. Both windowing systems
# are present because one binary serves X11 and Wayland sessions; the 24.04
# time64 rename of libasound2 is an alternative so a single package installs
# on 22.04 and 24.04 alike.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libasound2t64 | libasound2, libgl1, libegl1, libxkbcommon0, libwayland-client0, libwayland-cursor0, libwayland-egl1, libdecor-0-0, libx11-6, libx11-xcb1, libxcursor1, libxext6, libxi6, libxinerama1, libxrandr2, libxrender1, libxxf86vm1")
# No maintainer scripts: dpkg's file triggers (desktop-file-utils,
# hicolor-icon-theme's gtk-update-icon-cache) refresh the XDG caches on install
# and removal. The manual-install refresh in desktop/CMakeLists.txt skips
# itself while packaging — it would otherwise bake a stale icon-theme.cache
# INTO the package, the exact failure the Step-14 note warns about.
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
# COMPONENT mode, one component, grouped into ONE package. A monolithic CPack
# install ignores CPACK_COMPONENTS_ALL and sweeps in every FetchContent
# dependency's own install rules (GLFW/libremidi headers, static libs, CMake
# configs — ~190 files; measured on the first attempt); component mode with
# ALL_COMPONENTS_IN_ONE packages exactly the desktop-integration files under
# the top-level package name and file name.
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL desktop-integration)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGE_FILE_NAME "midi-sink-${SUMI_APP_VERSION}-linux-x64")
# lintian polish (advisory in the lane, but cheap to satisfy). A native
# package (no debian revision) must ship /usr/share/doc/midi-sink/changelog.gz:
# one entry per build, generated from the version — the user-facing notes are
# the site's changelog page. And the staged tree must carry 0755 directories /
# 0644 data files whatever the packaging host's umask is (CPack stages through
# install(CODE) too, which follows the umask): a pre-build script normalizes.
string(TIMESTAMP _sumi_deb_date "%a, %d %b %Y %H:%M:%S +0000" UTC)
file(WRITE "${CMAKE_BINARY_DIR}/deb/changelog"
"midi-sink (${SUMI_DEB_VERSION}) unstable; urgency=medium

  * midi-sink ${SUMI_APP_VERSION} — release notes:
    https://midi-sink.vibetuned.com/notes/changelog/

 -- Pedro Fillastre <info@vibetuned.com>  ${_sumi_deb_date}
")
file(ARCHIVE_CREATE OUTPUT "${CMAKE_BINARY_DIR}/deb/changelog.gz"
     PATHS "${CMAKE_BINARY_DIR}/deb/changelog" FORMAT raw COMPRESSION GZip COMPRESSION_LEVEL 9)
install(FILES "${CMAKE_BINARY_DIR}/deb/changelog.gz"
        DESTINATION share/doc/midi-sink COMPONENT desktop-integration)
file(WRITE "${CMAKE_BINARY_DIR}/deb/normalize_perms.cmake"
"file(GLOB_RECURSE _dirs LIST_DIRECTORIES true \"\${CPACK_TEMPORARY_INSTALL_DIRECTORY}/*\")
foreach(_p IN LISTS _dirs)
    if(IS_DIRECTORY \"\${_p}\")
        file(CHMOD \"\${_p}\" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    elseif(_p MATCHES \"/bin/\")
        file(CHMOD \"\${_p}\" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    else()
        file(CHMOD \"\${_p}\" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
endforeach()
")
set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/deb/normalize_perms.cmake")

include(CPack)
