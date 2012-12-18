
# Options from top level wscript:
# --mysql, --interactive

# my_lib = mysql_path + "/lib/"
# my_inc = mysql_path + "/include/"
# IF mysql exists under my_lib THEN my_lib += "mysql"
# IF mysql/mysql.h exists under my_inc THEN my_inc += "mysql/"
# ndb_inc is my_inc + "/storage/ndb"

# Then we recurse to 

# We set CPPPATH to my_inc + ndb_inc + ndb_inc+"/ndbapi" + "../include"
# We set CXXFLAGS to -Wall
# We set LIBPATH to my_lib

{
 
  'targets': 
  [ 
    {
      'target_name': "ndb_adapter",

      'include_dirs':
      [
        '<(mysql_path)/include',
        '<(mysql_path)/include/mysql',
        '<(mysql_path)/include/storage/ndb',
        '<(mysql_path)/include/storage/ndb/ndbapi',
        'include'
      ],
        
      'sources': 
      [
         "src/node_module.cpp",
         "src/async_common.cpp",
         "src/unified_debug.cpp",
         "src/Record.cpp",
         "src/Operation.cpp",
         "src/DBOperationHelper.cpp",
         "src/DBSessionImpl.cpp",
         "src/DBDictionaryImpl.cpp",
         "src/Record_wrapper.cpp",
         "src/Operation_wrapper.cpp",
         "src/Ndb_init_wrapper.cpp",
         "src/Ndb_cluster_connection_wrapper.cpp",
         "src/Ndb_wrapper.cpp",
         "src/NdbError_wrapper.cpp",
         "src/NdbTransaction_wrapper.cpp",
         "src/NdbOperation_wrapper.cpp"
      ],

      'libraries':
      [
        "-lndbclient",
        "-lmysqlclient"
      ],

      'conditions': 
      [
        ['OS=="win"', 
          # Windows 
          {
            'msvs_settings':
            [
              'AdditionalLibraryDirectories' : "-L<(mysql_path)/lib"
            ]
          },
          # Not Windows
          {
            'sources' : 
            [
               "src/mysqlclient_wrapper.cpp"
            ],
            'libraries':
            [
              "-L<(mysql_path)/lib",
            ]
          }
        ]
      ] 
      # End of conditions
    }
  ]
}

# Then recurse into test

# In test there are 3 targets
# mapper: c-api.cc api-mapper.cc
# outermapper: outer-mapper.cc
# debug_dlopen: debug_dlopen.cpp
