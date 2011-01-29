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
  Written by Magnus Svensson
*/

/*
  Converts a SQL file into a C file that can be compiled and linked
  into other programs
*/

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

/* Compiler-dependent constant for maximum string constant */
#define MAX_STRING_CONSTANT_LENGTH 65535

FILE *in, *out;

static void die(const char *fmt, ...)
{
  va_list args;

  /* Print the error message */
  fprintf(stderr, "FATAL ERROR: ");
  if (fmt)
  {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
  else
    fprintf(stderr, "unknown error");
  fprintf(stderr, "\n");
  fflush(stderr);

  /* Close any open files */
  if (in)
    fclose(in);
  if (out)
    fclose(out);

  exit(1);
}


int main(int argc, char *argv[])
{
  char buff[512];
  struct stat st;
  char* struct_name= argv[1];
  char* infile_name= argv[2];
  char* outfile_name= argv[3];


  if (argc != 4)
    die("Usage: comp_sql <struct_name> <sql_filename> <c_filename>");

  /* Open input and output file */
  if (!(in= fopen(infile_name, "r")))
    die("Failed to open SQL file '%s'", infile_name);

  
  if (!(out= fopen(outfile_name, "w")))
    die("Failed to open output file '%s'", outfile_name);
  fprintf(out, "const char %s[]={\n",struct_name);

  /* 
    Some compilers have limitations how long a string constant can be.
    We'll output very long strings as hexadecimal arrays, and short ones
    as strings (prettier)
  */
  stat(infile_name, &st);
  if (st.st_size > MAX_STRING_CONSTANT_LENGTH)
  {
    int cnt=0;
    int c;
    int first_char= 1;
    for(cnt=0;;cnt++)
    {
      c= fgetc(in);
      if (c== -1)
        break;

      if(cnt != 0)
        fputc(',', out);

      /* Put line break after each 16 hex characters */
      if(cnt && (cnt%16 == 0))
        fputc('\n', out);

      fprintf(out,"0x%02x",c);
    }
    fprintf(out,",0x00",c);
  }
  else
  {
    fprintf(out,"\"");
    while (fgets(buff, sizeof(buff), in))
    {
      char *curr= buff;
      while (*curr)
      {
        if (*curr == '\n')
        {
          /*
            Reached end of line, add escaped newline, escaped
            backslash and a newline to outfile
          */
          fprintf(out, "\\n \"\n\"");
          curr++;
        }
        else if (*curr == '\r')
        {
          curr++; /* Skip */
        }
        else
        {
          if (*curr == '"')
          {
            /* Needs escape */
            fputc('\\', out);
          }

          fputc(*curr, out);
          curr++;
        }
      }
      if (*(curr-1) != '\n')
      {
        /*
          Some compilers have a max string length,
          insert a newline at every 512th char in long
          strings
        */
        fprintf(out, "\"\n\"");
      }
    }
    fprintf(out, "\\\n\"");
  }
  
  fprintf(out, "};\n");
  fclose(in);
  fclose(out);

  exit(0);

}

