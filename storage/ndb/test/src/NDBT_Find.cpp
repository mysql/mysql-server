/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <NdbEnv.h>
#include <NDBT_Find.hpp>

#ifndef _WIN32
#define PATH_SEPARATOR ":"
#else
#define PATH_SEPARATOR ";"
#endif

void NDBT_find_binary_from_path(BaseString &name, const char *binary_name,
                                const char *_path) {
  char *copy = strdup(_path);

  // Loop the list of paths and see if the binary exists
  char *n = copy;
  while (n != NULL) {
    char *s = n;
    n = strstr(s, PATH_SEPARATOR);
    if (n != NULL) {
      *n = 0;
      n += strlen(PATH_SEPARATOR);
    }
    BaseString path;
#ifndef _WIN32
    path.assfmt("%s/%s", s, binary_name);
#else
    path.assfmt("%s\\%s", s, binary_name);
#endif
    if (access(path.c_str(), F_OK) == 0) {
      // Success, found the binary. Convert path to absolute and return it
      char realpath_buf[PATH_MAX];
#ifndef _WIN32
      if (realpath(path.c_str(), realpath_buf) == NULL) {
        fprintf(stderr, "Could not convert '%s' to realpath\n", path.c_str());
        abort();
      }
#else
      int ret = GetFullPathName(path.c_str(), sizeof(realpath_buf),
                                realpath_buf, NULL);
      if (ret == 0 || ret >= sizeof(realpath_buf)) {
        fprintf(stderr, "Could not convert '%s' with GetFullPathName\n",
                path.c_str());
        abort();
      }
#endif

      name.assign(realpath_buf);
      free(copy);
      printf("found %s\n", name.c_str());
      return;
    }
  }

  // Failed to find the binary in any of the supplied paths
  fprintf(stderr, "Could not find '%s' in '%s'\n", binary_name, _path);
  free(copy);
  abort();
}

void NDBT_find_binary(BaseString &name, const char *binary_name,
                      const char *first_path, ...) {
  BaseString path(first_path);

  // Push all the different paths to a list
  const char *str = first_path;
  va_list args;
  va_start(args, first_path);
  while ((str = va_arg(args, const char *)) != NULL) {
    path.appfmt("%s%s", PATH_SEPARATOR, str);
  }
  va_end(args);

  NDBT_find_binary_from_path(name, binary_name, path.c_str());
}

extern const char *my_progname;

static void NDBT_find_executable_in_test_env(BaseString &path,
                                             const char *program) {
  char pathbuf[1024];

  /**
   * 1) avoid using dirname/basename since they are not around on WIN
   * 2) define single character SEP to be able to use strrchr
   */
  BaseString copy(my_progname);
#ifdef _WIN32
  char SEP = '\\';
#else
  char SEP = '/';
#endif
  char *basename = const_cast<char *>(strrchr(copy.c_str(), SEP));
  if (basename == 0) {
    /**
     * No directory part in argv[0]
     *   => found in $PATH => search for ndb_mgmd in $PATH
     */
    NDBT_find_binary(path, program,
                     NdbEnv_GetEnv("PATH", pathbuf, sizeof(pathbuf)), NULL);
  } else {
    /**
     * Directory part in argv[0] (e.g storage/ndb/test/ndbapi/testMgmd)
     *   => don't add $PATH
     *   => search in relative places relative argv[0]
     */
    *basename = 0;
#ifndef _WIN32
    const char *places[] = {"../../../../runtime_output_directory",
                            "../../src/mgmsrv",
                            "../storage/ndb/src/mgmsrv",
                            "../libexec",
                            "../sbin",
                            "../bin",
                            0};
#else
    const char *places[] = {"..\\..\\..\\..\\runtime_output_directory",
                            "..\\..\\src\\mgmsrv",
                            "..\\storage\\ndb\\src\\mgmsrv",
                            "..\\libexec",
                            "..\\sbin",
                            "..\\bin",
                            ".",
                            0};

#endif
    BaseString searchpath = "";
    for (int i = 0; places[i] != 0; i++) {
      searchpath.appfmt("%s%s%c%s", (i == 0 ? "" : PATH_SEPARATOR),
                        copy.c_str(), SEP, places[i]);
    }
    NDBT_find_binary(path, program, searchpath.c_str(), NULL);
  }
}

void NDBT_find_ndb_mgmd(BaseString &path) {
#ifndef _WIN32
  return NDBT_find_executable_in_test_env(path, "ndb_mgmd");
#else
  return NDBT_find_executable_in_test_env(path, "ndb_mgmd.exe");
#endif
}

void NDBT_find_ndbd(BaseString &path) {
#ifndef _WIN32
  return NDBT_find_executable_in_test_env(path, "ndbd");
#else
  return NDBT_find_executable_in_test_env(path, "ndbd.exe");
#endif
}

void NDBT_find_sign_keys(BaseString &path) {
#ifndef _WIN32
  return NDBT_find_executable_in_test_env(path, "ndb_sign_keys");
#else
  return NDBT_find_executable_in_test_env(path, "ndb_sign_keys.exe");
#endif
}
