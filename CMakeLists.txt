cmake_minimum_required(VERSION 3.10)

set(BOREALIS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lib/borealis)
set(BOREALIS_LIBRARY ${BOREALIS_ROOT}/library)

include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(totk_save_editor)
option(TOTK_ICON_ATLAS "Use sprite-sheet icon atlases instead of per-PNG loads" OFF)
option(TOTK_NXLINK_DEBUG "NXLink debug profile (-O0, stamped build id, verbose logs)" OFF)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_ALTER "0")
set(VERSION_BUILD "0")
set(PROJECT_AUTHOR "TotK Save Editor")
set(PROJECT_TITLEID "0x0100000005134C00")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
if (NOT EXISTS "${PROJECT_ICON}")
    if (EXISTS "$ENV{DEVKITPRO}/libnx/default_icon.jpg")
        set(PROJECT_ICON "$ENV{DEVKITPRO}/libnx/default_icon.jpg")
    endif()
endif()
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(BOREALIS_RESOURCES ${BOREALIS_ROOT}/resources)

add_subdirectory(${BOREALIS_ROOT}/library)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

list(APPEND APP_PLATFORM_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/source
)

file(GLOB_RECURSE MAIN_SRC
    source/*.cpp
    source/**/*.cpp
)

set(PLATFORM_OPTION)
if (PLATFORM_SWITCH)
    if (BOREALIS_USE_DEKO3D)
        list(APPEND APP_PLATFORM_LIB deko3d nx m)
    elseif (USE_SDL2)
        list(APPEND APP_PLATFORM_LIB EGL glapi drm_nouveau nx m)
    else()
        list(APPEND APP_PLATFORM_LIB glfw3 EGL glapi drm_nouveau nx m)
    endif()
    # HyruleWorks Autobuild networking (curl -> mbedtls -> mbedx509 ->
    # mbedcrypto -> z is the required static-link order; devkitPro's
    # switch-curl portlib is built against switch-mbedtls, not OpenSSL).
    list(APPEND APP_PLATFORM_LIB curl mbedtls mbedx509 mbedcrypto z)
    list(APPEND MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/platform/switch_wrapper.c)
endif()

program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

if (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        gen_dksh("${CMAKE_BINARY_DIR}/resources/shaders")
    endif()
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "TotK Save Editor" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALIS_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/resources/assets.zip
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/assets/item_icons_preferred
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/assets/item_icons_legacy
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/assets/item_icons_orbs
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/assets/item_icons_curated
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/assets/icon_sources
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
elseif (PLATFORM_DESKTOP)
    add_custom_target(${PROJECT_NAME}.data
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
    )
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
if (TOTK_ICON_ATLAS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TOTK_ICON_ATLAS=1)
endif()
if (TOTK_NXLINK_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TOTK_NXLINK_DEBUG=1)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
