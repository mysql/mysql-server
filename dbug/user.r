.\"           @(#)user.r      1.13 10/29/86
.\"
.\" 2004-10-29: documented features implemented since 10/29/86
.\"             formatting cleanup
.\"                                               - Sergei Golubchik
.\"
.\"           DBUG (Macro Debugger Package) nroff source
.\"
.\"           nroff -mm user.r >user.t
.\"           groff -mm user.r >user.ps
.\"
.\" ===================================================
.\"
.\"           === Some sort of black magic, but I forget...
.tr ~
.\"           === Hyphenation control (1 = on)
.\".nr Hy 1
.\"           === Force all first level headings to start on new page
.nr Ej 1
.\"           === Set for breaks after headings for levels 1-3
.nr Hb 3
.\"           === Set for space after headings for levels 1-3
.nr Hs 3
.\"           === Set standard indent for one/half inch
.nr Si 10
.\"           === Set page header
.PH "/DBUG User Manual//\*(DT/"
.\"           === Set page footer
.PF "// - % - //"
.\"           === Set page offset
.\".po 0.60i
.\"           === Set line length
.\".ll 6.5i
.TL
D B U G
.P 0
C Program Debugging Package
.P 0
by
.AU "Fred Fish"
.AF ""
.SA 1
.\"           === All paragraphs indented.
.nr Pt 1
.AS 1
This document introduces
.I dbug ,
a macro based C debugging
package which has proven to be a very flexible and useful tool
for debugging, testing, and porting C programs.

.P
All of the features of the
.I dbug
package can be enabled or disabled dynamically at execution time.
This means that production programs will run normally when
debugging is not enabled, and eliminates the need to maintain two
separate versions of a program.

.P
Many of the things easily accomplished with conventional debugging
tools, such as symbolic debuggers, are difficult or impossible with this
package, and vice versa.
Thus the
.I dbug
package should 
.I not
be thought of as a replacement or substitute for
other debugging tools, but simply as a useful
.I addition
to the
program development and maintenance environment.

.AE
.MT 4
.SK
.B
INTRODUCTION
.R

.P
Almost every program development environment worthy of the name
provides some sort of debugging facility.
Usually this takes the form of a program which is capable of
controlling execution of other programs and examining the internal
state of other executing programs.
These types of programs will be referred to as external debuggers
since the debugger is not part of the executing program.
Examples of this type of debugger include the
.B adb
and
.B sdb
debuggers provided with the 
.B UNIX\*F
.FS
UNIX is a trademark of AT&T Bell Laboratories.
.FE
operating system.

.P
One of the problems associated with developing programs in an environment
with good external debuggers is that developed programs tend to have 
little or no internal instrumentation.
This is usually not a problem for the developer since he is,
or at least should be, intimately familiar with the internal organization,
data structures, and control flow of the program being debugged.
It is a serious problem for maintenance programmers, who
are unlikely to have such familiarity with the program being
maintained, modified, or ported to another environment.
It is also a problem, even for the developer, when the program is
moved to an environment with a primitive or unfamiliar debugger,
or even no debugger.

.P
On the other hand,
.I dbug
is an example of an internal debugger.
Because it requires internal instrumentation of a program,
and its usage does not depend on any special capabilities of
the execution environment, it is always available and will
execute in any environment that the program itself will
execute in.
In addition, since it is a complete package with a specific
user interface, all programs which use it will be provided
with similar debugging capabilities.
This is in sharp contrast to other forms of internal instrumentation
where each developer has their own, usually less capable, form
of internal debugger.
In summary,
because 
.I dbug
is an internal debugger it provides consistency across operating
environments, 
and because it is available to all developers it provides
consistency across all programs in the same environment.

.P
The
.I dbug
package imposes only a slight speed penalty on executing
programs, typically much less than 10 percent, and a modest size
penalty, typically 10 to 20 percent.
By defining a specific C preprocessor symbol both of these
can be reduced to zero with no changes required to the
source code.

