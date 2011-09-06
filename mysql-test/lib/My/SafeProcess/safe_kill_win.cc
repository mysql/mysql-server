/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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


/*
  Utility program used to signal a safe_process it's time to shutdown

  Usage:
    safe_kill <pid>
*/

#include <windows.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

int main(int argc, const char** argv )
{
  DWORD pid= -1;
  HANDLE shutdown_event;
  char safe_process_name[32]= {0};
  int retry_open_event= 2;
  /* Ignore any signals */
  signal(SIGINT,   SIG_IGN);
  signal(SIGBREAK, SIG_IGN);
  signal(SIGTERM,  SIG_IGN);

  if (argc != 2) {
    fprintf(stderr, "safe_kill <pid>\n");
    exit(2);
  }
  pid= atoi(argv[1]);

  _snprintf(safe_process_name, sizeof(safe_process_name),
            "safe_process[%d]", pid);

  /* Open the event to signal */
  while ((shutdown_event=
          OpenEvent(EVENT_MODIFY_STATE, FALSE, safe_process_name)) == NULL)
  {
     /*
      Check if the process is alive, otherwise there is really
      no sense to retry the open of the event
     */
    HANDLE process;
    DWORD exit_code;
    process= OpenProcess(SYNCHRONIZE| PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process)
    {
      /* Already died */
      exit(1);
    }

    if (!GetExitCodeProcess(process,&exit_code))
    {
       fprintf(stderr,  "GetExitCodeProcess failed, pid= %d, err= %d\n",
         pid, GetLastError());
       exit(1);
    }

    if (exit_code != STILL_ACTIVE)
    {
       /* Already died */
       CloseHandle(process);
       exit(2);
    }

    CloseHandle(process);

    if (retry_open_event--)
      Sleep(100);
    else
    {
      fprintf(stderr, "Failed to open shutdown_event '%s', error: %d\n",
              safe_process_name, GetLastError());
      exit(3);
    }
  }

  if(SetEvent(shutdown_event) == 0)
  {
    fprintf(stderr, "Failed to signal shutdown_event '%s', error: %d\n",
            safe_process_name, GetLastError());
    CloseHandle(shutdown_event);
    exit(4);
  }
  CloseHandle(shutdown_event);
  exit(0);
}

