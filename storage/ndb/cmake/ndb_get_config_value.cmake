MACRO(NDB_GET_CONFIG_VALUE keyword var)
 IF(NOT ${var})
   # Read the line which contains the keyword
   FILE (STRINGS ${NDB_SOURCE_DIR}/VERSION str
         REGEX "^[ ]*${keyword}=")
   IF(str)
     # Remove the keyword=
     STRING(REPLACE "${keyword}=" "" str ${str})
     # Remove whitespace
     STRING(REGEX REPLACE "[ ].*" "" str "${str}")
     SET(${var} ${str})
   ENDIF()
 ENDIF()
ENDMACRO()