.P
The following list is a quick summary of the capabilities
of the
.I dbug
package.
Each capability can be individually enabled or disabled
at the time a program is invoked by specifying the appropriate
command line arguments.
.SP 1
.ML o 1i
.LI
Execution trace showing function level control flow in a
semi-graphically manner using indentation to indicate nesting
depth.
.LI
Output the values of all, or any subset of, key internal variables.
.LI
Limit actions to a specific set of named functions.
.LI
Limit function trace to a specified nesting depth.
.LI
Label each output line with source file name and line number.
.LI
Label each output line with name of current process.
.LI
Push or pop internal debugging state to allow execution with
built in debugging defaults.
.LI
Redirect the debug output stream to standard output (stdout)
or a named file.
The default output stream is standard error (stderr).
The redirection mechanism is completely independent of
normal command line redirection to avoid output conflicts.
.LE

.SK
.B
PRIMITIVE DEBUGGING TECHNIQUES
.R

.P
Internal instrumentation is already a familiar concept
to most programmers, since it is usually the first debugging
technique learned.
Typically, "print\ statements" are inserted in the source
code at interesting points, the code is recompiled and executed,
and the resulting output is examined in an attempt to determine
where the problem is.

The procedure is iterative, with each iteration yielding more
and more output, and hopefully the source of the problem is
discovered before the output becomes too large to deal with
or previously inserted statements need to be removed.
Figure 1 is an example of this type of primitive debugging
technique.
.DS I N
.SP 2
\fC
.so example1.r
\fR
.SP 2
.ll -5
.ce
Figure 1
.ce
Primitive Debugging Technique
.ll +5
.SP 2
.DE

.P
Eventually, and usually after at least several iterations, the
problem will be found and corrected.
At this point, the newly inserted print statements must be 
dealt with.
One obvious solution is to simply delete them all.
Beginners usually do this a few times until they have to
repeat the entire process every time a new bug pops up.
The second most obvious solution is to somehow disable
the output, either through the source code comment facility,
creation of a debug variable to be switched on or off, or by using the
C preprocessor.
Figure 2 is an example of all three techniques.
.DS I N
.SP 2
\fC
.so example2.r
\fR
.SP 2
.ll -5
.ce
Figure 2
.ce
Debug Disable Techniques
.ll +5
.SP 2
.DE

.P
Each technique has its advantages and disadvantages with respect
to dynamic vs static activation, source code overhead, recompilation
requirements, ease of use, program readability, etc.
Overuse of the preprocessor solution quickly leads to problems with
source code readability and maintainability when multiple 
.B #ifdef
symbols are to be defined or undefined based on specific types
of debug desired.
The source code can be made slightly more readable by suitable indentation
of the 
.B #ifdef
arguments to match the indentation of the code, but
not all C preprocessors allow this.
The only requirement for the standard 
.B UNIX
C preprocessor is for the '#' character to appear
in the first column, but even this seems
like an arbitrary and unreasonable restriction.
Figure 3 is an example of this usage.
.DS I N
.SP 2
\fC
.so example3.r
\fR
.SP 2
.ll -5
.ce
Figure 3
.ce
More Readable Preprocessor Usage
.ll +5
.SP 2
.DE

.SK
.B
FUNCTION TRACE EXAMPLE
.R

.P
We will start off learning about the capabilities of the
.I dbug
package by using a simple minded program which computes the
factorial of a number.
In order to better demonstrate the function trace mechanism, this
program is implemented recursively.
Figure 4 is the main function for this factorial program.
.DS I N
.SP 2
\fC
.so main.r
\fR
.SP 2
.ll -5
.ce
Figure 4
.ce
Factorial Program Mainline
.ll +5
.SP 2
.DE

.P
The 
.B main
function is responsible for processing any command line
option arguments and then computing and printing the factorial of
each non-option argument.
.P
First of all, notice that all of the debugger functions are implemented
via preprocessor macros.
This does not detract from the readability of the code and makes disabling
all debug compilation trivial (a single preprocessor symbol, 
.B DBUG_OFF ,
forces the macro expansions to be null).
.P
Also notice the inclusion of the header file
.B dbug.h
from the local header file directory.
(The version included here is the test version in the dbug source
distribution directory).
This file contains all the definitions for the debugger macros, which
all have the form 
.B DBUG_XX...XX .

