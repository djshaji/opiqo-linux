cmake_minimum_required(VERSION 3.21)
project(opiqo VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(RPMBUILD_EXECUTABLE rpmbuild)
find_program(FIND_EXECUTABLE find)

# Enable debug symbols
add_compile_options(-g)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)

message(STATUS "Using manual build_version file")

set(OPIQO_RPM_TOPDIR "${CMAKE_SOURCE_DIR}/packaging/rpm")
set(OPIQO_RPM_TARBALL "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}.tar.gz")
set(OPIQO_RPM_SPEC_SRC "${OPIQO_RPM_TOPDIR}/opiqo.spec")
set(OPIQO_RPM_SPEC_DST "${OPIQO_RPM_TOPDIR}/SPECS/opiqo.spec")

set(OPIQO_SRPM_PRUNE_COMMANDS)
if(FIND_EXECUTABLE)
  list(APPEND OPIQO_SRPM_PRUNE_COMMANDS
    COMMAND ${FIND_EXECUTABLE} "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}"
      -type f
      \(
        -name "*.o" -o
        -name "*.obj" -o
        -name "*.so" -o
        -name "*.a" -o
        -name "*.d"
      \)
      -delete
  )
else()
  message(WARNING "find utility not available; SRPM staging will not prune build artifacts")
endif()

if(RPMBUILD_EXECUTABLE)
  add_custom_target(srpm
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "${OPIQO_RPM_TOPDIR}/BUILD"
      "${OPIQO_RPM_TOPDIR}/BUILDROOT"
      "${OPIQO_RPM_TOPDIR}/RPMS"
      "${OPIQO_RPM_TOPDIR}/SOURCES"
      "${OPIQO_RPM_TOPDIR}/SPECS"
      "${OPIQO_RPM_TOPDIR}/SRPMS"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OPIQO_RPM_SPEC_SRC}" "${OPIQO_RPM_SPEC_DST}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/src" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/src"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/CMakeLists.txt" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/CMakeLists.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/CMakePresets.json" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/CMakePresets.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/README-gtk4.md" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/README-gtk4.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/Makefile" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/Makefile"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/build_version" "${OPIQO_RPM_TOPDIR}/SOURCES/${PROJECT_NAME}-${PROJECT_VERSION}/build_version"
    ${OPIQO_SRPM_PRUNE_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E tar cfvz "${OPIQO_RPM_TARBALL}"
      --format=gnutar
      -- "${PROJECT_NAME}-${PROJECT_VERSION}"
    COMMAND ${RPMBUILD_EXECUTABLE} -bs
      --define "_topdir ${OPIQO_RPM_TOPDIR}"
      "${OPIQO_RPM_SPEC_DST}"
    WORKING_DIRECTORY "${OPIQO_RPM_TOPDIR}/SOURCES"
    COMMENT "Building source RPM in ${OPIQO_RPM_TOPDIR}/SRPMS"
    VERBATIM
  )
else()
  message(STATUS "srpm target disabled (requires rpmbuild)")
endif()

