cmake_minimum_required(VERSION 3.22)

project(ac-songlist VERSION 1.0.0 LANGUAGES CXX)

option(ACSL_BUILD_PLUGIN "Compilar el plugin de OBS" ON)
option(ACSL_BUILD_TESTS "Compilar las pruebas del nucleo" OFF)
# Windows: carpeta con las cabeceras de OBS y las librerias de importacion
# (ver .github/workflows/ac-songlist.yml). En Linux se usa el libobs del sistema.
set(OBS_SDK_DIR "" CACHE PATH "SDK de OBS preparado a mano (include/ y lib/)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(MSVC)
  add_compile_options(/utf-8 /W3 /permissive-)
  add_compile_definitions(UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# ---------------------------------------------------------------- nucleo
# Biblioteca, cola, comandos de chat y dibujado: no depende de OBS.
add_library(acsl-core STATIC
  src/core/model.hpp
  src/core/matcher.hpp src/core/matcher.cpp
  src/core/store.hpp src/core/store.cpp
  src/core/csv.hpp src/core/csv.cpp
  src/core/bot.hpp src/core/bot.cpp
  src/chat/twitch-chat.hpp src/chat/twitch-chat.cpp
  src/overlay/renderer.hpp src/overlay/renderer.cpp)
target_link_libraries(acsl-core PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
target_include_directories(acsl-core PUBLIC src)
target_compile_definitions(acsl-core PUBLIC QT_NO_CAST_TO_ASCII ACSL_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------- plugin
if(ACSL_BUILD_PLUGIN)
  if(OBS_SDK_DIR)
    if(APPLE)
      set(_obs_lib "${OBS_SDK_DIR}/lib/libobs.dylib")
      set(_obs_frontend_lib "${OBS_SDK_DIR}/lib/obs-frontend-api.dylib")
    else()
      set(_obs_lib "${OBS_SDK_DIR}/lib/obs.lib")
      set(_obs_frontend_lib "${OBS_SDK_DIR}/lib/obs-frontend-api.lib")
    endif()
    add_library(OBS::libobs UNKNOWN IMPORTED)
    set_target_properties(OBS::libobs PROPERTIES
      IMPORTED_LOCATION "${_obs_lib}"
      INTERFACE_INCLUDE_DIRECTORIES "${OBS_SDK_DIR}/include")
    add_library(OBS::obs-frontend-api UNKNOWN IMPORTED)
    set_target_properties(OBS::obs-frontend-api PROPERTIES
      IMPORTED_LOCATION "${_obs_frontend_lib}"
      INTERFACE_INCLUDE_DIRECTORIES "${OBS_SDK_DIR}/include")
  else()
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
  endif()

  add_library(ac-songlist MODULE
    src/plugin-main.cpp
    src/plugin-support.hpp
    src/plugin.hpp src/plugin.cpp
    src/overlay/overlay-source.hpp src/overlay/overlay-source.cpp
    src/ui/dock.hpp src/ui/dock.cpp
    src/ui/song-dialog.hpp src/ui/song-dialog.cpp)
  target_link_libraries(ac-songlist PRIVATE acsl-core OBS::libobs OBS::obs-frontend-api Qt6::Widgets)
  set_target_properties(ac-songlist PROPERTIES PREFIX "")
  if(APPLE)
    # Qt y libobs se resuelven dentro de OBS.app, no en la maquina donde se compilo.
    set_target_properties(ac-songlist PROPERTIES
      BUILD_WITH_INSTALL_RPATH ON
      INSTALL_RPATH "@executable_path/../Frameworks")
  endif()

  if(WIN32)
    # Estructura de C:\ProgramData\obs-studio\plugins\ac-songlist
    install(TARGETS ac-songlist LIBRARY DESTINATION "ac-songlist/bin/64bit" RUNTIME DESTINATION "ac-songlist/bin/64bit")
    install(DIRECTORY data/ DESTINATION "ac-songlist/data")
  else()
    include(GNUInstallDirs)
    install(TARGETS ac-songlist LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATADIR}/obs/obs-plugins/ac-songlist")
  endif()
endif()

# ---------------------------------------------------------------- pruebas
if(ACSL_BUILD_TESTS)
  enable_testing()
  add_executable(acsl-tests tests/test-core.cpp)
  target_link_libraries(acsl-tests PRIVATE acsl-core)
  add_test(NAME core COMMAND acsl-tests)
  set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_executable(acsl-render tests/render-samples.cpp)
  target_link_libraries(acsl-render PRIVATE acsl-core)
endif()
