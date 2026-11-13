cmake_minimum_required(VERSION 3.22)

project(BoulesDeGivre
    VERSION 0.1.0
    DESCRIPTION "Boules de Givre - jeu d'arcade C/SDL3"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(BDG_USE_SYSTEM_SDL "Utiliser un SDL3 installe sur le systeme s'il est trouve" OFF)
option(BDG_BUILD_TESTS "Construire les tests unitaires" ON)

# Identifiants de l'application (bundle iOS, package Android, dossier de sauvegarde)
set(BDG_APP_ID "com.johanbaskovec.boulesdegivre" CACHE STRING "Identifiant d'application (reverse DNS)")
set(BDG_APPLE_TEAM_ID "" CACHE STRING "Team ID Apple Developer pour la signature iOS")

# ---------------------------------------------------------------------------
# SDL3 : systeme (optionnel) > sous-module third_party/SDL > FetchContent
# ---------------------------------------------------------------------------
if(BDG_USE_SYSTEM_SDL)
    find_package(SDL3 3.2 CONFIG QUIET)
endif()

if(NOT TARGET SDL3::SDL3)
    if(ANDROID)
        # Android : SDLActivity charge libSDL3.so puis libmain.so
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    endif()
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL/CMakeLists.txt")
        if(ANDROID)
            # Gradle doit voir la cible SDL3-shared pour empaqueter libSDL3.so
            add_subdirectory(third_party/SDL)
        else()
            add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
        endif()
    else()
        message(STATUS "Sous-module SDL absent : telechargement via FetchContent")
        include(FetchContent)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.4.18
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL3)
    endif()
endif()

# ---------------------------------------------------------------------------
# Ressources embarquees (police + niveaux de la campagne) -> C genere
# ---------------------------------------------------------------------------
file(GLOB BDG_CAMPAIGN_LEVELS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/levels/*.lvl")
list(SORT BDG_CAMPAIGN_LEVELS)
set(BDG_FONT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Fredoka-SemiBold.ttf")
set(BDG_EMBED_C "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_assets.c")

add_custom_command(
    OUTPUT "${BDG_EMBED_C}"
    COMMAND ${CMAKE_COMMAND}
        "-DOUTPUT=${BDG_EMBED_C}"
        "-DFONT=${BDG_FONT_FILE}"
        "-DLEVELS=${BDG_CAMPAIGN_LEVELS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedAssets.cmake"
    DEPENDS ${BDG_CAMPAIGN_LEVELS} "${BDG_FONT_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedAssets.cmake"
    COMMENT "Integration de la police et des niveaux dans le binaire"
    VERBATIM)

# ---------------------------------------------------------------------------
# Bibliotheque coeur (sans point d'entree) : partagee par le jeu et les tests
# ---------------------------------------------------------------------------
set(BDG_CORE_SOURCES
    src/level.c
)

set(BDG_GAME_SOURCES
    src/main.c
    src/app.c
    src/gfx.c
    src/font.c
    src/audio.c
    src/input.c
    src/ui.c
    src/storage.c
    src/settings.c
    src/game.c
    src/game_draw.c
    src/scene_menu.c
    src/scene_levels.c
    src/scene_options.c
    src/scene_editor.c
    "${BDG_EMBED_C}"
)

add_library(bdg_core STATIC ${BDG_CORE_SOURCES})
target_include_directories(bdg_core PUBLIC src)

function(bdg_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wshadow -Wno-unused-parameter)
    endif()
endfunction()
bdg_warnings(bdg_core)
if(NOT MSVC AND NOT EMSCRIPTEN)
    target_link_libraries(bdg_core PUBLIC m)
endif()

if(ANDROID)
    add_library(main SHARED ${BDG_GAME_SOURCES})
    set(BDG_GAME_TARGET main)
else()
    add_executable(boules-de-givre ${BDG_GAME_SOURCES})
    set(BDG_GAME_TARGET boules-de-givre)
endif()

target_include_directories(${BDG_GAME_TARGET} PRIVATE src third_party/stb)
target_link_libraries(${BDG_GAME_TARGET} PRIVATE bdg_core SDL3::SDL3)
target_compile_definitions(${BDG_GAME_TARGET} PRIVATE
    BDG_VERSION="${PROJECT_VERSION}"
    BDG_APP_ID="${BDG_APP_ID}")
bdg_warnings(${BDG_GAME_TARGET})

# ---------------------------------------------------------------------------
# Specificites par plateforme
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
    set_target_properties(${BDG_GAME_TARGET} PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html")
    target_link_options(${BDG_GAME_TARGET} PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sEXPORTED_RUNTIME_METHODS=['FS']"
        "-lidbfs.js"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/platforms/web/shell.html")
    set_property(TARGET ${BDG_GAME_TARGET} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platforms/web/shell.html")
    configure_file(platforms/web/favicon.png "${CMAKE_CURRENT_BINARY_DIR}/favicon.png" COPYONLY)
elseif(IOS)
    set(BDG_IOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platforms/ios")
    set(BDG_IOS_RESOURCES
        "${BDG_IOS_DIR}/LaunchScreen.storyboard"
        "${BDG_IOS_DIR}/Assets.xcassets")
    target_sources(${BDG_GAME_TARGET} PRIVATE ${BDG_IOS_RESOURCES})
    set_source_files_properties(${BDG_IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(${BDG_GAME_TARGET} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${BDG_IOS_DIR}/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${BDG_APP_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "Boules de Givre"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BDG_APP_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${BDG_APPLE_TEAM_ID}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
elseif(APPLE)
    set_target_properties(${BDG_GAME_TARGET} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "${BDG_APP_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "Boules de Givre")
elseif(WIN32)
    set_target_properties(${BDG_GAME_TARGET} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BDG_BUILD_TESTS AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS)
    enable_testing()
    add_executable(test_level tests/test_level.c)
    target_link_libraries(test_level PRIVATE bdg_core)
    target_compile_definitions(test_level PRIVATE
        BDG_LEVELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/levels")
    bdg_warnings(test_level)
    add_test(NAME level_format COMMAND test_level)

    # Test de fumee de l'editeur : rejoue tests/editor_smoke.script hors ecran
    # (pilote video "offscreen" de SDL) et verifie le fichier .lvl produit.
    if(UNIX AND NOT APPLE)
        add_test(NAME editor_smoke
            COMMAND ${CMAKE_COMMAND}
                "-DGAME=$<TARGET_FILE:${BDG_GAME_TARGET}>"
                "-DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_smoke.script"
                "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/editor_smoke"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_editor_smoke.cmake")
    endif()
endif()