# ─── Linux / GTK4 / JACK target ──────────────────────────────────────────────
if(OPIQO_TARGET_PLATFORM STREQUAL "linux")

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK4     REQUIRED IMPORTED_TARGET gtk4)
  pkg_check_modules(JACK     REQUIRED IMPORTED_TARGET jack)
  pkg_check_modules(LILV     REQUIRED IMPORTED_TARGET lilv-0)
  pkg_check_modules(SNDFILE  REQUIRED IMPORTED_TARGET sndfile)

  # Optional audio encoders (non-fatal — FileWriter guards them at runtime)
  pkg_check_modules(OGG         IMPORTED_TARGET ogg)
  pkg_check_modules(VORBIS      IMPORTED_TARGET vorbis)
  pkg_check_modules(VORBISENC   IMPORTED_TARGET vorbisenc)

  # ── Compile GLib resources (CSS + UI stubs, if any) ──────────────────────
  find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
  set(GRESOURCE_XML  "${CMAKE_CURRENT_SOURCE_DIR}/src/gtk4/opiqo.gresource.xml")
  set(GRESOURCE_C    "${CMAKE_CURRENT_BINARY_DIR}/opiqo_resources.c")
  add_custom_command(
    OUTPUT  "${GRESOURCE_C}"
    COMMAND "${GLIB_COMPILE_RESOURCES}"
            --target="${GRESOURCE_C}"
            --generate-source
            --sourcedir="${CMAKE_CURRENT_SOURCE_DIR}/src/gtk4"
            "${GRESOURCE_XML}"
    DEPENDS "${GRESOURCE_XML}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gtk4/opiqo.css"
    COMMENT "Compiling GLib resources"
  )

  add_executable(opiqo
    # Resources (generated)
    "${GRESOURCE_C}"

    # Linux / GTK4 / JACK front-end
    src/main_linux.cpp
    src/gtk4/AppSettings.cpp
    src/gtk4/AudioEngine.cpp
    src/gtk4/JackPortEnum.cpp
    src/gtk4/MainWindow.cpp
    src/gtk4/ControlBar.cpp
    src/gtk4/PresetBar.cpp
    src/gtk4/PluginSlot.cpp
    src/gtk4/PluginDialog.cpp
    src/gtk4/ParameterPanel.cpp
    src/gtk4/SettingsDialog.cpp

    # Shared DSP core (unchanged from Android build)
    src/LiveEffectEngine.cpp
    src/FileWriter.cpp
    src/LockFreeQueue.cpp
  )

  target_include_directories(opiqo PRIVATE
    src/gtk4/
    src/
    ${GTK4_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${LILV_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
  )

  target_compile_options(opiqo PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${JACK_CFLAGS_OTHER}
    ${LILV_CFLAGS_OTHER}
    -Wall -Wextra -Wno-unused-parameter
  )

  target_link_libraries(opiqo PRIVATE
    PkgConfig::GTK4
    PkgConfig::JACK
    PkgConfig::LILV
    PkgConfig::SNDFILE
    -lmp3lame
    -lopus
    -lopusenc
    -lFLAC
    pthread
    dl
  )

  if(OGG_FOUND)
    target_link_libraries(opiqo PRIVATE PkgConfig::OGG)
  endif()
  if(VORBIS_FOUND)
    target_link_libraries(opiqo PRIVATE PkgConfig::VORBIS)
  endif()
  if(VORBISENC_FOUND)
    target_link_libraries(opiqo PRIVATE PkgConfig::VORBISENC)
  endif()

  install(TARGETS opiqo RUNTIME DESTINATION bin)

  # ─── Unit tests ──────────────────────────────────────────────────────────────
  find_package(GTest QUIET)
  if(GTest_FOUND)
    enable_testing()

    add_executable(opiqo_tests
      tests/test_live_effect_engine.cpp

      # DSP core (no GTK/JACK UI)
      src/LiveEffectEngine.cpp
      src/FileWriter.cpp
      src/LockFreeQueue.cpp

      # Stub AudioEngine — avoids GTK dependency while still testing the lifecycle API
      tests/audio_engine_stub.cpp
    )

    target_include_directories(opiqo_tests PRIVATE
      src/
      src/gtk4/
      ${JACK_INCLUDE_DIRS}
      ${LILV_INCLUDE_DIRS}
      ${SNDFILE_INCLUDE_DIRS}
    )

    target_compile_options(opiqo_tests PRIVATE
      ${JACK_CFLAGS_OTHER}
      ${LILV_CFLAGS_OTHER}
      -Wall -Wextra -Wno-unused-parameter
    )

    target_link_libraries(opiqo_tests PRIVATE
      GTest::GTest
      PkgConfig::JACK
      PkgConfig::LILV
      PkgConfig::SNDFILE
      -lmp3lame
      -lopus
      -lopusenc
      -lFLAC
      pthread
      dl
    )

    if(OGG_FOUND)
      target_link_libraries(opiqo_tests PRIVATE PkgConfig::OGG)
    endif()
    if(VORBIS_FOUND)
      target_link_libraries(opiqo_tests PRIVATE PkgConfig::VORBIS)
    endif()
    if(VORBISENC_FOUND)
      target_link_libraries(opiqo_tests PRIVATE PkgConfig::VORBISENC)
    endif()

    include(GoogleTest)
    gtest_discover_tests(opiqo_tests)

    # ── LV2Plugin unit tests (no JACK / GTK dependency) ──────────────────────
    add_executable(lv2plugin_tests
      tests/test_lv2plugin.cpp
      src/LockFreeQueue.cpp
    )

    target_include_directories(lv2plugin_tests PRIVATE
      src/
      ${LILV_INCLUDE_DIRS}
    )

    target_compile_options(lv2plugin_tests PRIVATE
      ${LILV_CFLAGS_OTHER}
      -Wall -Wextra -Wno-unused-parameter
    )

    target_link_libraries(lv2plugin_tests PRIVATE
      GTest::GTest
      GTest::Main
      PkgConfig::LILV
      pthread
      dl
    )

    gtest_discover_tests(lv2plugin_tests)

  else()
    message(STATUS "GTest not found — skipping unit test target")
  endif()

endif() # OPIQO_TARGET_PLATFORM linux

# ─── Linux / GTK3 / JACK target ──────────────────────────────────────────────
if(OPIQO_TARGET_PLATFORM STREQUAL "linux-gtk3")

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3     REQUIRED IMPORTED_TARGET gtk+-3.0)
  pkg_check_modules(JACK     REQUIRED IMPORTED_TARGET jack)
  pkg_check_modules(LILV     REQUIRED IMPORTED_TARGET lilv-0)
  pkg_check_modules(SNDFILE  REQUIRED IMPORTED_TARGET sndfile)

  pkg_check_modules(OGG         IMPORTED_TARGET ogg)
  pkg_check_modules(VORBIS      IMPORTED_TARGET vorbis)
  pkg_check_modules(VORBISENC   IMPORTED_TARGET vorbisenc)

  add_executable(opiqo-gtk3
    src/main_linux.cpp
    src/gtk3/AppSettings.cpp
    src/gtk3/AudioEngine.cpp
    src/gtk3/JackPortEnum.cpp
    src/gtk3/MainWindow.cpp
    src/gtk3/ControlBar.cpp
    src/gtk3/PresetBar.cpp
    src/gtk3/PluginSlot.cpp
    src/gtk3/PluginDialog.cpp
    src/gtk3/ParameterPanel.cpp
    src/gtk3/SettingsDialog.cpp
    src/LiveEffectEngine.cpp
    src/FileWriter.cpp
    src/LockFreeQueue.cpp
  )

  target_include_directories(opiqo-gtk3 PRIVATE
    src/gtk3/
    src/
    ${GTK3_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${LILV_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
  )

  target_compile_options(opiqo-gtk3 PRIVATE
    ${GTK3_CFLAGS_OTHER}
    ${JACK_CFLAGS_OTHER}
    ${LILV_CFLAGS_OTHER}
    -Wall -Wextra -Wno-unused-parameter
  )

  target_link_libraries(opiqo-gtk3 PRIVATE
    PkgConfig::GTK3
    PkgConfig::JACK
    PkgConfig::LILV
    PkgConfig::SNDFILE
    -lmp3lame
    -lopus
    -lopusenc
    -lFLAC
    pthread
    dl
  )

  if(OGG_FOUND)
    target_link_libraries(opiqo-gtk3 PRIVATE PkgConfig::OGG)
  endif()
  if(VORBIS_FOUND)
    target_link_libraries(opiqo-gtk3 PRIVATE PkgConfig::VORBIS)
  endif()
  if(VORBISENC_FOUND)
    target_link_libraries(opiqo-gtk3 PRIVATE PkgConfig::VORBISENC)
  endif()

  install(TARGETS opiqo-gtk3 RUNTIME DESTINATION bin)

endif() # OPIQO_TARGET_PLATFORM linux-gtk3

# ─── Linux / GTK2 / JACK target ──────────────────────────────────────────────
if(OPIQO_TARGET_PLATFORM STREQUAL "linux-gtk2")

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK2     REQUIRED IMPORTED_TARGET gtk+-2.0)
  pkg_check_modules(JACK     REQUIRED IMPORTED_TARGET jack)
  pkg_check_modules(LILV     REQUIRED IMPORTED_TARGET lilv-0)
  pkg_check_modules(SNDFILE  REQUIRED IMPORTED_TARGET sndfile)

  pkg_check_modules(OGG         IMPORTED_TARGET ogg)
  pkg_check_modules(VORBIS      IMPORTED_TARGET vorbis)
  pkg_check_modules(VORBISENC   IMPORTED_TARGET vorbisenc)

  add_executable(opiqo-gtk2
    src/gtk2/main_gtk2.cpp
    src/gtk2/AppSettings.cpp
    src/gtk2/AudioEngine.cpp
    src/gtk2/JackPortEnum.cpp
    src/gtk2/MainWindow.cpp
    src/gtk2/ControlBar.cpp
    src/gtk2/PresetBar.cpp
    src/gtk2/PluginSlot.cpp
    src/gtk2/PluginDialog.cpp
    src/gtk2/ParameterPanel.cpp
    src/gtk2/SettingsDialog.cpp
    src/LiveEffectEngine.cpp
    src/FileWriter.cpp
    src/LockFreeQueue.cpp
  )

  target_include_directories(opiqo-gtk2 PRIVATE
    src/gtk2/
    src/
    ${GTK2_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${LILV_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
  )

  target_compile_options(opiqo-gtk2 PRIVATE
    ${GTK2_CFLAGS_OTHER}
    ${JACK_CFLAGS_OTHER}
    ${LILV_CFLAGS_OTHER}
    -DOPIQO_GTK2
    -Wall -Wextra -Wno-unused-parameter
    -Wno-deprecated-declarations  # GTK2 uses deprecated GLib/Pango symbols
  )

  target_link_libraries(opiqo-gtk2 PRIVATE
    PkgConfig::GTK2
    PkgConfig::JACK
    PkgConfig::LILV
    PkgConfig::SNDFILE
    -lmp3lame
    -lopus
    -lopusenc
    -lFLAC
    pthread
    dl
  )

  if(OGG_FOUND)
    target_link_libraries(opiqo-gtk2 PRIVATE PkgConfig::OGG)
  endif()
  if(VORBIS_FOUND)
    target_link_libraries(opiqo-gtk2 PRIVATE PkgConfig::VORBIS)
  endif()
  if(VORBISENC_FOUND)
    target_link_libraries(opiqo-gtk2 PRIVATE PkgConfig::VORBISENC)
  endif()

  install(TARGETS opiqo-gtk2 RUNTIME DESTINATION bin)

endif() # OPIQO_TARGET_PLATFORM linux-gtk2
