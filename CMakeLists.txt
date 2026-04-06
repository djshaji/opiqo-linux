cmake_minimum_required(VERSION 3.21)
project(opiqo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
  set(GRESOURCE_XML  "${CMAKE_CURRENT_SOURCE_DIR}/src/linux/opiqo.gresource.xml")
  set(GRESOURCE_C    "${CMAKE_CURRENT_BINARY_DIR}/opiqo_resources.c")
  add_custom_command(
    OUTPUT  "${GRESOURCE_C}"
    COMMAND "${GLIB_COMPILE_RESOURCES}"
            --target="${GRESOURCE_C}"
            --generate-source
            --sourcedir="${CMAKE_CURRENT_SOURCE_DIR}/src/linux"
            "${GRESOURCE_XML}"
    DEPENDS "${GRESOURCE_XML}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/linux/opiqo.css"
    COMMENT "Compiling GLib resources"
  )

  add_executable(opiqo
    # Resources (generated)
    "${GRESOURCE_C}"

    # Linux / GTK4 / JACK front-end
    src/main_linux.cpp
    src/linux/AppSettings.cpp
    src/linux/AudioEngine.cpp
    src/linux/JackPortEnum.cpp
    src/linux/MainWindow.cpp
    src/linux/ControlBar.cpp
    src/linux/PluginSlot.cpp
    src/linux/PluginDialog.cpp
    src/linux/ParameterPanel.cpp
    src/linux/SettingsDialog.cpp

    # Shared DSP core (unchanged from Android build)
    src/LiveEffectEngine.cpp
    src/FileWriter.cpp
    src/LockFreeQueue.cpp
  )

  target_include_directories(opiqo PRIVATE
    src/
    src/linux/
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

  # Optional: address-sanitizer build for testing
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(opiqo PRIVATE -fsanitize=address,undefined)
    target_link_options(opiqo PRIVATE -fsanitize=address,undefined)
  endif()

  install(TARGETS opiqo RUNTIME DESTINATION bin)

endif() # OPIQO_TARGET_PLATFORM linux
