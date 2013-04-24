# Some help:
#  Microsoft linker options:  
#     http://msdn.microsoft.com/en-us/library/4khtbfyf.aspx
#  
#  Misc.:
#     https://github.com/mapnik/node-mapnik/issues/74 --  /FORCE:MULTIPLE
#     https://github.com/TooTallNate/node-gyp/wiki/%22binding.gyp%22-files-out-in-the-wild
#     https://github.com/TooTallNate/node-gyp/blob/master/addon.gypi

{
 
  'targets': 
  [ 
    {
      'target_name': "ndb_adapter",

      'include_dirs':
      [
        '<(mysql_path)/include',
        '<(mysql_path)/include/mysql',
        '<(mysql_path)/include/mysql/storage/ndb',
        '<(mysql_path)/include/mysql/storage/ndb/ndbapi',
        '<(mysql_path)/include/storage/ndb',
        '<(mysql_path)/include/storage/ndb/ndbapi',
        'Adapter/impl/include'
      ],
        
      'sources': 
      [
         "Adapter/impl/src/node_module.cpp",
         "Adapter/impl/src/async_common.cpp",
         "Adapter/impl/src/unified_debug.cpp",
         "Adapter/impl/src/Record.cpp",
         "Adapter/impl/src/AsyncNdbContext.cpp",
         "Adapter/impl/src/DBOperationHelper.cpp",
         "Adapter/impl/src/IndexBoundHelper.cpp",
         "Adapter/impl/src/DBScanHelper.cpp", 
         "Adapter/impl/src/DBDictionaryImpl.cpp",
         "Adapter/impl/src/NdbTypeEncoders.cpp",
         "Adapter/impl/src/NdbRecordObject.cpp",
         "Adapter/impl/src/ColumnProxy.cpp",
         "Adapter/impl/src/ColumnHandler.cpp",
         "Adapter/impl/src/ValueObject.cpp",
         "Adapter/impl/src/ScanImpl.cpp",

         "Adapter/impl/src/Record_wrapper.cpp",
         "Adapter/impl/src/Ndb_init_wrapper.cpp",
         "Adapter/impl/src/Ndb_util_wrapper.cpp",
         "Adapter/impl/src/Ndb_cluster_connection_wrapper.cpp",
         "Adapter/impl/src/NdbTransaction_wrapper.cpp",
         "Adapter/impl/src/Ndb_wrapper.cpp",
         "Adapter/impl/src/NdbError_wrapper.cpp",
         "Adapter/impl/src/NdbOperation_wrapper.cpp",
         "Adapter/impl/src/AsyncNdbContext_wrapper.cpp",
         "Adapter/impl/src/NdbInterpretedCode_wrapper.cpp",
         "Adapter/impl/src/NdbScanFilter_wrapper.cpp"
        ],

      'conditions': 
      [
        ['OS=="win"', 
          # Windows 
          {
            'libraries':
            [
              '-l<(mysql_path)/lib/ndbclient_static.lib',
              '-l<(mysql_path)/lib/mysqlclient.lib',
            ],
            'msvs_settings':
            {
              'VCLinkerTool':
                {
                  'AdditionalOptions': 
                  [
                    '/FORCE:MULTIPLE',
                    '/NODEFAULTLIB:LIBCMT'
                  ]
                }
            }
          },
          # Not Windows
          {
            'sources' : 
            [
               "Adapter/impl/src/mysqlclient_wrapper.cpp"
            ],
            'libraries':
            [
              "-L<(mysql_path)/lib",
              "-L<(mysql_path)/lib/mysql",
              "-lndbclient",
              "-lmysqlclient"
            ]
          }
        ]
      ] 
      # End of conditions
    }
  ]
}

