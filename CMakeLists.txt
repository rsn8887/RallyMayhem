# Rally Mayhem
#
# Desktop (macOS / Linux / Windows):
#   cmake -S . -B ../build && cmake --build ../build
#   -> ../build/rallymayhem  (with its assets/ folder next to it); on macOS ../build/RallyMayhem.app
#   macOS .dmg for sharing: ./make_dmg.sh
#
# Nintendo Switch (devkitPro, needs DEVKITPRO set):
#   cmake -S . -B ../build-switch -DPLATFORM=SWITCH && cmake --build ../build-switch
#   -> ../build-switch/rallymayhem.nro  (assets/ packed into the .nro as RomFS)
#
# PS Vita (VitaSDK, needs VITASDK set; runs on Vitas with custom firmware / HENkaku):
#   cmake -S . -B ../build-vita -DPLATFORM=VITA && cmake --build ../build-vita
#   -> ../build-vita/RallyMayhem.vpk  (assets/ packed in as app0:/assets)
#
# Android: Gradle project in android/ (uses this version number), built with
#   sh android/build.sh  inside the docker/android image -> build-android/RallyMayhem-<version>.apk
#
# Passing devkitPro's toolchain directly works too:
#   cmake -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
#   (or use the aarch64-none-elf-cmake wrapper)
cmake_minimum_required(VERSION 3.16)

set(PLATFORM "DESKTOP" CACHE STRING "Target platform: DESKTOP, SWITCH or VITA")
set_property(CACHE PLATFORM PROPERTY STRINGS DESKTOP SWITCH VITA)
string(TOUPPER "${PLATFORM}" PLATFORM_UC)

# The toolchain must be chosen before project()
if(PLATFORM_UC STREQUAL "SWITCH" AND NOT CMAKE_TOOLCHAIN_FILE)
  if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "PLATFORM=SWITCH needs the DEVKITPRO environment variable "
                        "(e.g. export DEVKITPRO=/opt/devkitpro)")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/Switch.cmake" CACHE FILEPATH "devkitPro Switch toolchain")
endif()
if(PLATFORM_UC STREQUAL "VITA" AND NOT CMAKE_TOOLCHAIN_FILE)
  if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "PLATFORM=VITA needs the VITASDK environment variable "
                        "(e.g. export VITASDK=/usr/local/vitasdk)")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE FILEPATH "VitaSDK toolchain")
endif()

project(RallyMayhem VERSION 1.2.0 LANGUAGES C CXX)

