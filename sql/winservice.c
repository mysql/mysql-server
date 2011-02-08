/*
  Get Properties of an existing mysqld Windows service 
*/

#include <windows.h>
#include <winsvc.h>
#include "winservice.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/*
  Get version from an executable file
*/
void get_file_version(const char *path, int *major, int *minor, int *patch)
{
  DWORD version_handle;
  char *ver= 0;
  VS_FIXEDFILEINFO info;
  UINT len;
  DWORD size;
  void *p;
  *major= *minor= *patch= 0;

  size= GetFileVersionInfoSize(path, &version_handle);
  if (size == 0) 
    return;
  ver= (char *)malloc(size);
  if(!GetFileVersionInfo(path, version_handle, size, ver))
    goto end;

  if(!VerQueryValue(ver,"\\",&p,&len))
    goto end;
  memcpy(&info,p ,sizeof(VS_FIXEDFILEINFO));

  *major= (info.dwFileVersionMS & 0xFFFF0000) >> 16;
  *minor= (info.dwFileVersionMS & 0x0000FFFF);
  *patch= (info.dwFileVersionLS & 0xFFFF0000) >> 16;
end:
  free(ver);
}

void normalize_path(char *path, size_t size)
{
  char buf[MAX_PATH];
  if (*path== '"')
  {
    char *p;
    strcpy_s(buf, MAX_PATH, path+1);
    p= strchr(buf, '"');
    if (p) 
      *p=0;
  }
  else
    strcpy_s(buf, MAX_PATH,  path);
  GetFullPathName(buf, MAX_PATH, buf, NULL);
  strcpy_s(path, size,  buf);
}

/*
  Retrieve some properties from windows mysqld service binary path.
  We're interested in ini file location and datadir, and also in version of 
  the data. We tolerate missing mysqld.exe.

  Note that this function carefully avoids using mysql libraries (e.g dbug), 
  since it is  used in unusual environments (windows installer, MFC), where we
  do not have much control over how threads are created and destroyed, so we 
  cannot assume MySQL thread initilization here.
*/
int get_mysql_service_properties(const wchar_t *bin_path, 
  mysqld_service_properties *props)
{
  int numargs;
  wchar_t mysqld_path[MAX_PATH + 4];
  wchar_t *file_part;
  wchar_t **args= NULL;
  int retval= 1;
  BOOL have_inifile;

  props->datadir[0]= 0;
  props->inifile[0]= 0;
  props->mysqld_exe[0]= 0;
  props->version_major= 0;
  props->version_minor= 0;
  props->version_patch= 0;

  args= CommandLineToArgvW(bin_path, &numargs);
  if(numargs == 2)
  {
    /*
      There are rare cases where service config does not have 
      --defaults-filein the binary parth . There services were registered with 
      plain mysqld --install, the data directory is next to "bin" in this case.
      Service name (second parameter) must be MySQL.
    */
    if(wcscmp(args[1], L"MySQL") != 0)
      goto end;
    have_inifile= FALSE;
  }
  else if(numargs == 3)
  {
    have_inifile= TRUE;
  }
  else
  {
    goto end;
  }

  if(have_inifile && wcsncmp(args[1], L"--defaults-file=", 16) != 0)
    goto end;

  GetFullPathNameW(args[0], MAX_PATH, mysqld_path, &file_part);

  if(wcsstr(mysqld_path, L".exe") == NULL)
    wcscat(mysqld_path, L".exe");

  if(wcsicmp(file_part, L"mysqld.exe") != 0 && 
    wcsicmp(file_part, L"mysqld.exe") != 0 &&
    wcsicmp(file_part, L"mysqld-nt.exe") != 0)
  {
    /* The service executable is not mysqld. */
    goto end;
  }

  wcstombs(props->mysqld_exe, mysqld_path, MAX_PATH);
  /* If mysqld.exe exists, try to get its version from executable */
  if (GetFileAttributes(props->mysqld_exe) != INVALID_FILE_ATTRIBUTES)
  {
     get_file_version(props->mysqld_exe, &props->version_major, 
      &props->version_minor, &props->version_patch);
  }
  if (have_inifile)
  {
    /* Easy case, we have --defaults-file in service definition. */
    wcstombs(props->inifile, args[1]+16, MAX_PATH);
    normalize_path(props->inifile, MAX_PATH);
    if (GetFileAttributes(props->inifile) == INVALID_FILE_ATTRIBUTES)
      goto end;
    GetPrivateProfileString("mysqld", "datadir", NULL, props->datadir, MAX_PATH, 
      props->inifile);
  }
  else
  {
    /*
      Hard, although a rare case, we're guessing datadir and defaults-file.
      On Windows, defaults-file is traditionally install-root\my.ini 
      and datadir is install-root\data
    */
    char install_root[MAX_PATH];
    int i;
    char *p;

    /*
      Get the  install root(parent of bin directory where mysqld.exe)
      is located.
    */
    strcpy_s(install_root, MAX_PATH, props->mysqld_exe);
    for (i=0; i< 2; i++)
    {
      p= strrchr(install_root, '\\');
      if(!p)
        goto end;
      *p= 0;
    }

    /* Look for my.ini, my.cnf in the install root */
    sprintf_s(props->inifile, MAX_PATH, "%s\\my.ini", install_root);
    if (GetFileAttributes(props->inifile) == INVALID_FILE_ATTRIBUTES)
    {
      sprintf_s(props->inifile, MAX_PATH, "%s\\my.cnf", install_root);
    }
    if (GetFileAttributes(props->inifile) != INVALID_FILE_ATTRIBUTES)
    {
      /* Ini file found, get datadir from there */
      GetPrivateProfileString("mysqld", "datadir", NULL, props->datadir,
        MAX_PATH, props->inifile);
    }
    else
    {
      /* No ini file */
      props->inifile[0]= 0;
    }

    /* Try datadir in install directory.*/
    if (props->datadir[0] == 0)
    {
      sprintf_s(props->datadir, MAX_PATH, "%s\\data", install_root);
    }
  }

  if (props->datadir[0])
  {
    normalize_path(props->datadir, MAX_PATH);
    /* Check if datadir really exists */
    if (GetFileAttributes(props->datadir) == INVALID_FILE_ATTRIBUTES)
      goto end;
  }
  else
  {
    /* There is no datadir in ini file,  bail out.*/
    goto end;
  }

  /*
    If version could not be determined so far, try mysql_upgrade_info in 
    database directory.
  */
  if(props->version_major == 0)
  {
    char buf[MAX_PATH];
    FILE *mysql_upgrade_info;

    sprintf_s(buf, MAX_PATH, "%s\\mysql_upgrade_info", props->datadir);
    mysql_upgrade_info= fopen(buf, "r");
    if(mysql_upgrade_info)
    {
      if (fgets(buf, MAX_PATH, mysql_upgrade_info))
      {
        int major,minor,patch;
        if (sscanf(buf, "%d.%d.%d", &major, &minor, &patch) == 3)
        {
          props->version_major= major;
          props->version_minor= minor;
          props->version_patch= patch;
        }
      }
    }
  }
  retval = 0;
end:
  LocalFree((HLOCAL)args);
  return retval;
}