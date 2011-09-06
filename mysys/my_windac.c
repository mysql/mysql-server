/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysys_priv.h"
#include "m_string.h"
#ifdef __WIN__

/* Windows NT/2000 discretionary access control utility functions. */

/*
  Check if the operating system is built on NT technology.

  RETURN
    0   Windows 95/98/Me
    1   otherwise
*/

static my_bool is_nt()
{
  return GetVersion() < 0x80000000;
}

/*
  Auxilary structure to store pointers to the data which we need to keep
  around while SECURITY_ATTRIBUTES is in use.
*/

typedef struct st_my_security_attr
{
  PSID everyone_sid;
  PACL dacl;
} My_security_attr;


/*
  Allocate and initialize SECURITY_ATTRIBUTES setting up access
  rights for the owner and group `Everybody'.

  SYNOPSIS
    my_security_attr_create()
    psa                [OUT] pointer to store the pointer to SA in
    perror             [OUT] pointer to store error message if there was an
                             error
    owner_rights       [IN]  access rights for the owner
    everyone_rights    [IN]  access rights for group Everybody

  DESCRIPTION
    Set up the security attributes to provide clients with sufficient
    access rights to a kernel object. We need this function
    because if we simply grant all access to everybody (by installing
    a NULL DACL) a mailicious user can attempt a denial of service
    attack by taking ownership over the kernel object. Upon successful
    return `psa' contains a pointer to SECUIRITY_ATTRIBUTES that can be used
    to create kernel objects with proper access rights.

  RETURN
    0  success, psa is 0 or points to a valid SA structure,
       perror is left intact
   !0  error, SA is set to 0, error message is stored in perror
*/

int my_security_attr_create(SECURITY_ATTRIBUTES **psa, const char **perror,
                            DWORD owner_rights, DWORD everyone_rights)
{
  /* Top-level SID authority */
  SID_IDENTIFIER_AUTHORITY world_auth= SECURITY_WORLD_SID_AUTHORITY;
  PSID everyone_sid= 0;
  HANDLE htoken= 0;
  SECURITY_ATTRIBUTES *sa= 0;
  PACL dacl= 0;
  DWORD owner_token_length, dacl_length;
  SECURITY_DESCRIPTOR *sd;
  PTOKEN_USER owner_token;
  PSID owner_sid;
  My_security_attr *attr;

  if (! is_nt())
  {
    *psa= 0;
    return 0;
  }

  /*
    Get SID of Everyone group. Easier to retrieve all SIDs each time
    this function is called than worry about thread safety.
  */
  if (! AllocateAndInitializeSid(&world_auth, 1, SECURITY_WORLD_RID,
                                 0, 0, 0, 0, 0, 0, 0, &everyone_sid))
  {
    *perror= "Failed to retrieve the SID of Everyone group";
    goto error;
  }

  /*
    Get SID of the owner. Using GetSecurityInfo this task can be done
    in just one call instead of five, but GetSecurityInfo declared in
    aclapi.h, so I hesitate to use it.
    SIC: OpenThreadToken works only if there is an active impersonation
    token, hence OpenProcessToken is used.
  */
  if (! OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &htoken))
  {
    *perror= "Failed to retrieve thread access token";
    goto error;
  }
  GetTokenInformation(htoken, TokenUser, 0, 0, &owner_token_length);

  if (! my_multi_malloc(MYF(MY_WME),
                        &sa, ALIGN_SIZE(sizeof(SECURITY_ATTRIBUTES)) +
                             sizeof(My_security_attr),
                        &sd, sizeof(SECURITY_DESCRIPTOR),
                        &owner_token, owner_token_length,
                        0))
  {
    *perror= "Failed to allocate memory for SECURITY_ATTRIBUTES";
    goto error;
  }
  memset(owner_token, 0, owner_token_length);
  if (! GetTokenInformation(htoken, TokenUser, owner_token,
                            owner_token_length, &owner_token_length))
  {
    *perror= "GetTokenInformation failed";
    goto error;
  }
  owner_sid= owner_token->User.Sid;

  if (! IsValidSid(owner_sid))
  {
    *perror= "IsValidSid failed";
    goto error;
  }

  /* Calculate the amount of memory that must be allocated for the DACL */
  dacl_length= sizeof(ACL) + (sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD)) * 2 +
               GetLengthSid(everyone_sid) + GetLengthSid(owner_sid);

  /* Create an ACL */
  if (! (dacl= (PACL) my_malloc(dacl_length, MYF(MY_ZEROFILL|MY_WME))))
  {
    *perror= "Failed to allocate memory for DACL";
    goto error;
  }
  if (! InitializeAcl(dacl, dacl_length, ACL_REVISION))
  {
    *perror= "Failed to initialize DACL";
    goto error;
  }
  if (! AddAccessAllowedAce(dacl, ACL_REVISION, everyone_rights, everyone_sid))
  {
    *perror= "Failed to set up DACL";
    goto error;
  }
  if (! AddAccessAllowedAce(dacl, ACL_REVISION, owner_rights, owner_sid))
  {
    *perror= "Failed to set up DACL";
    goto error;
  }
  if (! InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
  {
    *perror= "Could not initialize security descriptor";
    goto error;
  }
  if (! SetSecurityDescriptorDacl(sd, TRUE, dacl, FALSE))
  {
    *perror= "Failed to install DACL";
    goto error;
  }

  sa->nLength= sizeof(*sa);
  sa->bInheritHandle= TRUE;
  sa->lpSecurityDescriptor= sd;
  /* Save pointers to everyone_sid and dacl to be able to clean them up */
  attr= (My_security_attr*) (((char*) sa) + ALIGN_SIZE(sizeof(*sa)));
  attr->everyone_sid= everyone_sid;
  attr->dacl= dacl;
  *psa= sa;

  CloseHandle(htoken);
  return 0;
error:
  if (everyone_sid)
    FreeSid(everyone_sid);
  if (htoken)
    CloseHandle(htoken);
  my_free(sa);
  my_free(dacl);
  *psa= 0;
  return 1;
}

/*
  Cleanup security attributes freeing used memory.

  SYNOPSIS
    my_security_attr_free()
    sa   security attributes
*/

void my_security_attr_free(SECURITY_ATTRIBUTES *sa)
{
  if (sa)
  {
    My_security_attr *attr= (My_security_attr*)
                            (((char*)sa) + ALIGN_SIZE(sizeof(*sa)));
    FreeSid(attr->everyone_sid);
    my_free(attr->dacl);
    my_free(sa);
  }
}

#endif /* __WIN__ */
