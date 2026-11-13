# Lance le jeu hors ecran avec un script d'entrees et verifie le niveau enregistre.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{XDG_DATA_HOME} "${WORK}")
set(ENV{SDL_VIDEO_DRIVER} "offscreen")
set(ENV{SDL_RENDER_DRIVER} "software")
set(ENV{SDL_AUDIO_DRIVER} "dummy")
execute_process(COMMAND "${GAME}" --editor --script "${SCRIPT}"
    RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "le jeu a echoue (code ${rc})")
endif()
set(lvl "${WORK}/johanbaskovec/boules-de-givre/levels/le-grand-pont.lvl")
if(NOT EXISTS "${lvl}")
    message(FATAL_ERROR "niveau non enregistre : ${lvl}")
endif()
file(READ "${lvl}" content)
foreach(expected
        "name \"Le grand pont\""
        "start 1 600 500"
        "platform 200 300 400"
        "ladder 340 300 500"
        "ball 150 150 4 left")
    string(FIND "${content}" "${expected}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "ligne attendue absente : ${expected}\n--- fichier ---\n${content}")
    endif()
endforeach()
message(STATUS "editeur OK : ${lvl}")
