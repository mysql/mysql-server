# detect when we are being built as a subproject
if (NOT DEFINED MYSQL_PROJECT_NAME_DOCSTRING)
  install(
    FILES
      db-insert.c
      db-insert-multiple.c
      db-scan.c
      db-update.c
      Makefile
      README.examples
    DESTINATION
      examples
    )
endif ()