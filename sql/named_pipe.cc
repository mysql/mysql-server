/* Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <AclAPI.h>
#include <accctrl.h>
#include <errno.h>

#include <mysql/components/services/log_builtins.h>
#include "my_config.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/named_pipe.h"

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

bool is_valid_named_pipe_full_access_group(const char *group_name) {
  // Treat the DEFAULT_NAMED_PIPE_FULL_ACCESS_GROUP value
  // as a special case: we (later) convert it to the "world" SID
  if (!group_name || group_name[0] == '\0' ||
      strcmp(group_name, DEFAULT_NAMED_PIPE_FULL_ACCESS_GROUP) == 0 ||
      is_existing_windows_group_name(group_name)) {
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

  // Treat the DEFAULT_NAMED_PIPE_FULL_ACCESS_GROUP value
  // as a special case: we  convert it to the "world" SID
  if (strcmp(group_name, DEFAULT_NAMED_PIPE_FULL_ACCESS_GROUP) == 0) {
    if (!CreateWellKnownSid(WinWorldSid, NULL, soughtSID, &size_sid)) {
      DWORD last_error_num = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), NULL);
      log_message(
          LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
          LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
          "my_security_attr_add_rights_to_group, CreateWellKnownSid failed",
          last_error_msg);
      return true;
    }
  } else {
    if (!LookupAccountName(NULL, group_name, soughtSID, &size_sid,
                           referencedDomainName, &size_referencedDomainName,
                           &sid_name_use)) {
      DWORD last_error_num = GetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), NULL);
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

  PACL pNewDACL = NULL;
  PACL pOldDACL = NULL;
  BOOL dacl_present_in_descriptor = FALSE;
  BOOL dacl_defaulted = FALSE;
  if (!GetSecurityDescriptorDacl(psa->lpSecurityDescriptor,
                                 &dacl_present_in_descriptor, &pOldDACL,
                                 &dacl_defaulted) ||
      !dacl_present_in_descriptor) {
    DWORD last_error_num = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, last_error_num,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                  sizeof(last_error_msg) / sizeof(TCHAR), NULL);
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
  DWORD dwRes = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
  if (ERROR_SUCCESS != dwRes) {
    char num_buff[20];
    int10_to_str(dwRes, num_buff, 10);
    log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE,
                "SetEntriesInAcl to add group permissions failed", num_buff);
    return true;
  }

  // Apply the new DACL to the existing security descriptor...
  if (!SetSecurityDescriptorDacl(psa->lpSecurityDescriptor, TRUE, pNewDACL,
                                 FALSE)) {
    DWORD last_error_num = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, last_error_num,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                  sizeof(last_error_msg) / sizeof(TCHAR), NULL);
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
    DWORD last_error_num = GetLastError();

    if (last_error_num == ERROR_ACCESS_DENIED) {
      log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, ER_NPIPE_PIPE_ALREADY_IN_USE, name);
    } else {
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK,
                    NULL, last_error_num,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), last_error_msg,
                    sizeof(last_error_msg) / sizeof(TCHAR), NULL);
      char num_buff[20];
      int10_to_str(last_error_num, num_buff, 10);

      log_message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, (longlong)ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, ER_NPIPE_CANT_CREATE, last_error_msg,
                  num_buff);
    }
  }

  return ret_handle;
}
