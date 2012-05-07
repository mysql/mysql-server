include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(both_bins
  db-benchmark-test
  scanscan
  ptquery
  txncommit
  )

set(tokudbonly_bins
  scanrace
  multi-bench
  db-verify
  )

if(BDB_FOUND)
  foreach(bdb_bin ${both_bins})
    add_executable(${bdb_bin}-bdb ${bdb_bin}.c)
    set_property(TARGET ${bdb_bin}-bdb APPEND PROPERTY
      COMPILE_DEFINITIONS DIRSUF=bdb TOKU_ALLOW_DEPRECATED)
    set_target_properties(${bdb_bin}-bdb PROPERTIES
      INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../portability;${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(${bdb_bin}-bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
  endforeach(bdb_bin)
endif()

foreach(tokudb_bin ${both_bins} ${tokudb_bins})
  add_executable(${tokudb_bin}-tokudb ${tokudb_bin}.c)
  set_property(TARGET ${tokudb_bin}-tokudb APPEND PROPERTY
    COMPILE_DEFINITIONS TOKUDB)
  target_link_libraries(${tokudb_bin}-tokudb ${LIBTOKUPORTABILITY} ${LIBTOKUDB})
endforeach(tokudb_bin)
