/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* readline for batch mode */

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include "my_readline.h"

static bool init_line_buffer(LINE_BUFFER *buffer,File file,ulong size,
			    ulong max_size);
static bool init_line_buffer_from_string(LINE_BUFFER *buffer,my_string str);
static uint fill_buffer(LINE_BUFFER *buffer);
static char *intern_read_line(LINE_BUFFER *buffer,ulong *out_length);


LINE_BUFFER *batch_readline_init(ulong max_size,FILE *file)
{
  LINE_BUFFER *line_buff;
  if (!(line_buff=(LINE_BUFFER*) my_malloc(sizeof(*line_buff),MYF(MY_WME))))
    return 0;
  if (init_line_buffer(line_buff,fileno(file),IO_SIZE,max_size))
  {
    my_free((char*) line_buff,MYF(0));
    return 0;
  }
  return line_buff;
}


char *batch_readline(LINE_BUFFER *line_buff)
{
  char *pos;
  ulong out_length;

  if (!(pos=intern_read_line(line_buff,&out_length)))
    return 0;
  if (out_length && pos[out_length-1] == '\n')
    out_length--;				/* Remove '\n' */
  line_buff->read_length=out_length;
  pos[out_length]=0;
  return pos;
}


void batch_readline_end(LINE_BUFFER *line_buff)
{
  if (line_buff)
  {
    my_free((gptr) line_buff->buffer,MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*) line_buff,MYF(0));
  }
}


LINE_BUFFER *batch_readline_command(my_string str)
{
  LINE_BUFFER *line_buff;
  if (!(line_buff=(LINE_BUFFER*) my_malloc(sizeof(*line_buff),MYF(MY_WME))))
    return 0;
  if (init_line_buffer_from_string(line_buff,str))
  {
    my_free((char*) line_buff,MYF(0));
    return 0;
  }
  return line_buff;
}


/*****************************************************************************
      Functions to handle buffered readings of lines from a stream
******************************************************************************/

static bool
init_line_buffer(LINE_BUFFER *buffer,File file,ulong size,ulong max_buffer)
{
  bzero((char*) buffer,sizeof(buffer[0]));
  buffer->file=file;
  buffer->bufread=size;
  buffer->max_size=max_buffer;
  if (!(buffer->buffer = (char*) my_malloc(buffer->bufread+1,
					   MYF(MY_WME | MY_FAE))))
    return 1;
  buffer->end_of_line=buffer->end=buffer->buffer;
  buffer->buffer[0]=0;				/* For easy start test */
  return 0;
}


static bool init_line_buffer_from_string(LINE_BUFFER *buffer,my_string str)
{
  uint length;
  bzero((char*) buffer,sizeof(buffer[0]));
  length=(uint) strlen(str);
  if (!(buffer->buffer=buffer->start_of_line=buffer->end_of_line=
	(char*)my_malloc(length+2,MYF(MY_FAE))))
    return 1;
  memcpy(buffer->buffer,str,length);
  buffer->buffer[length]='\n';
  buffer->buffer[length+1]=0;
  buffer->end=buffer->buffer+length+1;
  buffer->eof=1;
  buffer->max_size=1;
  return 0;
}


static void free_line_buffer(LINE_BUFFER *buffer)
{
  if (buffer->buffer)
  {
    my_free((gptr) buffer->buffer,MYF(0));
    buffer->buffer=0;
  }
}


/* Fill the buffer retaining the last n bytes at the beginning of the
   newly filled buffer (for backward context).	Returns the number of new
   bytes read from disk. */


static uint fill_buffer(LINE_BUFFER *buffer)
{
  uint read_count;
  uint bufbytes= (uint) (buffer->end - buffer->start_of_line);

  if (buffer->eof)
    return 0;					/* Everything read */

  /* See if we need to grow the buffer. */

  for (;;)
  {
    uint start_offset=(uint) (buffer->start_of_line - buffer->buffer);
    read_count=(buffer->bufread - bufbytes)/IO_SIZE;
    if ((read_count*=IO_SIZE))
      break;
    buffer->bufread *= 2;
    if (!(buffer->buffer = (char*) my_realloc(buffer->buffer,
					      buffer->bufread+1,
					      MYF(MY_WME | MY_FAE))))
      return (uint) -1;
    buffer->start_of_line=buffer->buffer+start_offset;
    buffer->end=buffer->buffer+bufbytes;
  }

  /* Shift stuff down. */
  if (buffer->start_of_line != buffer->buffer)
  {
    bmove(buffer->buffer,buffer->start_of_line,(uint) bufbytes);
    buffer->end=buffer->buffer+bufbytes;
  }

  /* Read in new stuff. */
  if ((read_count= my_read(buffer->file, (byte*) buffer->end, read_count,
			   MYF(MY_WME))) == MY_FILE_ERROR)
    return read_count;

  DBUG_PRINT("fill_buff", ("Got %d bytes", read_count));

  /* Kludge to pretend every nonempty file ends with a newline. */
  if (!read_count && bufbytes && buffer->end[-1] != '\n')
  {
    buffer->eof = read_count = 1;
    *buffer->end = '\n';
  }
  buffer->end_of_line=(buffer->start_of_line=buffer->buffer)+bufbytes;
  buffer->end+=read_count;
  *buffer->end=0;				/* Sentinel */
  return read_count;
}



char *intern_read_line(LINE_BUFFER *buffer,ulong *out_length)
{
  char *pos;
  uint length;
  DBUG_ENTER("intern_read_line");

  buffer->start_of_line=buffer->end_of_line;
  for (;;)
  {
    pos=buffer->end_of_line;
    while (*pos != '\n' && *pos)
      pos++;
    if (pos == buffer->end)
    {
      if ((uint) (pos - buffer->start_of_line) < buffer->max_size)
      {
	if (!(length=fill_buffer(buffer)) || length == (uint) -1)
	  DBUG_RETURN(0);
	continue;
      }
      pos--;					/* break line here */
    }
    buffer->end_of_line=pos+1;
    *out_length=(ulong) (pos + 1 - buffer->eof - buffer->start_of_line);
    DBUG_RETURN(buffer->start_of_line);
  }
}
