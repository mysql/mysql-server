/*  -*- c-basic-offset: 4; -*-
**  $Revision: 1.6 $
**
**  Main editing routines for editline library.
*/
#include <ndb_global.h>

#include "editline_internal.h"
#include <signal.h>

/*
**  Manifest constants.
*/
#define SCREEN_WIDTH	80
#define SCREEN_ROWS	24
#define NO_ARG		(-1)
#define DEL		127
#define TAB		'\t'
#define CTL(x)		((x) & 0x1F)
#define ISCTL(x)	((x) && (x) < ' ')
#define UNCTL(x)	((x) + 64)
#define META(x)		((x) | 0x80)
#define ISMETA(x)	((x) & 0x80)
#define UNMETA(x)	((x) & 0x7F)
#define MAPSIZE		32
#define METAMAPSIZE	16
#if	!defined(HIST_SIZE)
#define HIST_SIZE	20
#endif	/* !defined(HIST_SIZE) */

/*
**  Command status codes.
*/
typedef enum _STATUS {
    CSdone, CSeof, CSmove, CSdispatch, CSstay, CSsignal
} STATUS;

/*
**  The type of case-changing to perform.
*/
typedef enum _CASE {
    TOupper, TOlower
} CASE;

/*
**  Key to command mapping.
*/
typedef struct _KEYMAP {
    char	Key;
    char	Active;
    STATUS	(*Function)();
} KEYMAP;

/*
**  Command history structure.
*/
typedef struct _HISTORY {
    int		Size;
    int		Pos;
    char	*Lines[HIST_SIZE];
} HISTORY;

/*
**  Globals.
*/
int		rl_eof;
int		rl_erase;
int		rl_intr;
int		rl_kill;
int		rl_quit;
#if	defined(DO_SIGTSTP)
int		rl_susp;
#endif	/* defined(DO_SIGTSTP) */

static char		NIL[] = "";
static const char	*Input = NIL;
static char		*Line;
static const char	*Prompt;
static char		*Yanked;
static char		*Screen;
static char		NEWLINE[]= CRLF;
static HISTORY		H;
static int		Repeat;
static int		End;
static int		Mark;
static int		OldPoint;
static int		Point;
static int		PushBack;
static int		Pushed;
static int		Signal;
static KEYMAP		Map[MAPSIZE];
static KEYMAP		MetaMap[METAMAPSIZE];
static size_t		Length;
static size_t		ScreenCount;
static size_t		ScreenSize;
static char		*backspace;
static int		TTYwidth;
static int		TTYrows;

/* Display print 8-bit chars as `M-x' or as the actual 8-bit char? */
int		rl_meta_chars = 1;

/*
**  Declarations.
*/
static char	*editinput();

#if	defined(USE_TERMCAP)
extern char	*getenv();
extern char	*tgetstr();
extern int	tgetent();
extern int	tgetnum();
#endif	/* defined(USE_TERMCAP) */

/*
**  TTY input/output functions.
*/

static void
TTYflush()
{
    if (ScreenCount) {
	(void)write(1, Screen, ScreenCount);
	ScreenCount = 0;
    }
}

static void
TTYput(const char c)
{
    Screen[ScreenCount] = c;
    if (++ScreenCount >= ScreenSize - 1) {
	ScreenSize += SCREEN_INC;
	Screen = realloc(Screen, sizeof(char) * ScreenSize);
	/* XXX what to do if realloc failes? */
    }
}

static void
TTYputs(const char *p)
{
    while (*p)
	TTYput(*p++);
}

static void
TTYshow(char c)
{
    if (c == DEL) {
	TTYput('^');
	TTYput('?');
    }
    else if (c == TAB) {
	/* XXX */
    }
    else if (ISCTL(c)) {
	TTYput('^');
	TTYput(UNCTL(c));
    }
    else if (rl_meta_chars && ISMETA(c)) {
	TTYput('M');
	TTYput('-');
	TTYput(UNMETA(c));
    }
    else
	TTYput(c);
}

static void
TTYstring(char *p)
{
    while (*p)
	TTYshow(*p++);
}

