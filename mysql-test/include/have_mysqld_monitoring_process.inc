###########################################################
# In on Unix, checks if mysqld_safe is installed in the path
# specified by MTR. mysqld_safe is obsolete on platforms
# that use systemd to monitor mysqld or windows services
############################################################

let $have_windows = `SELECT CONVERT(@@version_compile_os using latin1) IN ("Win32", "Win64", "Windows")`;

if (!$have_windows) {
  --source include/have_mysqld_safe.inc
}
