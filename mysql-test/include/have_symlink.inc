# Several partition-related tests include this file, because Valgrind/Purify
# builds disable symlink support; some partition functionality depends on
# symlink support, and so does not test correctly under valgrind.  See the
# comment in mysqld.cc for the --symbolic-links option.

-- require r/have_symlink.require

disable_query_log;
show variables like 'have_symlink';
enable_query_log;
