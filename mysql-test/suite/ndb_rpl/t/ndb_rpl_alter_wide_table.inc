let $query = ALTER TABLE $table ALGORITHM=INPLACE;
let $col_idx = 1;
let $num_cols = 249;
while ($num_cols)
{
  let $query = $query,
      ADD COLUMN
        column_with_long_name_345678901234567890123456789012345678_$col_idx
        INT COLUMN_FORMAT DYNAMIC;
  dec $num_cols;
  inc $col_idx;
}
eval $query;
