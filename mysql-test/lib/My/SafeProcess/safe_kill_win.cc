/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/*
  Utility program used to signal a safe_process it's time to shutdown

  Usage:
    safe_kill <pid>
*/

#include <windows.h>
#include <stdio.h>

int main(int argc, const char** argv )
{
  DWORD pid= -1;
  HANDLE shutdown_event;
  char safe_process_name[32]= {0};
  int retry_open_event= 100;

  if (argc != 2) {
    fprintf(stderr, "safe_kill <pid>\n");
    exit(2);
  }
  pid= atoi(argv[1]);

  _snprintf(safe_process_name, sizeof(safe_process_name), "safe_process[%d]", pid);

  /* Open the event to signal */
  while ((shutdown_event=
          OpenEvent(EVENT_MODIFY_STATE, FALSE, safe_process_name)) == NULL)
  {
    fprintf(stderr, "Failed to open shutdown_event '%s', error: %d\n",
            safe_process_name, GetLastError());

    /* Just check to see if pid exists */
    HANDLE pid_handle= OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (pid_handle == NULL)
      fprintf(stderr, "Could not open process with pid %d, error: %d\n", pid);
	else
	  CloseHandle(pid_handle);

    if (retry_open_event--)
    {
      fprintf(stderr, "retrying...\n");
      Sleep(100); /* In milli seconds */
    }
    else
    {
      fprintf(stderr, "No more retries, exiting");
      exit(1);
    }
  }

  if(SetEvent(shutdown_event) == 0)
  {
    fprintf(stderr, "Failed to signal shutdown_event '%s', error: %d\n",
            safe_process_name, GetLastError());
    CloseHandle(shutdown_event);
    exit(1);
  }
  CloseHandle(shutdown_event);
  exit(0);
}