.P
The 
.B DBUG_ENTER 
macro informs that debugger that we have entered the
function named 
.B main .
It must be the very first "executable" line in a function, after
all declarations and before any other executable line.
The 
.B DBUG_PROCESS
macro is generally used only once per program to
inform the debugger what name the program was invoked with.
The
.B DBUG_PUSH
macro modifies the current debugger state by
saving the previous state and setting a new state based on the
control string passed as its argument.
The
.B DBUG_PRINT
macro is used to print the values of each argument
for which a factorial is to be computed.
The 
.B DBUG_RETURN
macro tells the debugger that the end of the current
function has been reached and returns a value to the calling
function.
All of these macros will be fully explained in subsequent sections.
.P
To use the debugger, the factorial program is invoked with a command
line of the form:
.DS CB N
\fCfactorial -#d:t 1 2 3
.DE
The 
.B main
function recognizes the "-#d:t" string as a debugger control
string, and passes the debugger arguments ("d:t") to the 
.I dbug
runtime support routines via the
.B DBUG_PUSH 
macro.
This particular string enables output from the
.B DBUG_PRINT
macro with the 'd' flag and enables function tracing with the 't' flag.
The factorial function is then called three times, with the arguments
"1", "2", and "3".
Note that the DBUG_PRINT takes exactly
.B two
arguments, with the second argument (a format string and list
of printable values) enclosed in parenthesis.
.P
Debug control strings consist of a header, the "-#", followed
by a colon separated list of debugger arguments.
Each debugger argument is a single character flag followed
by an optional comma separated list of arguments specific
to the given flag.
Some examples are:
.DS CB N
\fC
-#d:t:o
-#d,in,out:f,main:F:L
.DE
Note that previously enabled debugger actions can be disabled by the
control string "-#".

.P
The definition of the factorial function, symbolized as "N!", is
given by:
.DS CB N
N! = N * N-1 * ... 2 * 1
.DE
Figure 5 is the factorial function which implements this algorithm
recursively.
Note that this is not necessarily the best way to do factorials
and error conditions are ignored completely.
.DS I N
.SP 2
\fC
.so factorial.r
\fR
.SP 2
.ll -5
.ce
Figure 5
.ce
Factorial Function
.ll +5
.SP 2
.DE

.P
One advantage (some may not consider it so) to using the
.I dbug
package is that it strongly encourages fully structured coding
with only one entry and one exit point in each function.
Multiple exit points, such as early returns to escape a loop,
may be used, but each such point requires the use of an
appropriate 
.B DBUG_RETURN
or
.B DBUG_VOID_RETURN
macro.

.P
To build the factorial program on a 
.B UNIX
system, compile and
link with the command:
.DS CB N
\fCcc -o factorial main.c factorial.c -ldbug
.DE
The "-ldbug" argument tells the loader to link in the
runtime support modules for the
.I dbug
package.
Executing the factorial program with a command of the form:
.DS CB N
\fCfactorial 1 2 3 4 5
.DE
generates the output shown in figure 6.
.DS I N
.SP 2
\fC
.so output1.r
\fR
.SP 2
.ll -5
.ce
Figure 6
.ce
\fCfactorial 1 2 3 4 5
.ll +5
.SP 2
.DE

.P
Function level tracing is enabled by passing the debugger
the 't' flag in the debug control string.
Figure 7 is the output resulting from the command
"factorial\ -#t:o\ 2\ 3".
.DS I N
.SP 2
\fC
.so output2.r
\fR
.SP 2
.ll -5
.ce
Figure 7
.ce
\fCfactorial -#t:o 2 3
.ll +5
.SP 2
.DE

.P
Each entry to or return from a function is indicated by '>' for the
entry point and '<' for the exit point, connected by
vertical bars to allow matching points to be easily found
when separated by large distances.

.P
This trace output indicates that there was an initial call
to factorial from main (to compute 2!), followed by
a single recursive call to factorial to compute 1!.
The main program then output the result for 2! and called the
factorial function again with the second argument, 3.
Factorial called itself recursively to compute 2! and 1!, then
returned control to main, which output the value for 3! and exited.

