/*
  Extract properties of a windows service binary path
*/
#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h> 
typedef struct mysqld_service_properties_st
{
  char mysqld_exe[MAX_PATH];
  char inifile[MAX_PATH];
  char datadir[MAX_PATH];
  int  version_major;
  int  version_minor;
  int  version_patch;
} mysqld_service_properties;

extern int get_mysql_service_properties(const wchar_t *bin_path, 
  mysqld_service_properties *props);

#ifdef __cplusplus
}
#endif