static int
TTYget()
{
    char	c;

    TTYflush();
    if (Pushed) {
	Pushed = 0;
	return PushBack;
    }
    if (*Input)
	return *Input++;
    return read(0, &c, (size_t)1) == 1 ? c : EOF;
}

#define TTYback()	(backspace ? TTYputs((const char *)backspace) : TTYput('\b'))

static void
TTYbackn(int n)
{
    while (--n >= 0)
	TTYback();
}

static void
TTYinfo()
{
    static int		init;
#if	defined(USE_TERMCAP)
    char		*term;
    char		buff[2048];
    char		*bp;
    char		*p;
#endif	/* defined(USE_TERMCAP) */
#if	defined(TIOCGWINSZ)
    struct winsize	W;
#endif	/* defined(TIOCGWINSZ) */

    if (init) {
#if	defined(TIOCGWINSZ)
	/* Perhaps we got resized. */
	if (ioctl(0, TIOCGWINSZ, &W) >= 0
	 && W.ws_col > 0 && W.ws_row > 0) {
	    TTYwidth = (int)W.ws_col;
	    TTYrows = (int)W.ws_row;
	}
#endif	/* defined(TIOCGWINSZ) */
	return;
    }
    init++;

    TTYwidth = TTYrows = 0;
#if	defined(USE_TERMCAP)
    bp = &buff[0];
    if ((term = getenv("TERM")) == NULL)
	term = "dumb";
    if (tgetent(buff, term) < 0) {
	TTYwidth = SCREEN_WIDTH;
	TTYrows = SCREEN_ROWS;
	return;
    }
    p = tgetstr("le", &bp);
    backspace = p ? strdup(p) : NULL;
    TTYwidth = tgetnum("co");
    TTYrows = tgetnum("li");
#endif	/* defined(USE_TERMCAP) */

#if	defined(TIOCGWINSZ)
    if (ioctl(0, TIOCGWINSZ, &W) >= 0) {
	TTYwidth = (int)W.ws_col;
	TTYrows = (int)W.ws_row;
    }
#endif	/* defined(TIOCGWINSZ) */

    if (TTYwidth <= 0 || TTYrows <= 0) {
	TTYwidth = SCREEN_WIDTH;
	TTYrows = SCREEN_ROWS;
    }
}


/*
**  Print an array of words in columns.
*/
static void
columns(int ac, char **av)
{
    char	*p;
    int		i;
    int		j;
    int		k;
    int		len;
    int		skip;
    int		longest;
    int		cols;

    /* Find longest name, determine column count from that. */
    for (longest = 0, i = 0; i < ac; i++)
	if ((j = strlen((char *)av[i])) > longest)
	    longest = j;
    cols = TTYwidth / (longest + 3);

    TTYputs((const char *)NEWLINE);
    for (skip = ac / cols + 1, i = 0; i < skip; i++) {
	for (j = i; j < ac; j += skip) {
	    for (p = av[j], len = strlen((char *)p), k = len; --k >= 0; p++)
		TTYput(*p);
	    if (j + skip < ac)
		while (++len < longest + 3)
		    TTYput(' ');
	}
	TTYputs((const char *)NEWLINE);
    }
}

static void
reposition()
{
    int		i;
    char	*p;

    TTYput('\r');
    TTYputs((const char *)Prompt);
    for (i = Point, p = Line; --i >= 0; p++)
	TTYshow(*p);
}

static void
left(STATUS Change)
{
    char	c;

    TTYback();
    if (Point) {
	c = Line[Point - 1];
	if (c == TAB) {
	    /* XXX */
	}
	else if (ISCTL(c))
	    TTYback();
	else if (rl_meta_chars && ISMETA(c)) {
	    TTYback();
	    TTYback();
	}
    }
    if (Change == CSmove)
	Point--;
}

static void
right(STATUS Change)
{
    TTYshow(Line[Point]);
    if (Change == CSmove)
	Point++;
}

static STATUS
ring_bell()
{
    TTYput('\07');
    TTYflush();
    return CSstay;
}

static STATUS
do_macro(int c)
{
    char		name[4];

    name[0] = '_';
    name[1] = c;
    name[2] = '_';
    name[3] = '\0';

    if ((Input = (char *)getenv((char *)name)) == NULL) {
	Input = NIL;
	return ring_bell();
    }
    return CSstay;
}

