# File included in all makefiles of the database
# (c) Innobase Oy 1995 - 2000

CCOM=cl

# Flags for the debug version
#CFL= -MTd -Za -Zi -W4 -WX -F8192 -D "WIN32"
#CFLN = -MTd -Zi -W4 -F8192 -D "WIN32"
#CFLW = -MTd -Zi -W3 -WX -F8192 -D "WIN32"
#LFL =

# Flags for the fast version
#CFL= -MT -Zi -Og -O2 -W3 -WX -D "WIN32"
#CFLN = -MT -Zi -Og -O2 -W3 -D "WIN32"
#CFLW = -MT -Zi -Og -O2 -W3 -WX -D "WIN32"
#LFL =

# Flags for the fast debug version
CFL= -MTd -Zi -W3 -WX -F8192 -D "WIN32"
CFLN = -MTd -Zi -W3 -F8192 -D "WIN32"
CFLW = -MTd -Zi -W3 -WX -F8192 -D "WIN32"
LFL = /link/NODEFAULTLIB:LIBCMT

# Flags for the profiler version
#CFL= -MT -Zi -Og -O2 -W3 -WX -D "WIN32"
#CFLN = -MT -Zi -Og -O2 -WX -D "WIN32"
#CFLW = -MT -Zi -Og -O2 -W3 -WX -D "WIN32"
#LFL= -link -PROFILE

# Flags for the fast version without debug info (= the production version)
#CFL= -MT -Og -O2 -G6 -W3 -WX -D "WIN32"
#CFLN = -MT -Og -O2 -G6 -W3 -D "WIN32"
#CFLW = -MT -Og -O2 -G6 -W3 -WX -D "WIN32"
#LFL =
