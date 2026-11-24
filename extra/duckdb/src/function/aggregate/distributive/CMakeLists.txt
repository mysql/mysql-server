add_library_unity(duckdb_aggr_distr OBJECT count.cpp first_last_any.cpp
                  minmax.cpp multi_count.cpp)
set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_aggr_distr>
    PARENT_SCOPE)
