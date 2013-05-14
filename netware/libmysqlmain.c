/*
  Copyright (c) 2002 Novell, Inc. All Rights Reserved. 

  This program is free software; you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation; either version 2 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
  MA 02110-1301, USA
*/ 

#include "my_global.h"

void init_available_charsets(void);

/* this function is required so that global memory is allocated against this
library nlm, and not against a paticular client */
int _NonAppStart(void *NLMHandle, void *errorScreen, const char *commandLine,
  const char *loadDirPath, size_t uninitializedDataLength,
  void *NLMFileHandle, int (*readRoutineP)( int conn, void *fileHandle,
  size_t offset, size_t nbytes, size_t *bytesRead, void *buffer ),
  size_t customDataOffset, size_t customDataSize, int messageCount,
  const char **messages)
{
  mysql_server_init(0, NULL, NULL);
  
  init_available_charsets();

  return 0;
}

