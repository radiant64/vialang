cmake_minimum_required(VERSION 3.12)

file(READ ${DATA_FILE} HEX_DATA HEX ENCODING UTF-8)
if(APPEND_ZERO)
    string(CONCAT HEX_DATA ${HEX_DATA} "00")
endif()
string(REGEX REPLACE "([A-Fa-f0-9][A-Fa-f0-9])" "0x\\1," TEMP ${HEX_DATA})
string(REGEX REPLACE ",$" "" HEX_BYTES ${TEMP})
string(REGEX MATCHALL
    "0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,"
    LINES
    ${HEX_BYTES}
)
string(CONCAT LINES_JOINED ${LINES})
list(TRANSFORM LINES APPEND "\n")
list(TRANSFORM LINES PREPEND "    ")
string(LENGTH ${LINES_JOINED} JOINED_LENGTH)
string(SUBSTRING ${HEX_BYTES} ${JOINED_LENGTH} -1 REMAINDER)
list(APPEND LINES "    ${REMAINDER}")

set(HEADER "#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern \"C\" {
#endif

extern const char* ${VAR_NAME}\;
extern const size_t ${VAR_NAME}_size\;

#ifdef __cplusplus
}
#endif
")

set(IMPL "#include \"${HEADER_FILE}\"

const char ${VAR_NAME}_impl[] = {
" ${LINES} "
}\;

const char* ${VAR_NAME} = ${VAR_NAME}_impl\;
const size_t ${VAR_NAME}_size = sizeof(${VAR_NAME}_impl)\;
")

file(WRITE ${HEADER_FILE} ${HEADER})
file(WRITE ${IMPL_FILE} ${IMPL})

