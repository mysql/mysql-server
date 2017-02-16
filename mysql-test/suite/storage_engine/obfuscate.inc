let $storage_engine_search_string = ENGINE=$storage_engine;

--replace_result '  ' ' ' ' ,' ',' ' )' ')' '( ' '(' $default_tbl_opts <CUSTOM_TABLE_OPTIONS> $int_indexed_col <INT_COLUMN> $char_indexed_col <CHAR_COLUMN> $int_col <INT_COLUMN> $char_col <CHAR_COLUMN>  $default_col_indexed_opts <CUSTOM_COL_OPTIONS> $default_col_opts <CUSTOM_COL_OPTIONS> $storage_engine_search_string ENGINE=<STORAGE_ENGINE> $default_index <CUSTOM_INDEX>

