cmake_minimum_required (VERSION 2.8)

# This has to happen after the project command, otherwise CMAKE_CXX_FLAGS_* won't be set properly.
if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable ExternalProject CMake module
include(ExternalProject)

# Set default ExternalProject root directory
set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/third_party)

include(FindPkgConfig)
find_package(Boost COMPONENTS serialization regex system filesystem thread REQUIRED)
find_package(Libiw REQUIRED)
pkg_check_modules(JMSWM REQUIRED x11 xft xrandr libevent pango pangoxft alsa)

# Add chaos-pp external project.
include(ExternalProject)
ExternalProject_Add(
  chaos-pp
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/third_party
  GIT_REPOSITORY https://github.com/ldionne/chaos-pp.git
  GIT_TAG 0ba18b6c915e6718177f2911dd63c7f88493a2c3
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  TEST_COMMAND "")

set(CHAOS_PP_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/third_party/src/chaos-pp)

add_executable(jmswm
  src/util/spawn.cpp
  src/util/path.cpp
  src/util/event.cpp
  src/util/close_on_exec.cpp
  src/util/log.cpp
  src/draw/draw.cpp
  src/style/db.cpp
  src/menu/list_completion.cpp
  src/menu/url_completion.cpp
  src/menu/file_completion.cpp
  src/menu/menu.cpp
  src/wm/view.cpp
  src/wm/main.cpp
  src/wm/bar.cpp
  src/wm/event.cpp
  src/wm/extra/place.cpp
  src/wm/extra/gnus_applet.cpp
  src/wm/extra/battery_applet.cpp
  src/wm/extra/erc_applet.cpp
  src/wm/extra/web_browser.cpp
  src/wm/extra/volume_applet.cpp
  src/wm/extra/device_applet.cpp
  src/wm/extra/view_columns.cpp
  src/wm/extra/fullscreen.cpp
  src/wm/extra/network_applet.cpp
  src/wm/extra/cwd.cpp
  src/wm/extra/bar_view_applet.cpp
  src/wm/client.cpp
  src/wm/xwindow.cpp
  src/wm/wm.cpp
  src/wm/sizehint.cpp
  src/wm/key.cpp
  src/wm/persistence.cpp
  src/wm/frame.cpp
  src/wm/commands.cpp
  )

add_dependencies(jmswm chaos-pp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1y -ggdb -Wall -Wno-maybe-uninitialized")
add_definitions(-DCHAOS_PP_VARIADICS=1)

target_link_libraries(jmswm
  ${Boost_SERIALIZATION_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${LIBIW_LIBRARIES}
  ${JMSWM_LIBRARIES}
  )
include_directories(
  "${PROJECT_SOURCE_DIR}/src"
  ${CHAOS_PP_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${LIBIW_INCLUDE_DIR}
  ${JMSWM_INCLUDE_DIRS}
  )
