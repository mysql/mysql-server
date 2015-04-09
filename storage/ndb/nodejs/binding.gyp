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
        'Adapter/impl/common/include',
        'Adapter/impl/ndb/include'
      ],
        
      'sources': 
      [
         "Adapter/impl/common/src/async_common.cpp",
         "Adapter/impl/common/src/unified_debug.cpp",
         "Adapter/impl/common/src/common_v8_values.cpp",

         "Adapter/impl/ndb/src/AsyncNdbContext_wrapper.cpp",
         "Adapter/impl/ndb/src/AsyncNdbContext.cpp",
         "Adapter/impl/ndb/src/BlobHandler.cpp",
         "Adapter/impl/ndb/src/ColumnHandler.cpp",
         "Adapter/impl/ndb/src/ColumnProxy.cpp",
         "Adapter/impl/ndb/src/DBDictionaryImpl.cpp",
         "Adapter/impl/ndb/src/DBOperationHelper.cpp",
         "Adapter/impl/ndb/src/DBOperationSet_wrapper.cpp",
         "Adapter/impl/ndb/src/DBOperationSet.cpp",
         "Adapter/impl/ndb/src/DBSessionImpl_wrapper.cpp",
         "Adapter/impl/ndb/src/DBSessionImpl.cpp",
         "Adapter/impl/ndb/src/DBTransactionContext_wrapper.cpp",
         "Adapter/impl/ndb/src/DBTransactionContext.cpp",
         "Adapter/impl/ndb/src/EncoderCharset.cpp",
         "Adapter/impl/ndb/src/IndexBoundHelper.cpp",
         "Adapter/impl/ndb/src/KeyOperation.cpp",
         "Adapter/impl/ndb/src/Ndb_cluster_connection_wrapper.cpp",
         "Adapter/impl/ndb/src/Ndb_init_wrapper.cpp",
         "Adapter/impl/ndb/src/Ndb_util_wrapper.cpp",
         "Adapter/impl/ndb/src/Ndb_wrapper.cpp",
         "Adapter/impl/ndb/src/NdbError_wrapper.cpp",
         "Adapter/impl/ndb/src/NdbInterpretedCode_wrapper.cpp",
         "Adapter/impl/ndb/src/NdbRecordObject.cpp",
         "Adapter/impl/ndb/src/NdbScanFilter_wrapper.cpp",
         "Adapter/impl/ndb/src/NdbTypeEncoders.cpp",
         "Adapter/impl/ndb/src/Record_wrapper.cpp",
         "Adapter/impl/ndb/src/Record.cpp",
         "Adapter/impl/ndb/src/ScanOperation_wrapper.cpp",
         "Adapter/impl/ndb/src/ScanOperation.cpp", 
         "Adapter/impl/ndb/src/ValueObject.cpp",
         "Adapter/impl/ndb/src/node_module.cpp"
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
               "Adapter/impl/ndb/src/mysqlclient_wrapper.cpp"
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