static STATUS
do_forward(STATUS move)
{
    int		i;
    char	*p;

    i = 0;
    do {
	p = &Line[Point];
	for ( ; Point < End && (*p == ' ' || !isalnum((int)*p)); Point++, p++)
	    if (move == CSmove)
		right(CSstay);

	for (; Point < End && isalnum((int)*p); Point++, p++)
	    if (move == CSmove)
		right(CSstay);

	if (Point == End)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

static STATUS
do_case(CASE type)
{
    int		i;
    int		end;
    int		count;
    char	*p;

    (void)do_forward(CSstay);
    if (OldPoint != Point) {
	if ((count = Point - OldPoint) < 0)
	    count = -count;
	Point = OldPoint;
	if ((end = Point + count) > End)
	    end = End;
	for (i = Point, p = &Line[i]; i < end; i++, p++) {
	    if (type == TOupper) {
		if (islower((int)*p))
		    *p = toupper((int)*p);
	    }
	    else if (isupper((int)*p))
		*p = tolower((int)*p);
	    right(CSmove);
	}
    }
    return CSstay;
}

static STATUS
case_down_word()
{
    return do_case(TOlower);
}

static STATUS
case_up_word()
{
    return do_case(TOupper);
}

static void
ceol()
{
    int		extras;
    int		i;
    char	*p;

    for (extras = 0, i = Point, p = &Line[i]; i <= End; i++, p++) {
	TTYput(' ');
	if (*p == TAB) {
	    /* XXX */
	}
	else if (ISCTL(*p)) {
	    TTYput(' ');
	    extras++;
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    TTYput(' ');
	    TTYput(' ');
	    extras += 2;
	}
    }

    for (i += extras; i > Point; i--)
	TTYback();
}

static void
clear_line()
{
    Point = -strlen(Prompt);
    TTYput('\r');
    ceol();
    Point = 0;
    End = 0;
    Line[0] = '\0';
}

static STATUS
insert_string(char *p)
{
    size_t	len;
    int		i;
    char	*new;
    char	*q;

    len = strlen((char *)p);
    if (End + len >= Length) {
	if ((new = malloc(sizeof(char) * (Length + len + MEM_INC))) == NULL)
	    return CSstay;
	if (Length) {
	    memcpy(new, Line, Length);
	    free(Line);
	}
	Line = new;
	Length += len + MEM_INC;
    }

    for (q = &Line[Point], i = End - Point; --i >= 0; )
	q[len + i] = q[i];
    memcpy(&Line[Point], p, len);
    End += len;
    Line[End] = '\0';
    TTYstring(&Line[Point]);
    Point += len;

    return Point == End ? CSstay : CSmove;
}

static STATUS
redisplay()
{
    TTYputs((const char *)NEWLINE);
    TTYputs((const char *)Prompt);
    TTYstring(Line);
    return CSmove;
}

static STATUS
redisplay_no_nl()
{
    TTYput('\r');
    TTYputs((const char *)Prompt);
    TTYstring(Line);
    return CSmove;
}

static STATUS
toggle_meta_mode()
{
    rl_meta_chars = !rl_meta_chars;
    return redisplay();
}


static char *
next_hist()
{
    return H.Pos >= H.Size - 1 ? NULL : H.Lines[++H.Pos];
}

static char *
prev_hist()
{
    return H.Pos == 0 ? NULL : H.Lines[--H.Pos];
}

static STATUS
do_insert_hist(char *p)
{
    if (p == NULL)
	return ring_bell();
    Point = 0;
    reposition();
    ceol();
    End = 0;
    return insert_string(p);
}

static STATUS
do_hist(char *(*move)())
{
    char	*p;
    int		i;

    i = 0;
    do {
	if ((p = (*move)()) == NULL)
	    return ring_bell();
    } while (++i < Repeat);
    return do_insert_hist(p);
}

static STATUS
h_next()
{
    return do_hist(next_hist);
}

static STATUS
h_prev()
{
    return do_hist(prev_hist);
}

static STATUS
h_first()
{
    return do_insert_hist(H.Lines[H.Pos = 0]);
}

static STATUS
h_last()
{
    return do_insert_hist(H.Lines[H.Pos = H.Size - 1]);
}

/*
**  Return zero if pat appears as a substring in text.
*/
static int
substrcmp(char *text, char *pat,int len)
{
    char	c;

    if ((c = *pat) == '\0')
	return *text == '\0';
    for ( ; *text; text++)
	if (*text == c && strncmp(text, pat, len) == 0)
	    return 0;
    return 1;
}

static char *
search_hist(char *search,char *(*move)())
{
    static char	*old_search;
    int		len;
    int		pos;
    int		(*match)();
    char	*pat;

    /* Save or get remembered search pattern. */
    if (search && *search) {
	if (old_search)
	    free(old_search);
	old_search = strdup(search);
    }
    else {
	if (old_search == NULL || *old_search == '\0')
	    return NULL;
	search = old_search;
    }

    /* Set up pattern-finder. */
    if (*search == '^') {
	match = strncmp;
	pat = (char *)(search + 1);
    }
    else {
	match = substrcmp;
	pat = (char *)search;
    }
    len = strlen(pat);

    for (pos = H.Pos; (*move)() != NULL; )
	if ((*match)((char *)H.Lines[H.Pos], pat, len) == 0)
	    return H.Lines[H.Pos];
    H.Pos = pos;
    return NULL;
}

static STATUS
h_search()
{
    static int	Searching;
    const char	*old_prompt;
    char	*(*move)();
    char	*p;

    if (Searching)
	return ring_bell();
    Searching = 1;

    clear_line();
    old_prompt = Prompt;
    Prompt = "Search: ";
    TTYputs((const char *)Prompt);
    move = Repeat == NO_ARG ? prev_hist : next_hist;
    p = editinput();
    Searching = 0;
    if (p == NULL && Signal > 0) {
	Signal = 0;
	clear_line();
	Prompt = old_prompt;
	return redisplay_no_nl();
    }
    p = search_hist(p, move);
    clear_line();
    Prompt = old_prompt;
    if (p == NULL) {
	(void)ring_bell();
	return redisplay_no_nl();
    }
    return do_insert_hist(p);
}

static STATUS
fd_char()
{
    int		i;

    i = 0;
    do {
	if (Point >= End)
	    break;
	right(CSmove);
    } while (++i < Repeat);
    return CSstay;
}

static void
save_yank(int begin, int i)
{
    if (Yanked) {
	free(Yanked);
	Yanked = NULL;
    }

    if (i < 1)
	return;

    if ((Yanked = malloc(sizeof(char) * (i + 1))) != NULL) {
	memcpy(Yanked, &Line[begin], i);
	Yanked[i] = '\0';
    }
}

static STATUS
delete_string(int count)
{
    int		i;
    char	*p;

    if (count <= 0 || End == Point)
	return ring_bell();

    if (count == 1 && Point == End - 1) {
	/* Optimize common case of delete at end of line. */
	End--;
	p = &Line[Point];
	i = 1;
	TTYput(' ');
	if (*p == TAB) {
	    /* XXX */
	}
	else if (ISCTL(*p)) {
	    i = 2;
	    TTYput(' ');
	}
	else if (rl_meta_chars && ISMETA(*p)) {
	    i = 3;
	    TTYput(' ');
	    TTYput(' ');
	}
	TTYbackn(i);
	*p = '\0';
	return CSmove;
    }
    if (Point + count > End && (count = End - Point) <= 0)
	return CSstay;

    if (count > 1)
	save_yank(Point, count);

    ceol();
    for (p = &Line[Point], i = End - (Point + count) + 1; --i >= 0; p++)
	p[0] = p[count];
    End -= count;
    TTYstring(&Line[Point]);
    return CSmove;
}

static STATUS
bk_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return CSstay;
}

static STATUS
bk_del_char()
{
    int		i;

    i = 0;
    do {
	if (Point == 0)
	    break;
	left(CSmove);
    } while (++i < Repeat);

    return delete_string(i);
}

static STATUS
kill_line()
{
    int		i;

    if (Repeat != NO_ARG) {
	if (Repeat < Point) {
	    i = Point;
	    Point = Repeat;
	    reposition();
	    (void)delete_string(i - Point);
	}
	else if (Repeat > Point) {
	    right(CSmove);
	    (void)delete_string(Repeat - Point - 1);
	}
	return CSmove;
    }

    save_yank(Point, End - Point);
    ceol();
    Line[Point] = '\0';
    End = Point;
    return CSstay;
}

static STATUS
insert_char(int c)
{
    STATUS	s;
    char	buff[2];
    char	*p;
    char	*q;
    int		i;

    if (Repeat == NO_ARG || Repeat < 2) {
	buff[0] = c;
	buff[1] = '\0';
	return insert_string(buff);
    }

    if ((p = malloc(sizeof(char) * (Repeat + 1))) == NULL)
	return CSstay;
    for (i = Repeat, q = p; --i >= 0; )
	*q++ = c;
    *q = '\0';
    Repeat = 0;
    s = insert_string(p);
    free(p);
    return s;
}

static STATUS
meta()
{
    int c;
    KEYMAP		*kp;

    if ((c = TTYget()) == EOF)
	return CSeof;
#if	defined(ANSI_ARROWS)
    /* Also include VT-100 arrows. */
    if (c == '[' || c == 'O')
	switch ((int)(c = TTYget())) {
	default:	return ring_bell();
	case EOF:	return CSeof;
	case 'A':	return h_prev();
	case 'B':	return h_next();
	case 'C':	return fd_char();
	case 'D':	return bk_char();
	}
#endif	/* defined(ANSI_ARROWS) */

    if (isdigit(c)) {
	for (Repeat = c - '0'; (c = TTYget()) != EOF && isdigit(c); )
	    Repeat = Repeat * 10 + c - '0';
	Pushed = 1;
	PushBack = c;
	return CSstay;
    }

    if (isupper(c))
	return do_macro(c);
    for (OldPoint = Point, kp = MetaMap; kp < &MetaMap[METAMAPSIZE]; kp++)
	if (kp->Key == c && kp->Active)
	    return (*kp->Function)();

    return ring_bell();
}

static STATUS
emacs(int c)
{
    STATUS		s;
    KEYMAP		*kp;

#if	0
    /* This test makes it impossible to enter eight-bit characters when
     * meta-char mode is enabled. */
    if (rl_meta_chars && ISMETA(c)) {
	Pushed = 1;
	PushBack = UNMETA(c);
	return meta();
    }
#endif	/* 0 */
    for (kp = Map; kp < &Map[MAPSIZE]; kp++)
	if (kp->Key == c && kp->Active)
	    break;
    s = kp < &Map[MAPSIZE] ? (*kp->Function)() : insert_char((int)c);
    if (!Pushed)
	/* No pushback means no repeat count; hacky, but true. */
	Repeat = NO_ARG;
    return s;
}

static STATUS
TTYspecial(int c)
{
    if (rl_meta_chars && ISMETA(c))
	return CSdispatch;

    if (c == rl_erase || c == DEL)
	return bk_del_char();
    if (c == rl_kill) {
	if (Point != 0) {
	    Point = 0;
	    reposition();
	}
	Repeat = NO_ARG;
	return kill_line();
    }
    if (c == rl_eof && Point == 0 && End == 0)
	return CSeof;
    if (c == rl_intr) {
	Signal = SIGINT;
	return CSsignal;
    }
    if (c == rl_quit) {
	Signal = SIGQUIT;
	return CSsignal;
    }
#if	defined(DO_SIGTSTP)
    if (c == rl_susp) {
	Signal = SIGTSTP;
	return CSsignal;
    }
#endif	/* defined(DO_SIGTSTP) */

    return CSdispatch;
}

static char *
editinput()
{
    int c;

    Repeat = NO_ARG;
    OldPoint = Point = Mark = End = 0;
    Line[0] = '\0';

    Signal = -1;
    while ((c = TTYget()) != EOF)
	switch (TTYspecial(c)) {
	case CSdone:
	    return Line;
	case CSeof:
	    return NULL;
	case CSsignal:
	    return (char *)"";
	case CSmove:
	    reposition();
	    break;
	case CSdispatch:
	    switch (emacs(c)) {
	    case CSdone:
		return Line;
	    case CSeof:
		return NULL;
	    case CSsignal:
		return (char *)"";
	    case CSmove:
		reposition();
		break;
	    case CSdispatch:
	    case CSstay:
		break;
	    }
	    break;
	case CSstay:
	    break;
	}
    return NULL;
}

static void
hist_add(char *p)
{
    int		i;

    if ((p = strdup(p)) == NULL)
	return;
    if (H.Size < HIST_SIZE)
	H.Lines[H.Size++] = p;
    else {
	free(H.Lines[0]);
	for (i = 0; i < HIST_SIZE - 1; i++)
	    H.Lines[i] = H.Lines[i + 1];
	H.Lines[i] = p;
    }
    H.Pos = H.Size - 1;
}

static char *
read_redirected()
{
    int		size;
    char	*p;
    char	*line;
    char	*end;

    for (size = MEM_INC, p = line = malloc(sizeof(char) * size), end = p + size; ; p++) {
	if (p == end) {
	    size += MEM_INC;
	    p = line = realloc(line, size);
	    end = p + size;
	}
	if (read(0, p, 1) <= 0) {
	    /* Ignore "incomplete" lines at EOF, just like we do for a tty. */
	    free(line);
	    return NULL;
	}
	if (*p == '\n')
	    break;
    }
    *p = '\0';
    return line;
}

/*
**  For compatibility with FSF readline.
*/
/* ARGSUSED0 */
void
rl_reset_terminal(char *p)
{
    (void)p; /* Suppress warning */
}

void
rl_initialize()
{
}

int
rl_insert(int count, int c)
{
    if (count > 0) {
	Repeat = count;
	(void)insert_char(c);
	(void)redisplay_no_nl();
    }
    return 0;
}

int (*rl_event_hook)();

int
rl_key_action(int c, char flag)
{
    KEYMAP	*kp;
    int		size;

    (void)flag; /* Suppress warning */

    if (ISMETA(c)) {
	kp = MetaMap;
	size = METAMAPSIZE;
    }
    else {
	kp = Map;
	size = MAPSIZE;
    }
    for ( ; --size >= 0; kp++)
	if (kp->Key == c) {
	    kp->Active = c ? 1 : 0;
	    return 1;
	}
    return -1;
}

char *
readline(const char *prompt)
{
    char	*line;
    int		s;

    if (!isatty(0)) {
	TTYflush();
	return read_redirected();
    }

    if (Line == NULL) {
	Length = MEM_INC;
	if ((Line = malloc(sizeof(char) * Length)) == NULL)
	    return NULL;
    }

    TTYinfo();
    rl_ttyset(0);
    hist_add(NIL);
    ScreenSize = SCREEN_INC;
    Screen = malloc(sizeof(char) * ScreenSize);
    Prompt = prompt ? prompt : (char *)NIL;
    TTYputs((const char *)Prompt);
    if ((line = editinput()) != NULL) {
	line = strdup(line);
	TTYputs((const char *)NEWLINE);
	TTYflush();
    }
    rl_ttyset(1);
    free(Screen);
    free(H.Lines[--H.Size]);
    if (Signal > 0) {
	s = Signal;
	Signal = 0;
	(void)kill(getpid(), s);
    }
    return (char *)line;
}

void
add_history(char *p)
{
    if (p == NULL || *p == '\0')
	return;

#if	defined(UNIQUE_HISTORY)
    if (H.Size && strcmp(p, H.Lines[H.Size - 1]) == 0)
	return;
#endif	/* defined(UNIQUE_HISTORY) */
    hist_add((char *)p);
}


static STATUS
beg_line()
{
    if (Point) {
	Point = 0;
	return CSmove;
    }
    return CSstay;
}

static STATUS
del_char()
{
    return delete_string(Repeat == NO_ARG ? 1 : Repeat);
}

static STATUS
end_line()
{
    if (Point != End) {
	Point = End;
	return CSmove;
    }
    return CSstay;
}

/*
**  Return allocated copy of word under cursor, moving cursor after the
**  word.
*/
static char *
find_word()
{
    static char	SEPS[] = "\"#;&|^$=`'{}()<>\n\t ";
    char	*p;
    char	*new;
    size_t	len;

    /* Move forward to end of word. */
    p = &Line[Point];
    for ( ; Point < End && strchr(SEPS, (char)*p) == NULL; Point++, p++)
	right(CSstay);

    /* Back up to beginning of word. */
    for (p = &Line[Point]; p > Line && strchr(SEPS, (char)p[-1]) == NULL; p--)
	continue;
    len = Point - (p - Line) + 1;
    if ((new = malloc(sizeof(char) * len)) == NULL)
	return NULL;
    memcpy(new, p, len);
    new[len - 1] = '\0';
    return new;
}

static STATUS
c_complete()
{
    char	*p;
    char	*word;
    int		unique;

    word = find_word();
    p = (char *)rl_complete((char *)word, &unique);
    if (word)
	free(word);
    if (p && *p) {
	(void)insert_string(p);
	if (!unique)
	    (void)ring_bell();
	free(p);
	return redisplay_no_nl();
    }
    return ring_bell();
}

static STATUS
c_possible()
{
    char	**av;
    char	*word;
    int		ac;

    word = find_word();
    ac = rl_list_possib((char *)word, (char ***)&av);
    if (word)
	free(word);
    if (ac) {
	columns(ac, av);
	while (--ac >= 0)
	    free(av[ac]);
	free(av);
	return redisplay_no_nl();
    }
    return ring_bell();
}

static STATUS
accept_line()
{
    Line[End] = '\0';
    return CSdone;
}

static STATUS
transpose()
{
    char	c;

    if (Point) {
	if (Point == End)
	    left(CSmove);
	c = Line[Point - 1];
	left(CSstay);
	Line[Point - 1] = Line[Point];
	TTYshow(Line[Point - 1]);
	Line[Point++] = c;
	TTYshow(c);
    }
    return CSstay;
}

static STATUS
quote()
{
    int	c;

    return (c = TTYget()) == EOF ? CSeof : insert_char((int)c);
}

static STATUS
wipe()
{
    int		i;

    if (Mark > End)
	return ring_bell();

    if (Point > Mark) {
	i = Point;
	Point = Mark;
	Mark = i;
	reposition();
    }

    return delete_string(Mark - Point);
}

static STATUS
mk_set()
{
    Mark = Point;
    return CSstay;
}

static STATUS
exchange()
{
    int c;

    if ((c = TTYget()) != CTL('X'))
	return c == EOF ? CSeof : ring_bell();

    if ((c = Mark) <= End) {
	Mark = Point;
	Point = c;
	return CSmove;
    }
    return CSstay;
}

static STATUS
yank()
{
    if (Yanked && *Yanked)
	return insert_string(Yanked);
    return CSstay;
}

static STATUS
copy_region()
{
    if (Mark > End)
	return ring_bell();

    if (Point > Mark)
	save_yank(Mark, Point - Mark);
    else
	save_yank(Point, Mark - Point);

    return CSstay;
}

static STATUS
move_to_char()
{
    int c;
    int i;
    char		*p;

    if ((c = TTYget()) == EOF)
	return CSeof;
    for (i = Point + 1, p = &Line[i]; i < End; i++, p++)
	if (*p == c) {
	    Point = i;
	    return CSmove;
	}
    return CSstay;
}

static STATUS
fd_word()
{
    return do_forward(CSmove);
}

static STATUS
fd_kill_word()
{
    int		i;

    (void)do_forward(CSstay);
    if (OldPoint != Point) {
	i = Point - OldPoint;
	Point = OldPoint;
	return delete_string(i);
    }
    return CSstay;
}

static STATUS
bk_word()
{
    int		i;
    char	*p;

    i = 0;
    do {
	for (p = &Line[Point]; p > Line && !isalnum((int)p[-1]); p--)
	    left(CSmove);

	for (; p > Line && p[-1] != ' ' && isalnum((int)p[-1]); p--)
	    left(CSmove);

	if (Point == 0)
	    break;
    } while (++i < Repeat);

    return CSstay;
}

static STATUS
bk_kill_word()
{
    (void)bk_word();
    if (OldPoint != Point)
	return delete_string(OldPoint - Point);
    return CSstay;
}

static int
argify(char *line, char ***avp)
{
    char	*c;
    char	**p;
    char	**new;
    int		ac;
    int		i;

    i = MEM_INC;
    if ((*avp = p = malloc(sizeof(char*) * i))== NULL)
	 return 0;

    for (c = line; isspace((int)*c); c++)
	continue;
    if (*c == '\n' || *c == '\0')
	return 0;

    for (ac = 0, p[ac++] = c; *c && *c != '\n'; ) {
	if (isspace((int)*c)) {
	    *c++ = '\0';
	    if (*c && *c != '\n') {
		if (ac + 1 == i) {
		    new = malloc(sizeof(char*) * (i + MEM_INC));
		    if (new == NULL) {
			p[ac] = NULL;
			return ac;
		    }
		    memcpy(new, p, i * sizeof (char **));
		    i += MEM_INC;
		    free(p);
		    *avp = p = new;
		}
		p[ac++] = c;
	    }
	}
	else
	    c++;
    }
    *c = '\0';
    p[ac] = NULL;
    return ac;
}

static STATUS
last_argument()
{
    char	**av;
    char	*p;
    STATUS	s;
    int		ac;

    if (H.Size == 1 || (p = H.Lines[H.Size - 2]) == NULL)
	return ring_bell();

    if ((p = strdup(p)) == NULL)
	return CSstay;
    ac = argify(p, &av);

    if (Repeat != NO_ARG)
	s = Repeat < ac ? insert_string(av[Repeat]) : ring_bell();
    else
	s = ac ? insert_string(av[ac - 1]) : CSstay;

    if (ac)
	free(av);
    free(p);
    return s;
}

static KEYMAP	Map[MAPSIZE] = {
    {	CTL('@'),	1,	ring_bell	},
    {	CTL('A'),	1,	beg_line	},
    {	CTL('B'),	1,	bk_char		},
    {	CTL('D'),	1,	del_char	},
    {	CTL('E'),	1,	end_line	},
    {	CTL('F'),	1,	fd_char		},
    {	CTL('G'),	1,	ring_bell	},
    {	CTL('H'),	1,	bk_del_char	},
    {	CTL('I'),	1,	c_complete	},
    {	CTL('J'),	1,	accept_line	},
    {	CTL('K'),	1,	kill_line	},
    {	CTL('L'),	1,	redisplay	},
    {	CTL('M'),	1,	accept_line	},
    {	CTL('N'),	1,	h_next		},
    {	CTL('O'),	1,	ring_bell	},
    {	CTL('P'),	1,	h_prev		},
    {	CTL('Q'),	1,	ring_bell	},
    {	CTL('R'),	1,	h_search	},
    {	CTL('S'),	1,	ring_bell	},
    {	CTL('T'),	1,	transpose	},
    {	CTL('U'),	1,	ring_bell	},
    {	CTL('V'),	1,	quote		},
    {	CTL('W'),	1,	wipe		},
    {	CTL('X'),	1,	exchange	},
    {	CTL('Y'),	1,	yank		},
    {	CTL('Z'),	1,	ring_bell	},
    {	CTL('['),	1,	meta		},
    {	CTL(']'),	1,	move_to_char	},
    {	CTL('^'),	1,	ring_bell	},
    {	CTL('_'),	1,	ring_bell	},
};

static KEYMAP	MetaMap[16]= {
    {	CTL('H'),	1,	bk_kill_word	},
    {	CTL('['),	1,	c_possible	},
    {	DEL,		1,	bk_kill_word	},
    {	' ',		1,	mk_set		},
    {	'.',		1,	last_argument	},
    {	'<',		1,	h_first		},
    {	'>',		1,	h_last		},
    {	'?',		1,	c_possible	},
    {	'b',		1,	bk_word		},
    {	'd',		1,	fd_kill_word	},
    {	'f',		1,	fd_word		},
    {	'l',		1,	case_down_word	},
    {	'm',		1,	toggle_meta_mode},
    {	'u',		1,	case_up_word	},
    {	'y',		1,	yank		},
    {	'w',		1,	copy_region	},
};
