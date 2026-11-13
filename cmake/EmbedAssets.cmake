# Genere un fichier C contenant la police et les niveaux de la campagne.
# Appele en mode script : cmake -DOUTPUT=... -DFONT=... -DLEVELS="a;b;c" -P EmbedAssets.cmake

function(bdg_hex_array path varname out)
    file(READ "${path}" hex HEX)
    string(LENGTH "${hex}" hexlen)
    math(EXPR size "${hexlen} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," body "${hex}")
    # retour a la ligne tous les 24 octets pour garder un fichier lisible
    string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],){24})" "\\1\n    " body "${body}")
    set(${out} "static const unsigned char ${varname}[] = {\n    ${body}0x00\n};\n" PARENT_SCOPE)
    set(${varname}_SIZE ${size} PARENT_SCOPE)
endfunction()

set(src "/* Fichier genere par cmake/EmbedAssets.cmake - ne pas modifier. */\n")
string(APPEND src "#include \"embedded.h\"\n\n")

bdg_hex_array("${FONT}" bdg_font_data arr)
string(APPEND src "${arr}")
string(APPEND src "const unsigned char *const g_font_data = bdg_font_data;\n")
string(APPEND src "const size_t g_font_size = ${bdg_font_data_SIZE};\n\n")

set(table "")
set(count 0)
foreach(lvl IN LISTS LEVELS)
    get_filename_component(name "${lvl}" NAME)
    bdg_hex_array("${lvl}" bdg_level_${count} arr)
    string(APPEND src "${arr}")
    string(APPEND table "    { \"${name}\", bdg_level_${count}, ${bdg_level_${count}_SIZE} },\n")
    math(EXPR count "${count} + 1")
endforeach()

if(count EQUAL 0)
    string(APPEND src "const EmbeddedFile g_campaign_levels[1] = { { 0, 0, 0 } };\n")
else()
    string(APPEND src "const EmbeddedFile g_campaign_levels[] = {\n${table}};\n")
endif()
string(APPEND src "const int g_campaign_level_count = ${count};\n")

file(WRITE "${OUTPUT}.tmp" "${src}")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