.P
Note that there is no matching entry point "main>" for the
return point "<main" because at the time the 
.B DBUG_ENTER
macro was reached in main, tracing was not enabled yet.
It was only after the macro
.B DBUG_PUSH
was executing that tracing became enabled.
This implies that the argument list should be processed as early as
possible since all code preceding the first call to
.B DBUG_PUSH 
is
essentially invisible to 
.B dbug
(this can be worked around by
inserting a temporary 
.B DBUG_PUSH(argv[1])
immediately after the
.B DBUG_ENTER("main")
macro.

.P
One last note,
the trace output normally comes out on the standard error.
Since the factorial program prints its result on the standard
output, there is the possibility of the output on the terminal
being scrambled if the two streams are not synchronized.
Thus the debugger is told to write its output on the standard
output instead, via the 'o' flag character.
Note that no 'o' implies the default (standard error), a 'o' 
with no arguments means standard output, and a 'o' 
with an argument means used the named file.
i.e, "factorial\ -#t:o,logfile\ 3\ 2" would write the trace
output in "logfile".
Because of 
.B UNIX
implementation details, programs usually run
faster when writing to stdout rather than stderr, though this
is not a prime consideration in this example.

.SK
.B
USE OF DBUG_PRINT MACRO
.R

.P
The mechanism used to produce "printf" style output is the
.B DBUG_PRINT
macro.

.P
To allow selection of output from specific macros, the first argument
to every 
.B DBUG_PRINT
macro is a 
.I dbug
keyword.
When this keyword appears in the argument list of the 'd' flag in
a debug control string, as in "-#d,keyword1,keyword2,...:t",
output from the corresponding macro is enabled.
The default when there is no 'd' flag in the control string is to
enable output from all 
.B DBUG_PRINT
macros.

.P
Typically, a program will be run once, with no keywords specified,
to determine what keywords are significant for the current problem
(the keywords are printed in the macro output line).
Then the program will be run again, with the desired keywords,
to examine only specific areas of interest.

.P
The second argument to a
.B DBUG_PRINT 
macro is a standard printf style
format string and one or more arguments to print, all
enclosed in parenthesis so that they collectively become a single macro
argument.
This is how variable numbers of printf arguments are supported.
Also note that no explicit newline is required at the end of the format string.
As a matter of style, two or three small 
.B DBUG_PRINT
macros are preferable
to a single macro with a huge format string.
Figure 8 shows the output for default tracing and debug.
.DS I N
.SP 2
\fC
.so output3.r
\fR
.SP 2
.ll -5
.ce
Figure 8
.ce
\fCfactorial -#d:t:o 3
.ll +5
.SP 2
.DE

.P
The output from the 
.B DBUG_PRINT
macro is indented to match the trace output
for the function in which the macro occurs.
When debugging is enabled, but not trace, the output starts at the left
margin, without indentation.

.P
To demonstrate selection of specific macros for output, figure
9 shows the result when the factorial program is invoked with
the debug control string "-#d,result:o".
.DS I N
.SP 2
\fC
.so output4.r
\fR
.SP 2
.ll -5
.ce
Figure 9
.ce
\fCfactorial -#d,result:o 4
.ll +5
.SP 2
.DE

.P
It is sometimes desirable to restrict debugging and trace actions
to a specific function or list of functions.
This is accomplished with the 'f' flag character in the debug
control string.
Figure 10 is the output of the factorial program when run with the
control string "-#d:f,factorial:F:L:o".
The 'F' flag enables printing of the source file name and the 'L'
flag enables printing of the source file line number.
.DS I N
.SP 2
\fC
.so output5.r
\fR
.SP 2
.ll -5
.ce
Figure 10
.ce
\fCfactorial -#d:f,factorial:F:L:o 3
.ll +5
.SP 2
.DE

.P
The output in figure 10 shows that the "find" macro is in file
"factorial.c" at source line 8 and the "result" macro is in the same
file at source line 12.

.SK
.B
SUMMARY OF MACROS
.R

.P
This section summarizes the usage of all currently defined macros
in the 
.I dbug
package.
The macros definitions are found in the user include file
.B dbug.h
from the standard include directory.

.SP 2
.BL 20
.LI DBUG_ENTER\ 
Used to tell the runtime support module the name of the function
being entered.
The argument must be of type "pointer to character".
The 
DBUG_ENTER
macro must precede all executable lines in the
function just entered, and must come after all local declarations.
Each 
DBUG_ENTER
macro must have a matching 
DBUG_RETURN 
or
DBUG_VOID_RETURN
macro 
at the function exit points.
DBUG_ENTER 
macros used without a matching 
DBUG_RETURN 
or 
DBUG_VOID_RETURN
macro 
will cause warning messages from the 
.I dbug
package runtime support module.
.SP 1
EX:\ \fCDBUG_ENTER\ ("main");\fR
.SP 1
.LI DBUG_RETURN\ 
Used at each exit point of a function containing a 
DBUG_ENTER 
macro
at the entry point.
The argument is the value to return.
Functions which return no value (void) should use the 
DBUG_VOID_RETURN
macro.
It 
is an error to have a 
DBUG_RETURN 
or 
DBUG_VOID_RETURN 
macro in a function
which has no matching 
DBUG_ENTER 
macro, and the compiler will complain
if the macros are actually used (expanded).
.SP 1
EX:\ \fCDBUG_RETURN\ (value);\fR
.br
EX:\ \fCDBUG_VOID_RETURN;\fR
.SP 1
.LI DBUG_PROCESS\ 
Used to name the current process being executed.
A typical argument for this macro is "argv[0]", though
it will be perfectly happy with any other string.
.SP 1
EX:\ \fCDBUG_PROCESS\ (argv[0]);\fR
.SP 1
.LI DBUG_PUSH\ 
Sets a new debugger state by pushing the current
.B dbug
state onto an
internal stack and setting up the new state using the debug control
string passed as the macro argument.
The most common usage is to set the state specified by a debug
control string retrieved from the argument list.
Note that the leading "-#" in a debug control string specified
as a command line argument must
.B not
be passed as part of the macro argument.
The proper usage is to pass a pointer to the first character
.B after
the "-#" string.
.SP 1
EX:\ \fCDBUG_PUSH\ (\&(argv[i][2]));\fR
.br
EX:\ \fCDBUG_PUSH\ ("d:t");\fR
.br
EX:\ \fCDBUG_PUSH\ ("");\fR
.SP 1
.LI DBUG_POP\ 
Restores the previous debugger state by popping the state stack.
Attempting to pop more states than pushed will be ignored and no
warning will be given.
The 
DBUG_POP 
macro has no arguments.
.SP 1
EX:\ \fCDBUG_POP\ ();\fR
.SP 1
.LI DBUG_FILE\ 
The 
DBUG_FILE 
macro is used to do explicit I/O on the debug output
stream.
It is used in the same manner as the symbols "stdout" and "stderr"
in the standard I/O package.
.SP 1
EX:\ \fCfprintf\ (DBUG_FILE,\ "Doing\ my\ own\ I/O!\\n");\fR
.SP 1
.LI DBUG_EXECUTE\ 
The DBUG_EXECUTE macro is used to execute any arbitrary C code.
The first argument is the debug keyword, used to trigger execution
of the code specified as the second argument.
This macro must be used cautiously because, like the 
DBUG_PRINT 
macro,
it is automatically selected by default whenever the 'd' flag has
no argument list (i.e., a "-#d:t" control string).
.SP 1
EX:\ \fCDBUG_EXECUTE\ ("status",\ print_status\ ());\fR
.SP 1
.LI DBUG_EXECUTE_IF\ 
Works like DBUG_EXECUTE macro, but the code is
.B not
executed "by default", if the keyword is not explicitly listed in
the 'd' flag. Used to conditionally execute "dangerous" actions, e.g
to crash the program testing how recovery works, or to introduce an
artificial delay checking for race conditions.
.SP 1
EX:\ \fCDBUG_EXECUTE_IF\ ("crashme",\ abort\ ());\fR
.SP 1
.LI DBUG_N\ 
These macros, where N is in the range 2-5, are currently obsolete
and will be removed in a future release.
Use the new DBUG_PRINT macro.
.LI DBUG_PRINT\ 
Used to do printing via the "fprintf" library function on the
current debug stream,
DBUG_FILE.
The first argument is a debug keyword, the second is a format string
and the corresponding argument list.
Note that the format string and argument list are all one macro argument
and
.B must
be enclosed in parenthesis.
.SP 1
EX:\ \fCDBUG_PRINT\ ("eof",\ ("end\ of\ file\ found"));\fR
.br
EX:\ \fCDBUG_PRINT\ ("type",\ ("type\ is\ %x", type));\fR
.br
EX:\ \fCDBUG_PRINT\ ("stp",\ ("%x\ ->\ %s", stp, stp\ ->\ name));\fR
.SP 1
.LI DBUG_DUMP\ 
Used to dump a memory block in hex via the "fprintf" library function on the
current debug stream, DBUG_FILE.
The first argument is a debug keyword, the second is a pointer to
a memory to dump, the third is a number of bytes to dump.
.SP 1
EX: \fCDBUG_DBUG\ ("net",\ packet,\ len);\fR
.SP 1
.LI DBUG_SETJMP\ 
Used in place of the setjmp() function to first save the current
debugger state and then execute the standard setjmp call.
This allows to the debugger to restore it's state when the
DBUG_LONGJMP macro is used to invoke the standard longjmp() call.
Currently all instances of DBUG_SETJMP must occur within the
same function and at the same function nesting level.
.SP 1
EX: \fCDBUG_SETJMP\ (env);\fR
.SP 1
.LI DBUG_LONGJMP\ 
Used in place of the longjmp() function to first restore the
previous debugger state at the time of the last DBUG_SETJMP
and then execute the standard longjmp() call.
Note that currently all DBUG_LONGJMP macros restore the state
at the time of the last DBUG_SETJMP.
It would be possible to maintain separate DBUG_SETJMP and DBUG_LONGJMP
pairs by having the debugger runtime support module use the first
argument to differentiate the pairs.
.SP 1
EX: \fCDBUG_LONGJMP\ (env,val);\fR
.SP 1
.LI DBUG_LOCK_FILE\ 
Used in multi-threaded environment to lock DBUG_FILE stream.
It can be used, for example, in functions that need to write something to a
debug stream more than in one fprintf() call and want to ensure that no other
thread will write something in between.
.SP 1
EX:\fC
.br
  DBUG_LOCK_FILE;
.br
  fprintf (DBUG_FILE, "a=[");
.br
  for (int i=0; i < a_length; i++)
.br
    fprintf (DBUG_FILE, "0x%03x ", a[i]);
.br
  fprintf (DBUG_FILE, "]");
.br
  DBUG_UNLOCK_FILE;\fR
.SP 1
.LI DBUG_UNLOCK_FILE\ 
Unlocks DBUG_FILE stream, that was locked with a DBUG_LOCK_FILE.
.LI DBUG_ASSERT\ 
This macro just does a regular assert(). The difference is that it will be
disabled by DBUG_OFF togeher with the
.I dbug
library. So there will be no need to disable asserts separately with NDEBUG.
.SP 1
EX:\ \fCDBUG_ASSERT(\ a\ >\ 0\ );\fR
.SP 1
.LI DBUG_OUTPUT\ 
In multi-threaded environment disables (or enables) any
.I dbug
output from the current thread.
.SP 1
EX:\ \fCDBUG_OUTPUT(\ 0\ );\fR
.SP 1
.LE

.SK
.B
DEBUG CONTROL STRING
.R

.P
The debug control string is used to set the state of the debugger
via the 
.B DBUG_PUSH 
macro.
This section summarizes the currently available debugger options
and the flag characters which enable or disable them.
Argument lists enclosed in '[' and ']' are optional.
.SP 2
.BL 22
.LI a[,file]
Redirect the debugger output stream and append it to the specified file.
The default output stream is stderr.
A null argument list causes output to be redirected to stdout.
Double the colon, if you want it in the path
.SP 1
EX: \fCa,C::\\tmp\\log\fR
.LI A[,file]
Like 'a[,file]' but ensure that data are written after each write
(this typically implies flush or close/reopen). It helps to get
a complete log file in case of crashes. This mode is implied in
multi-threaded environment.
.LI d[,keywords]
Enable output from macros with specified keywords.
A null list of keywords implies that all keywords are selected.
.LI D[,time]
Delay for specified time after each output line, to let output drain.
Time is given in tenths of a second (value of 10 is one second).
Default is zero.
.LI f[,functions]
Limit debugger actions to the specified list of functions.
A null list of functions implies that all functions are selected.
.LI F
Mark each debugger output line with the name of the source file
containing the macro causing the output.
.LI i
Mark each debugger output line with the PID of the current process.
.LI g,[functions]
Enable profiling for the specified list of functions.
By default profiling is enabled for all functions.
See
.B PROFILING\ WITH\ DBUG
below.
.LI L
Mark each debugger output line with the source file line number of
the macro causing the output.
.LI n
Mark each debugger output line with the current function nesting depth.
.LI N
Sequentially number each debugger output line starting at 1.
This is useful for reference purposes when debugger output is
interspersed with program output.
.LI o[,file]
Like 'a[,file]' but overwrite old file, do not append.
.LI O[,file]
Like 'A[,file]' but overwrite old file, do not append.
.LI p[,processes]
Limit debugger actions to the specified processes.
A null list implies all processes.
This is useful for processes which run child processes.
Note that each debugger output line can be marked with the name of
the current process via the 'P' flag.
The process name must match the argument passed to the
.B DBUG_PROCESS
macro.
.LI P
Mark each debugger output line with the name of the current process.
Most useful when used with a process which runs child processes that
are also being debugged.
Note that the parent process must arrange for the debugger control
string to be passed to the child processes.
.LI r
Used in conjunction with the 
.B DBUG_PUSH 
macro to reset the current
indentation level back to zero.
Most useful with 
.B DBUG_PUSH 
macros used to temporarily alter the
debugger state.
.LI S
When compiled with
.I safemalloc
this flag forces "sanity" memory checks (for overwrites/underwrites)
on each
.B DBUG_ENTER
and
.B DBUG_RETURN.
.LI t[,N]
Enable function control flow tracing.
The maximum nesting depth is specified by N, and defaults to
200.
.LE
.SK
.B
PROFILING WITH DBUG
.R

.P
With
.I dbug
one can do profiling in a machine independent fashion,
without a need for profiled version of system libraries.
For this,
.I dbug
can write out a file
called
.B dbugmon.out
(by default).  This is an ascii file containing lines of the form:
.DS CB N
\fC<function-name> E <time-entered>
<function-name> X <time-exited>
.DE

.P
A second program (\fBanalyze\fR) reads this file, and produces a report on
standard output.

.P
Profiling is enabled through the 
.B g
flag.  It can take a list of
function names for which profiling is enabled.  By default, it
profiles all functions.

.P
The profile file is opened for appending.  This
is in order that one can run a program several times, and get the
sum total of all the times, etc.

.P
An example of the report generated follows:
.DS CB N
\fC
            Profile of Execution
            Execution times are in milliseconds

                Calls                        Time
                -----                        ----
            Times   Percentage   Time Spent    Percentage
Function    Called  of total     in Function   of total    Importance
========    ======  ==========   ===========   ==========  ==========
factorial        5       83.33            30       100.00        8333
main             1       16.67             0         0.00           0
========    ======  ==========   ===========   ==========
Totals           6      100.00            30       100.00
.DE
.P
As you can see, it's quite self-evident.  The 
.B Importance
column is a
metric obtained by multiplying the percentage of the calls and the percentage
of the time.  Functions with higher 'importance' benefit the most from
being sped up.

.P
As a limitation - setjmp/longjmp, or child processes, are ignored
for the time being. Also, profiling does not work
in a multi-threaded environment.

.P
Profiling code is (c) Binayak Banerjee.

.SK
.B
HINTS AND MISCELLANEOUS
.R

.P
One of the most useful capabilities of the 
.I dbug 
package is to compare the executions of a given program in two
different environments.
This is typically done by executing the program in the environment
where it behaves properly and saving the debugger output in a
reference file.
The program is then run with identical inputs in the environment where 
it misbehaves and the output is again captured in a reference file.
The two reference files can then be differentially compared to
determine exactly where execution of the two processes diverges.

.P
A related usage is regression testing where the execution of a current
version is compared against executions of previous versions.
This is most useful when there are only minor changes.

.P
It is not difficult to modify an existing compiler to implement
some of the functionality of the 
.I dbug
package automatically, without source code changes to the
program being debugged.
In fact, such changes were implemented in a version of the
Portable C Compiler by the author in less than a day.
However, it is strongly encouraged that all newly
developed code continue to use the debugger macros
for the portability reasons noted earlier.
The modified compiler should be used only for testing existing
programs.

.SK
.B
CAVEATS
.R

.P
The 
.I dbug
package works best with programs which have "line\ oriented"
output, such as text processors, general purpose utilities, etc.
It can be interfaced with screen oriented programs such as
visual editors by redefining the appropriate macros to call
special functions for displaying the debugger results.
Of course, this caveat is not applicable if the debugger output
is simply dumped into a file for post-execution examination.

.P
Programs which use memory allocation functions other than
.B malloc
will usually have problems using the standard
.I dbug
package.
The most common problem is multiply allocated memory.
.SP 2
.\" .DE nroff dident like this. davida 900108
.CS

.\" vim:filetype=nroff
