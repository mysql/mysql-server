
# Options from top level wscript:
# --mysql, --interactive

# my_lib = mysql_path + "/lib/"
# my_inc = mysql_path + "/include/"
# IF mysql exists under my_lib THEN my_lib += "mysql"
# IF mysql/mysql.h exists under my_inc THEN my_inc += "mysql/"
# ndb_inc is my_inc + "/storage/ndb"

# Then we recurse to Adapter/impl

# We set CPPPATH to my_inc + ndb_inc + ndb_inc+"/ndbapi" + "../include"
# We set CXXFLAGS to -Wall
# We set LIBPATH to my_lib

{
  'variables': [
    'mysql_path' : "/Users/jdd/bzr-repo/install-tests/7.2-working",    
  ],
 
  'targets': [
  {
      'target_name': "ndb_adapter",

      'include_dirs': 
      [
        '<(mysql_path)/include',
        '<(mysql_path)/include/mysql',
        '<(mysql_path)/include/storage/ndb',
        'Adapter/impl/include'
      ],
        
      'sources': 
      [
         "Adapter/impl/src/node_module.cpp",
         "Adapter/impl/src/async_common.cpp",
         "Adapter/impl/src/unified_debug.cpp",
         "Adapter/impl/src/Record.cpp",
         "Adapter/impl/src/Operation.cpp",
         "Adapter/impl/src/DBOperationHelper.cpp ",
         "Adapter/impl/src/DBSessionImpl.cpp",
         "Adapter/impl/src/DBDictionaryImpl.cpp",
         "Adapter/impl/src/Record_wrapper.cpp",
         "Adapter/impl/src/Operation_wrapper.cpp",
         "Adapter/impl/src/Ndb_init_wrapper.cpp",
         "Adapter/impl/src/Ndb_cluster_connection_wrapper.cpp",
         "Adapter/impl/src/Ndb_wrapper.cpp",
         "Adapter/impl/src/NdbError_wrapper.cpp",
         "Adapter/impl/src/NdbTransaction_wrapper.cpp",
         "Adapter/impl/src/NdbOperation_wrapper.cpp",
         "Adapter/impl/src/mysqlclient_wrapper.cpp"
      ],

      'libraries':
      [
        "-lndbclient", 
        "-lmysqlclient"
      ],
  }
  ]
}

# Then recurse into test

# In test there are 3 targets
# mapper: c-api.cc api-mapper.cc
# outermapper: outer-mapper.cc
# debug_dlopen: debug_dlopen.cpp