# Out-of-project builds only: build folders sit next to the project folder, never inside it.
get_filename_component(_src "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(_bin "${CMAKE_BINARY_DIR}" REALPATH)
string(FIND "${_bin}/" "${_src}/" _inside)
if(_inside EQUAL 0)
  message(FATAL_ERROR "Please build outside the project folder, e.g.\n  cmake -S ./rallymayhem -B build")
endif()

# Also recognise Switch / Vita builds configured via the toolchain file alone
if(NINTENDO_SWITCH)
  set(PLATFORM_UC "SWITCH")
elseif(VITA)
  set(PLATFORM_UC "VITA")
elseif(PLATFORM_UC STREQUAL "SWITCH" OR PLATFORM_UC STREQUAL "VITA")
  message(FATAL_ERROR "PLATFORM=${PLATFORM_UC} but its toolchain is not active "
                      "(delete the build folder and configure again)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(rallymayhem
  src/main.cpp src/game.cpp src/cars.cpp src/level.cpp src/draw.cpp src/gfx.cpp src/synth.cpp src/tracks.cpp)
target_include_directories(rallymayhem PRIVATE src)

if(PLATFORM_UC STREQUAL "SWITCH")
  # ---------------------------------------------------------------- Nintendo Switch
  # The devkitPro toolchain already adds -D__SWITCH__, the CPU flags, libnx and the
  # link specs. SDL2 + SDL2_ttf (switch-sdl2, switch-sdl2_ttf) come from portlibs via
  # aarch64-none-elf-pkg-config. Everything is linked statically.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL REQUIRED SDL2_ttf sdl2)
  target_include_directories(rallymayhem PRIVATE ${SDL_STATIC_INCLUDE_DIRS})
  target_compile_options(rallymayhem PRIVATE ${SDL_STATIC_CFLAGS_OTHER})
  target_link_libraries(rallymayhem PRIVATE ${SDL_STATIC_LDFLAGS})

  # RomFS: the assets/ folder becomes romfs:/ inside the .nro. The game reads the font
  # from there (romfsInit() / "romfs:/font.ttf" in main.cpp). Every file is registered
  # so editing an asset rebuilds the .nro.
  dkp_add_asset_target(rallymayhem_romfs "${CMAKE_CURRENT_SOURCE_DIR}/assets")
  file(GLOB_RECURSE ROMFS_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
       CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
  dkp_track_assets(rallymayhem_romfs FILES ${ROMFS_FILES})

  nx_generate_nacp(OUTPUT rallymayhem.nacp
    NAME    "Rally Mayhem"
    AUTHOR  "rsn8887"
    VERSION "${PROJECT_VERSION}")

  nx_create_nro(rallymayhem
    NACP  rallymayhem.nacp
    ICON  "${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg"
    ROMFS rallymayhem_romfs)
  # -> ${CMAKE_BINARY_DIR}/rallymayhem.nro

elseif(PLATFORM_UC STREQUAL "VITA")
  # ---------------------------------------------------------------- PS Vita
  # VitaSDK's toolchain defines __vita__ and links with -Wl,-q. SDL2 + SDL2_ttf (and
  # freetype, harfbuzz, the Sce* system stubs, ...) come from VitaSDK's packages via
  # arm-vita-eabi-pkg-config and are linked statically.
  include("${VITASDK}/share/vita.cmake" REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL REQUIRED SDL2_ttf sdl2)
  target_include_directories(rallymayhem PRIVATE ${SDL_STATIC_INCLUDE_DIRS})
  target_compile_options(rallymayhem PRIVATE ${SDL_STATIC_CFLAGS_OTHER})
  target_link_libraries(rallymayhem PRIVATE ${SDL_STATIC_LDFLAGS})
  # VitaSDK's SDL2 is built with vitaGL, whose libvitashark calls into SceShaccCgExt
  # (which needs taiHEN); SDL2's pkg-config file doesn't list them, so add them after
  # the SDL libraries (static link order matters).
  find_library(SHACCCGEXT_LIB SceShaccCgExt)
  if(NOT SHACCCGEXT_LIB)
    message(FATAL_ERROR "libSceShaccCgExt not found in VitaSDK - install it with:  vdpm SceShaccCgExt")
  endif()
  target_link_libraries(rallymayhem PRIVATE SceShaccCgExt taihen_stub SceShaccCg_stub)

  # VPK version is ##.## : 1.0.0 -> "01.00"
  math(EXPR _vmin "${PROJECT_VERSION_MINOR} * 10 + ${PROJECT_VERSION_PATCH}")
  set(_vmaj "${PROJECT_VERSION_MAJOR}")
  if(_vmaj LESS 10)
    set(_vmaj "0${_vmaj}")
  endif()
  if(_vmin LESS 10)
    set(_vmin "0${_vmin}")
  endif()
  set(VITA_APP_VERSION "${_vmaj}.${_vmin}")
  set(VITA_TITLEID "RLYM00001")   # 4 letters + 5 digits, unique per homebrew

  # Files packed into the .vpk: LiveArea graphics (8-bit indexed PNGs) and the game
  # assets, which the game reads from app0:/assets/ (see main.cpp).
  set(VPK_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/icon0.png" sce_sys/icon0.png
    "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/livearea/contents/bg.png" sce_sys/livearea/contents/bg.png
    "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/livearea/contents/startup.png" sce_sys/livearea/contents/startup.png
    "${CMAKE_CURRENT_SOURCE_DIR}/vita/sce_sys/livearea/contents/template.xml" sce_sys/livearea/contents/template.xml)
  file(GLOB_RECURSE VITA_ASSETS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
       CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
  foreach(f IN LISTS VITA_ASSETS)
    list(APPEND VPK_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/${f}" "assets/${f}")
  endforeach()

  vita_create_self(eboot.bin rallymayhem)
  vita_create_vpk(RallyMayhem.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_APP_VERSION}
    NAME "Rally Mayhem"
    FILE ${VPK_FILES})
  # -> ${CMAKE_BINARY_DIR}/RallyMayhem.vpk

else()
  # ---------------------------------------------------------------- Desktop
  find_package(SDL2 CONFIG QUIET)
  find_package(SDL2_ttf CONFIG QUIET)

  if(TARGET SDL2::SDL2 AND TARGET SDL2_ttf::SDL2_ttf)
    if(TARGET SDL2::SDL2main)
      target_link_libraries(rallymayhem PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(rallymayhem PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
  else()
    # fallback: pkg-config (Linux distros, older Homebrew)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2_ttf)
    target_link_libraries(rallymayhem PRIVATE PkgConfig::SDL2)
  endif()

  if(MSVC)
    target_compile_options(rallymayhem PRIVATE /W3)
  else()
    target_compile_options(rallymayhem PRIVATE -Wall -Wextra -Wno-unused-function)
  endif()

  if(APPLE)
    # macOS: the game is an app bundle (RallyMayhem.app) with its icon and the assets in Contents/Resources
    # (SDL_GetBasePath() returns the bundle's Resources folder). make_dmg.sh turns it into a .dmg.
    set(ICNS ${CMAKE_CURRENT_SOURCE_DIR}/macos/RallyMayhem.icns)
    target_sources(rallymayhem PRIVATE ${ICNS})
    set_source_files_properties(${ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(rallymayhem PROPERTIES
      OUTPUT_NAME RallyMayhem
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in
      MACOSX_BUNDLE_BUNDLE_NAME "Rally Mayhem"
      MACOSX_BUNDLE_GUI_IDENTIFIER com.rsn8887.rallymayhem
      MACOSX_BUNDLE_ICON_FILE RallyMayhem.icns
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_COPYRIGHT "rsn8887")
    # room in the header so make_dmg.sh can point the library references into the app
    target_link_options(rallymayhem PRIVATE -Wl,-headerpad_max_install_names)
    set(ASSETS_DEST $<TARGET_BUNDLE_CONTENT_DIR:rallymayhem>/Resources/assets)
  else()
    set(ASSETS_DEST $<TARGET_FILE_DIR:rallymayhem>/assets)
  endif()
  # the game looks for assets/ next to its executable (macOS: in the app's Resources)
  add_custom_command(TARGET rallymayhem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${ASSETS_DEST})
endif()
