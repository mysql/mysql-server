/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDBMAIN_H
#define NDBMAIN_H

#if defined NDB_SOFTOSE || defined NDB_OSE
#include <ose.h>
#include <shell.h>

/* Define an OSE_PROCESS that can be started from osemain.con */
#define NDB_MAIN(name)                          \
int main_ ## name(int argc, const char** argv); \
OS_PROCESS(name){                               \
  main_ ## name(0, 0);                          \
  stop(current_process());                      \
  exit(0);                                      \
}                                               \
int main_ ## name(int argc, const char** argv)

/*  Define an function that can be started from the command line */
#define NDB_COMMAND(name, str_name, syntax, description, stacksize) \
int main_ ## name(int argc, const char** argv);  \
                                                 \
static int run_ ## name(int argc, char *argv[]){ \
 return main_ ## name (argc, argv);              \
}                                                \
                                                 \
OS_PROCESS(init_ ## name){                       \
 shell_add_cmd_attrs(str_name, syntax, description, \
          run_ ## name, OS_PRI_PROC, 25, stacksize); \
 stop(current_process());                        \
 return;                                         \
}                                                \
                                                 \
int main_ ## name(int argc, const char** argv)




#else

#define NDB_MAIN(name) \
int main(int argc, const char** argv)

#define NDB_COMMAND(name, str_name, syntax, description, stacksize) \
int main(int argc, const char** argv)


#endif


#endif
