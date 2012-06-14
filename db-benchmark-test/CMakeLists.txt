include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE)

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
      COMPILE_DEFINITIONS "DIRSUF=bdb;TOKU_ALLOW_DEPRECATED;ENVDIR=\"${bdb_bin}.c.bdb\"")
    set_target_properties(${bdb_bin}-bdb PROPERTIES
      INCLUDE_DIRECTORIES "${BDB_INCLUDE_DIR};${CMAKE_CURRENT_BINARY_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../toku_include;${CMAKE_CURRENT_SOURCE_DIR}/../portability;${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(${bdb_bin}-bdb ${LIBTOKUPORTABILITY} ${BDB_LIBRARIES})
  endforeach(bdb_bin)
endif()

foreach(tokudb_bin ${both_bins} ${tokudbonly_bins})
  add_executable(${tokudb_bin}-tokudb ${tokudb_bin}.c)
  set_property(TARGET ${tokudb_bin}-tokudb APPEND PROPERTY
    COMPILE_DEFINITIONS "TOKUDB;ENVDIR=\"${tokudb_bin}.c.tdb\"")
  add_space_separated_property(TARGET ${tokudb_bin}-tokudb COMPILE_FLAGS "-fvisibility=hidden -fPIE")
  add_space_separated_property(TARGET ${tokudb_bin}-tokudb LINK_FLAGS -pie)
  target_link_libraries(${tokudb_bin}-tokudb ${LIBTOKUDB} ${LIBTOKUPORTABILITY})
endforeach(tokudb_bin)
