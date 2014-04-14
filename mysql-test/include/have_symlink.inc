# Several partition-related tests include this file, because Valgrind/Purify
# builds previously disabled symlink support;
# Some partition functionality depends on symlink support.

-- require r/have_symlink.require

disable_query_log;
show variables like 'have_symlink';
enable_query_log;
