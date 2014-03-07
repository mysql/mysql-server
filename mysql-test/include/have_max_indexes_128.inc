--source include/max_indexes.inc

if ($max_indexes != 128) {
  --skip Test needs a server built with --with-max-indexes=128
}
