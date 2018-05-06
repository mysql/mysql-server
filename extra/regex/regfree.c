#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "my_regex.h"
#include "regex2.h"
#include "utils.h"

/*
 - regfree - free everything
 = extern void regfree(regex_t *);
 */
void
my_regfree(preg)
my_regex_t *preg;
{
	struct re_guts *g;

	if (preg->re_magic != MAGIC1)	/* oops */
		return;			/* nice to complain, but hard */

	g = preg->re_g;
	if (g == NULL || g->magic != MAGIC2)	/* oops again */
		return;
	preg->re_magic = 0;		/* mark it invalid */
	g->magic = 0;			/* mark it invalid */

	if (g->strip != NULL)
		free((char *)g->strip);
	if (g->sets != NULL)
		free((char *)g->sets);
	if (g->setbits != NULL)
		free((char *)g->setbits);
	if (g->must != NULL)
		free(g->must);
	free((char *)g);
}
