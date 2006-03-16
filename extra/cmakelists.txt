SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DSAFEMALLOC -DSAFE_MUTEX")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

ADD_EXECUTABLE(comp_err comp_err.c)
TARGET_LINK_LIBRARIES(comp_err dbug mysys strings wsock32)

GET_TARGET_PROPERTY(COMP_ERR_EXE comp_err LOCATION)

ADD_CUSTOM_COMMAND(OUTPUT ${PROJECT_SOURCE_DIR}/include/mysqld_error.h
                   COMMAND ${COMP_ERR_EXE}
                    --charset=${PROJECT_SOURCE_DIR}/sql/share/charsets 
                    --out-dir=${PROJECT_SOURCE_DIR}/sql/share/
                    --header_file=${PROJECT_SOURCE_DIR}/include/mysqld_error.h 
                    --name_file=${PROJECT_SOURCE_DIR}/include/mysqld_ername.h
                    --state_file=${PROJECT_SOURCE_DIR}/include/sql_state.h
                    --in_file=${PROJECT_SOURCE_DIR}/sql/share/errmsg.txt
                   DEPENDS comp_err ${PROJECT_SOURCE_DIR}/sql/share/errmsg.txt)

ADD_CUSTOM_TARGET(GenError
                  ALL
                  DEPENDS ${PROJECT_SOURCE_DIR}/include/mysqld_error.h)

ADD_EXECUTABLE(my_print_defaults my_print_defaults.c)
TARGET_LINK_LIBRARIES(my_print_defaults strings mysys dbug taocrypt odbc32 odbccp32 wsock32)

ADD_EXECUTABLE(perror perror.c)
TARGET_LINK_LIBRARIES(perror strings mysys dbug wsock32)

ADD_EXECUTABLE(replace replace.c)
TARGET_LINK_LIBRARIES(replace strings mysys dbug wsock32)
