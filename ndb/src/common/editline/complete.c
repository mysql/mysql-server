/* -*- c-basic-offset: 4; -*-
** $Revision: 1.8 $
**
**  History and file completion functions for editline library.
*/
#include "editline_internal.h"


/*
**  strcmp-like sorting predicate for qsort.
*/
static int
compare(const void *p1, const void *p2)
{
    const char	**v1;
    const char	**v2;

    v1 = (const char **)p1;
    v2 = (const char **)p2;
    return strcmp(*v1, *v2);
}

/*
**  Fill in *avp with an array of names that match file, up to its length.
**  Ignore . and .. .
*/
static int
FindMatches(char *dir, char *file, char ***avp)
{
    char	**av;
    char	**new;
    char	*p;
    DIR		*dp;
    struct dirent *ep;
    size_t	ac;
    size_t	len;

    if ((dp = opendir(dir)) == NULL)
	return 0;

    av = NULL;
    ac = 0;
    len = strlen(file);
    while ((ep = readdir(dp)) != NULL) {
	p = ep->d_name;
	if (p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0')))
	    continue;
	if (len && strncmp(p, file, len) != 0)
	    continue;

	if ((ac % MEM_INC) == 0) {
	    if ((new = malloc(sizeof(char*) * (ac + MEM_INC))) == NULL)
		break;
	    if (ac) {
		memcpy(new, av, ac * sizeof (char **));
		free(av);
	    }
	    *avp = av = new;
	}

	if ((av[ac] = strdup(p)) == NULL) {
	    if (ac == 0)
		free(av);
	    break;
	}
	ac++;
    }

    /* Clean up and return. */
    (void)closedir(dp);
    if (ac)
	qsort(av, ac, sizeof (char **), compare);
    return ac;
}

/*
**  Split a pathname into allocated directory and trailing filename parts.
*/
static int
SplitPath(char *path, char **dirpart, char ** filepart)
{
    static char	DOT[] = ".";
    char	*dpart;
    char	*fpart;

    if ((fpart = strrchr(path, '/')) == NULL) {
	if ((dpart = strdup(DOT)) == NULL)
	    return -1;
	if ((fpart = strdup(path)) == NULL) {
	    free(dpart);
	    return -1;
	}
    }
    else {
	if ((dpart = strdup(path)) == NULL)
	    return -1;
	dpart[fpart - path + 1] = '\0';
	if ((fpart = strdup(++fpart)) == NULL) {
	    free(dpart);
	    return -1;
	}
    }
    *dirpart = dpart;
    *filepart = fpart;
    return 0;
}

/*
**  Attempt to complete the pathname, returning an allocated copy.
**  Fill in *unique if we completed it, or set it to 0 if ambiguous.
*/
char *
rl_complete(char *pathname,int *unique)
{
    char	**av;
    char	*dir;
    char	*file;
    char	*new;
    char	*p;
    size_t	ac;
    size_t	end;
    size_t	i;
    size_t	j;
    size_t	len;
    size_t	new_len;
    size_t	p_len;

    if (SplitPath(pathname, &dir, &file) < 0)
	return NULL;
    if ((ac = FindMatches(dir, file, &av)) == 0) {
	free(dir);
	free(file);
	return NULL;
    }

    p = NULL;
    len = strlen(file);
    if (ac == 1) {
	/* Exactly one match -- finish it off. */
	*unique = 1;
	j = strlen(av[0]) - len + 2;
	p_len = sizeof(char) * (j + 1);
	if ((p = malloc(p_len)) != NULL) {
	    memcpy(p, av[0] + len, j);
	    new_len = sizeof(char) * (strlen(dir) + strlen(av[0]) + 2);
	    new = malloc(new_len);
	    if(new != NULL) {
		snprintf(new, new_len, "%s/%s", dir, av[0]);
		rl_add_slash(new, p, p_len);
		free(new);
	    }
	}
    }
    else {
	/* Find largest matching substring. */
	for (*unique = 0, i = len, end = strlen(av[0]); i < end; i++)
	    for (j = 1; j < ac; j++)
		if (av[0][i] != av[j][i])
		    goto breakout;
breakout:
	if (i > len) {
	    j = i - len + 1;
	    if ((p = malloc(sizeof(char) * j)) != NULL) {
		memcpy(p, av[0] + len, j);
		p[j - 1] = '\0';
	    }
	}
    }

    /* Clean up and return. */
    free(dir);
    free(file);
    for (i = 0; i < ac; i++)
	free(av[i]);
    free(av);
    return p;
}

/*
**  Return all possible completions.
*/
int
rl_list_possib(char *pathname, char ***avp)
{
    char	*dir;
    char	*file;
    int		ac;

    if (SplitPath(pathname, &dir, &file) < 0)
	return 0;
    ac = FindMatches(dir, file, avp);
    free(dir);
    free(file);
    return ac;
}
