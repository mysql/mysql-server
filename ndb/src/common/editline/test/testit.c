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

/*  -*- c-basic-offset: 4; -*-
**  $Revision: 1.5 $
**
**  A "micro-shell" to test editline library.
**  If given any arguments, commands aren't executed.
*/
#include <ndb_global.h>
#include <editline/editline.h>

int
main(int argc, char **argv)
{
    char	*prompt;
    char	*p;
    int		doit;

    (void)argv; /* Suppress warning */

    doit = argc == 1;
    if ((prompt = getenv("TESTPROMPT")) == NULL)
	prompt = "testit>  ";

    while ((p = readline(prompt)) != NULL) {
	(void)printf("\t\t\t|%s|\n", p);
	if (doit) {
	    if (strncmp(p, "cd ", 3) == 0) {
		if (chdir(&p[3]) < 0)
		    perror(&p[3]);
	    } else {
		if (system(p) != 0)
		    perror(p);
	    }
	}
	add_history(p);
	free(p);
    }

    return 0;
}
