/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/named_pipe.h"

#include <AclAPI.h>
#include <accctrl.h>
#include <errno.h>

#include <mysql/components/services/log_builtins.h>
#include "my_config.h"
#include "my_sys.h"
#include "mysql/strings/int2str.h"
#include "mysqld_error.h"
#include "nulls.h"
#include "sql/current_thd.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/sql_error.h"
#include "strxnmov.h"

bool is_existing_windows_group_name(const char *group_name) {
  // First, let's get a SID for the given group name...
  BYTE soughtSID[SECURITY_MAX_SID_SIZE] = {0};
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  char referencedDomainName[MAX_PATH];
  DWORD size_referencedDomainName = MAX_PATH;
  SID_NAME_USE sid_name_use;

  if (!LookupAccountName(nullptr, group_name, soughtSID, &size_sid,
                         referencedDomainName, &size_referencedDomainName,
                         &sid_name_use)) {
    return false;
  }

  // sid_name_use is SidTypeAlias when group_name is a local group
  if (sid_name_use != SidTypeAlias && sid_name_use != SidTypeWellKnownGroup) {
    return false;
  }
  return true;
}

/*
return false on successfully checking group, true on error.
*/
static bool check_windows_group_for_everyone(const char *group_name,
                                             bool *is_everyone_group) {
  *is_everyone_group = false;
  if (!group_name || group_name[0] == '\0') {
    return false;
  }

  if (strcmp(group_name, NAMED_PIPE_FULL_ACCESS_GROUP_EVERYONE) == 0) {
    *is_everyone_group = true;
    return false;
  } else {
    TCHAR last_error_msg[256];
    // First, let's get a SID for the given group name...
    BYTE soughtSID[SECURITY_MAX_SID_SIZE] = {0};
    DWORD size_sought_sid = SECURITY_MAX_SID_SIZE;
    BYTE worldSID[SECURITY_MAX_SID_SIZE] = {0};
    DWORD size_world_sid = SECURITY_MAX_SID_SIZE;
    char referencedDomainName[MAX_PATH];
    DWORD size_referencedDomainName = MAX_PATH;
    SID_NAME_USE sid_name_use;

    if (!LookupAccountName(nullptr, group_name, soughtSID, &size_sought_sid,
                           referencedDomainName, &size_referencedDomainName,
                           &sid_name_use)) {
      return false;
    }

    if (!CreateWellKnownSid(WinWorldSid, nullptr, worldSID, &size_world_sid)) {
      const DWORD last_error_num = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), nullptr);
      my_printf_error(
          ER_UNKNOWN_ERROR,
          "check_windows_group_for_everyone, CreateWellKnownSid failed: %s",
          MYF(0), last_error_msg);
      return true;
    }

    *is_everyone_group = EqualSid(soughtSID, worldSID);
    return false;
  }
}

bool is_valid_named_pipe_full_access_group(const char *group_name) {
  if (!group_name || group_name[0] == '\0') {
    return true;
  }

  bool is_everyone_group = false;

  if (check_windows_group_for_everyone(group_name, &is_everyone_group)) {
    return false;
  }

  if (is_everyone_group || is_existing_windows_group_name(group_name)) {
    return true;
  }
  return false;
}

