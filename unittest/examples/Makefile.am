AM_CPPFLAGS        = -I$(srcdir) -I$(top_builddir)/include \
		     -I$(top_srcdir)/unittest/mytap -I$(top_srcdir)/include

AM_LDFLAGS         = -L$(top_builddir)/unittest/mytap

LDADD              = -lmytap

noinst_PROGRAMS    = simple.t skip.t todo.t skip_all.t no_plan.t

simple_t_SOURCES   = simple.t.c

skip_t_SOURCES     = skip.t.c

todo_t_SOURCES     = todo.t.c

skip_all_t_SOURCES = skip_all.t.c

no_plan_t_SOURCES  = no_plan.t.c
