# Copyright (C) The Qt Company Ltd.
# Copyright (C) 2026 AwaSubaru
# SPDX-License-Identifier: LGPL-3.0-only
#
# This is a secondary development based on Qt Multimedia.

file(STRINGS "${INPUT_FILE}" LINES)
set(OUTPUT "")
foreach(LINE IN LISTS LINES)
    string(FIND "${LINE}" "prefer " POS)
    if(POS EQUAL -1)
        string(APPEND OUTPUT "${LINE}\n")
    endif()
endforeach()
file(WRITE "${OUTPUT_FILE}" "${OUTPUT}")