// return false on success, true on failure.
bool my_security_attr_add_rights_to_group(SECURITY_ATTRIBUTES *psa,
                                          const char *group_name,
                                          DWORD group_rights) {
  TCHAR last_error_msg[256];
  // First, let's get a SID for the given group name...
  BYTE soughtSID[SECURITY_MAX_SID_SIZE] = {0};
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  char referencedDomainName[MAX_PATH];
  DWORD size_referencedDomainName = MAX_PATH;
  SID_NAME_USE sid_name_use;

  bool is_everyone_group = false;

  if (check_windows_group_for_everyone(group_name, &is_everyone_group)) {
    return true;
  }

  if (is_everyone_group) {
    sql_print_warning(ER_DEFAULT(WARN_NAMED_PIPE_ACCESS_EVERYONE), group_name);
    if (current_thd) {
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          WARN_NAMED_PIPE_ACCESS_EVERYONE,
                          ER_THD(current_thd, WARN_NAMED_PIPE_ACCESS_EVERYONE),
                          group_name);
    }
  }

  // Treat the NAMED_PIPE_FULL_ACCESS_GROUP_EVERYONE value
  // as a special case: we  convert it to the "world" SID
  if (strcmp(group_name, NAMED_PIPE_FULL_ACCESS_GROUP_EVERYONE) == 0) {
    if (!CreateWellKnownSid(WinWorldSid, nullptr, soughtSID, &size_sid)) {
      const DWORD last_error_num = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), nullptr);
      log_message(
          LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
          LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
          "my_security_attr_add_rights_to_group, CreateWellKnownSid failed",
          last_error_msg);
      return true;
    }
  } else {
    if (!LookupAccountName(nullptr, group_name, soughtSID, &size_sid,
                           referencedDomainName, &size_referencedDomainName,
                           &sid_name_use)) {
      const DWORD last_error_num = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), nullptr);
      log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                  "LookupAccountName failed", last_error_msg);
      return true;
    }

    // sid_name_use is SidTypeAlias when group_name is a local group
    if (sid_name_use != SidTypeAlias && sid_name_use != SidTypeWellKnownGroup) {
      log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                  "LookupAccountName failed", "unexpected sid_name_use");
      return true;
    }
  }

  PACL pNewDACL = nullptr;
  PACL pOldDACL = nullptr;
  BOOL dacl_present_in_descriptor = FALSE;
  BOOL dacl_defaulted = FALSE;
  if (!GetSecurityDescriptorDacl(psa->lpSecurityDescriptor,
                                 &dacl_present_in_descriptor, &pOldDACL,
                                 &dacl_defaulted) ||
      !dacl_present_in_descriptor) {
    const DWORD last_error_num = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, last_error_num,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                  sizeof(last_error_msg) / sizeof(TCHAR), nullptr);
    log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                "GetSecurityDescriptorDacl failed", last_error_msg);
    return true;
  }

  // Just because GetSecurityDescriptorDacl succeeded doesn't mean we're out of
  // the woods: a NULL value for pOldDACL is a bad/unexpected thing, as is
  // dacl_defaulted == TRUE
  if (pOldDACL == nullptr || dacl_defaulted) {
    log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                "Invalid DACL on named pipe",
                (pOldDACL == nullptr) ? "NULL DACL" : "Defaulted DACL");
    return true;
  }

  EXPLICIT_ACCESS ea;
  // Initialize an EXPLICIT_ACCESS structure for the new ACE.

  ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
  ea.grfAccessPermissions = group_rights;
  ea.grfAccessMode = SET_ACCESS;
  ea.grfInheritance = NO_INHERITANCE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.ptstrName = (LPSTR)soughtSID;

  // Create a new ACL that merges the new ACE
  // into the existing DACL.
  const DWORD dwRes = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
  if (ERROR_SUCCESS != dwRes) {
    char num_buff[20];
    longlong10_to_str(dwRes, num_buff, 10);
    log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                "SetEntriesInAcl to add group permissions failed", num_buff);
    return true;
  }

  // Apply the new DACL to the existing security descriptor...
  if (!SetSecurityDescriptorDacl(psa->lpSecurityDescriptor, TRUE, pNewDACL,
                                 FALSE)) {
    const DWORD last_error_num = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, last_error_num,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                  sizeof(last_error_msg) / sizeof(TCHAR), nullptr);
    log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                "SetSecurityDescriptorDacl failed", last_error_msg);
    return true;
  }

  return false;
}

/**
  Creates an instance of a named pipe and returns a handle.

  @param ppsec_attr  Output argument: on exit, points to the security
                     attributes for the pipe.
  @param buffer_size Number of bytes to reserve for input and output buffers.
  @param name        The name of the pipe.
  @param name_buf    Output argument: null-terminated concatenation of
                     "\\.\pipe\" and name.
  @param buflen      The size of name_buff.
  @param full_access_group_name The name of the local Windows group whose
                                members will have full access to the named
                                pipe.

  @returns           Pipe handle, or INVALID_HANDLE_VALUE in case of error.

  @note  The entire pipe name string can be up to 256 characters long.
         Pipe names are not case sensitive.
 */
HANDLE create_server_named_pipe(SECURITY_ATTRIBUTES **ppsec_attr,
                                DWORD buffer_size, const char *name,
                                char *name_buf, size_t buflen,
                                const char *full_access_group_name) {
  HANDLE ret_handle = INVALID_HANDLE_VALUE;
  TCHAR last_error_msg[256];

  strxnmov(name_buf, buflen - 1, "\\\\.\\pipe\\", name, NullS);
  const char *perror = nullptr;
  // Set up security for the named pipe to provide full access to the owner
  // and minimal read/write access to others.
  if (my_security_attr_create(ppsec_attr, &perror, NAMED_PIPE_OWNER_PERMISSIONS,
                              NAMED_PIPE_EVERYONE_PERMISSIONS) != 0) {
    log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                "my_security_attr_create", perror);
    return ret_handle;
  }

  if (full_access_group_name && full_access_group_name[0] != '\0') {
    if (my_security_attr_add_rights_to_group(
            *ppsec_attr, full_access_group_name,
            NAMED_PIPE_FULL_ACCESS_GROUP_PERMISSIONS)) {
      return ret_handle;
    }
  }

  ret_handle = CreateNamedPipe(
      name_buf,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
          FILE_FLAG_FIRST_PIPE_INSTANCE | WRITE_DAC,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
      buffer_size, buffer_size, NMPWAIT_USE_DEFAULT_WAIT, *ppsec_attr);

  if (ret_handle == INVALID_HANDLE_VALUE) {
    const DWORD last_error_num = GetLastError();

    if (last_error_num == ERROR_ACCESS_DENIED) {
      /*
        TODO: ER_NPIPE_PIPE_ALREADY_IN_USE is in the error-log range; refactor
        this to use LogErr() or log_message() instead of my_printf_error() once
        the logger has been refactored to simplify unit testing of expected
        errors.
      */
      my_printf_error(ER_NPIPE_PIPE_ALREADY_IN_USE,
                      ER_DEFAULT(ER_NPIPE_PIPE_ALREADY_IN_USE),
                      MYF(ME_FATALERROR), name);
    } else {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK,
                    nullptr, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), nullptr);
      char num_buff[20];
      longlong10_to_str(last_error_num, num_buff, 10);

      log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE, last_error_msg,
                  num_buff);
    }
  }

  return ret_handle;
}
