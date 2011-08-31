#!/bin/sh

# Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA

die()
{
  echo "ERROR: $@"; exit 1;
}

get_key_value()
{
  echo "$1" | sed 's/^--[a-zA-Z_-]*=//'
}

developer_usage()
{
cat <<EOF

  This script can be used by developers of MySQL, early adopters wanting 
  to try out early versions of MySQL before binary versions are 
  available, anyone needing a version with a special patch included that 
  needs to be built from source code, or anyone else wanting to exercise 
  full control over the MySQL build process.

  This help text is targeted towards those that want to debug and test 
  MySQL using source code releases. If you have downloaded a source code 
  release and simply want to build a usable binary, you should read the 
  --sysadmin-help instead.

  The script is also designed to be used by anyone receiving a source 
  code release of MySQL Cluster Carrier Grade Edition. The default
  behaviour is to build the standard MySQL Cluster Carrier Grade Edition
  package. Three environment variables can be used to change the 
  default behaviour:

  MYSQL_DEVELOPER
    Defining this variable is similar to setting the --developer flag
  MYSQL_DEVELOPER_PACKAGE=package
    Defining this variable is similar to setting the --package=* 
    variable
  MYSQL_DEVELOPER_DEBUG
    Defining this variable sets the --with-debug flag

  Options used with this script always override any default behaviour. 
  The default package is MySQL Cluster Carrier Grade (standard) Edition. 
  For developers, the default package is MySQL Cluster Carrier Grade 
  Extended Edition.

  More information for developers can be found in --help, 
  --sysadmin-help, and --extended-help.

  The most common usage for developers is to set the three variables 
  mentioned previously and then simply to run the script without any 
  additional parameters, using them only when needing to change some 
  things like not requiring debug build. If some of these environment 
  variables have already been set, you can use the corresponding options 
  to unset them and reverse their effects.
EOF
}

sysadmin_usage()
{
cat <<EOF

  This script can be used to build MySQL Cluster Carrier Grade Edition
  based on a source code release you received from MySQL. It can also
  be used to build many variants other variants of MySQL, in particular
  various performance-optimised versions of MySQL.

  It is assumed that you are building on a computer which is of the 
  same type as that on which you intend to run MySQL/MySQL Cluster.

  The simplest possible way to run this script is to allow it to use the 
  built-in defaults everywhere, invoking it simply as (from top-level
  MySQL directory):

  shell> BUILD/build_mccge.sh

  This performs the following operations:
    1) Detects the operating system. Currently, Linux, FreeBSD, Solaris 
      8/9/10/11, and Mac OS X are supported by this script.
    2) Detect the type of CPU being used. Currently supported processors 
      are: x86 for all supported operating systems, Itanium for Linux 
      with GCC, and x86 + SPARC for Solaris using the Forte compiler and
      finally x86 on Linux using the Intel compiler.
    3) Invokes the GCC compiler.
    4) Builds a set of MySQL/MySQL Cluster binaries; for
      more information about these, see --extended-help.
    5) Default compiler is always gcc.
  
  The default version assumes that you have a source code tarball from 
  which you are building, and thus autoconf and automake do not need to 
  be run. If you have downloaded a BitKeeper tree then you should read 
  --developer-help.

  If you are building MySQL/MySQL Cluster for commercial 
  use then you need to set the --commercial flag to ensure that the 
  commercial libraries are compiled in, rather than the GPL-only 
  libraries. The default is to build a GPL version of MySQL Cluster 
  Carrier Grade Edition.

  If your building on a Solaris SPARC machine and you want to compile
  using SunStudio you must set 
  --compiler=forte; if you want to build using the Intel compiler on 
  Linux, you need to set --compiler=icc. If you want to use the AMD
  compiler Open64 set --compiler=open64.

  A synonym for forte is SunStudio, so one can also use
  --compiler=SunStudio.

  If you want to make sure that a 64-bit version is built then you 
  should add the flag --64. This is always set on Solaris machines and 
  when check-cpu is able to discover that a 64-bit CPU is being used. If 
  you want to ensure that a 32-bit binary is produced, use --32 instead.

  If you need the binaries to be installed in a different location from 
  /usr/local/mysql, then you should set --prefix to point to where you
  want the binaries installed.

  Using a data directory other than the default (PREFIX/data) can be 
  done when starting the MySQL Server, or by invoking this script with 
  the --datadir option.

  If you want your binaries stripped of surplus debug or other 
  information, use the --strip option.

  If you want debug information in the binary (for example, to be
  able to send gdb core dumps to MySQL Support), then you should add the 
  flag --with-debug; if you want a production build with only debugging 
  information in the binary then use --debug.

  If your aim is not to build MySQL Cluster Carrier Grade Edition, you 
  can also use this script to build MySQL Classic and MySQL Enterprise Pro 
  versions; see the --extended-help for descriptions of these packages.
EOF
}

usage()
{
cat <<EOF

Usage: $0 [options]
  --help                  Show this help message.
  --sysadmin-help         Show help for system administrators wishing 
                          to build MySQL Cluster Carrier Grade Edition
                          or other MySQL versions.
  --developer-help        Show help for developers trying to build MySQL
  --with-help             Show extended help on --with-xxx options to 
                          configure
  --extended-help         Show extended help message
  --without-debug         Build non-debug version
  --use-comment           Set the comment in the build
  --with-fast-mutexes     Use try/retry method of acquiring mutex
  --without-fast-mutexes  Don't use try/retry method of acquiring mutex
  --without-perfschema    Don't build with performance schema
  --generate-feedback path Compile with feedback using the specified directory
                          to store the feedback files
  --use-feedback path     Compile using feedback information from the specified
                          directory
  --with-debug            Build debug version
  --extra-debug-flag flag Add -Dflag to compiler flags
                          InnoDB supports the following debug flags,
                          UNIV_DEBUG, UNIV_SYNC_DEBUG, UNIV_MEM_DEBUG,
                          UNIV_DEBUG_THREAD_CREATION, UNIV_DEBUG_LOCK_VALIDATE,
                          UNIV_DEBUG_PRINT, UNIV_DEBUG_FILE_ACCESS,
                          UNIV_LIGHT_MEM_DEBUG, UNIV_LOG_DEBUG,
                          UNIV_IBUF_COUNT_DEBUG, UNIV_SEARCH_DEBUG,
                          UNIV_LOG_LSN_DEBUG, UNIV_ZIP_DEBUG, UNIV_AHI_DEBUG,
                          UNIV_DEBUG_VALGRIND, UNIV_SQL_DEBUG, UNIV_AIO_DEBUG,
                          UNIV_BTR_DEBUG, UNIV_LRU_DEBUG, UNIV_BUF_DEBUG,
                          UNIV_HASH_DEBUG, UNIV_LIST_DEBUG, UNIV_IBUF_DEBUG
  --with-link-time-optimizer
                          Link time optimizations enabled (Requires GCC 4.5
                          if GCC used), available for icc as well. This flag
                          is only considered if also fast is set.
  --with-mso              Special flag used by Open64 compiler (requres at
                          least version 4.2.3) that enables optimisations
                          for multi-core scalability.
  --configure-only        Stop after running configure.
  --print-only            Print commands that the script will execute, 
                          but do not actually execute
  --prefix=path           Build with prefix 'path'
  --datadir=path          Build with data directory set to non-standard 
                          'path'
  --debug                 Build normal version, but add debug 
                          information to binary
  --developer             Use extensions that most MySQL developers use
  --no-developer          Do not use extensions that most developers of 
                          MySQL use
  --commercial            Use commercial libraries
  --gpl                   Use gpl libraries
  --compiler=[gcc|icc|forte|SunStudio|open64] Select compiler
  --cpu=[x86|x86_64|sparc|itanium]            Select CPU type
                          x86 => x86 and 32-bit binary
                          x86_64 => x86 and 64 bit binary
  --warning-mode=[extra|pedantic|normal|no]   Set warning mode level
  --warnings              Set warning mode to normal
  --32                    Build a 32-bit binary even if CPU is 64-bit
  --64                    Build a 64-bit binary even if not sure a
                          64-bit CPU is being used
  --package=[cge|extended|pro|classic]        Select package to build
  --parallelism=number    Define parallelism in make
  --strip                 Strip binaries
  --error-inject          Enable error injection into MySQL Server and 
                          data nodes
  --valgrind              Build with valgrind
  --fast                  Optimise for CPU architecture built on
  --static-linking        Statically link system libraries into binaries
  --use-tcmalloc          Link with tcmalloc instead of standard malloc (Linux only)
  --with-flags *          Pass extra --with-xxx options to configure
EOF
  if test "x$1" != "x" ; then
    echo "Failure Message: $1"
  fi
}

extended_usage()
{
  cat <<EOF

  Extended help text for this script:
  -----------------------------------
  This script is intended to make it easier for customers using MySQL
  Cluster Carrier Grade Edition, customers using performance-optimised
  MySQL versions and developers to build the product from source on 
  these platforms/compilers: Linux/x86 (32-bit and 64-bit) (either using
  gcc or icc), Linux Itanium, Solaris 8,9,10 and 11 x86 and SPARC using
  gcc or SunStudio and MacOSX/x86/gcc.

  The script automatically detects CPU type and operating system; The
  default compiler is always gcc.

  To build on other platforms you can use the --print-only option on a
  supported platform and edit the output for a proper set of commands on
  the specific platform you are using. MySQL also provides custom builds
  for any type of platform that is officially supported for MySQL
  Cluster. For a list of supported platforms, see
  http://www.mysql.com/support/supportedplatforms/cluster.html.

  Using the --package option, it is also possible to build a "classic"
  version of MySQL having only the MyISAM storage engine, a "Pro"
  package including all storage engines and other features except MySQL
  Cluster, and an "extended" package including these features plus MySQL
  Cluster (this is the default if the --developer option is used).

  Different MySQL storage engines are included in the build, depending
  on which --package option is used. The comment and version strong
  suffix are also set according to the package selected.

  --package=cge
    storage engines:
      ARCHIVE, BLACKHOLE, CSV, FEDERATED, MYISAM, NDB
      (All storage engines except InnoDB)
    comment: MySQL Cluster Carrier Grade Edition GPL/Commercial version 
             built from source
    version string suffix: -cge

  --package=extended
    storage engines:
      ARCHIVE, BLACKHOLE, CSV, FEDERATED, MYISAM, INNODB, NDB
      (All storage engines)
    comment: MySQL Cluster Carrier Grade Extended Edition GPL/Commercial 
             version built from source
    version string suffix: -cge-extended

  --package=pro
    storage engines:
      ARCHIVE, BLACKHOLE, CSV, FEDERATED, INNODB, MYISAM
      (All storage engines except NDB)
    comment: MySQL Pro GPL/Commercial version built from 
             source
    version string suffix: [none]

  --package=classic
    storage engines: CSV, MYISAM
    comment: MySQL Classic GPL/Commercial version built 
             from source
    version string suffix: [none]

  All packages except Classic include support for user-defined
  partitioning. All packages include support for Performance
  Schema.

  If --with-debug is used, an additional "-debug" is appended to the
  version string.

  --commercial
    This flag prevents the use of GPL libraries which cannot be used
    under a commercial license, such as the readline library.

  --developer
    This option changes a number of things to make the version built
    more appropriate to the debugging and testing needs of developers. 
    It changes the default package to "extended". It also changes the 
    default warning mode from "none" to "normal", which allows an 
    extensive list of warnings to be generated.

  --error-inject
    This flag is used only when the --developer option is also used, and
    enables error injection in both the MySQL Server and in MySQL
    Cluster data nodes.

  The following is a list of the default configure options used for all
  packages:

  --prefix: /usr/local/mysql (can be overridden)

  --libexecdir: <prefix>/bin (can be overridden)

  --localstatedir: <prefix>/data, unless --datadir is used, in which
    case it defaults to <datadir>/data (can be overridden by setting
    --localstatedir explicitly).

  --enable-local-infile: Enable use of the LOAD DATA FROM  LOCAL INFILE
    command (cannot be overridden).

  --enable-thread-safe-client: Enable the multi-threaded mysql client
    library (cannot be overridden).

  --with-big-tables: Enable use of tables with more than 4G rows (cannot
    be overridden).

  --with-extra-charsets=all: Enable use of all character sets supported
    by MySQL (cannot be overridden).

  --with-ssl: Enable use of yaSSL library included in the MySQL source
    if possible (GCC and same CC and CXX).
    (cannot be overridden).

  --with-pic: Build all binaries using position independent assembler
    to avoid problems with dynamic linkers (cannot be overridden).

  --without-example-engine: Ensure that the example engine isn't built,
    it cannot do any useful things, it's merely intended as documentation.
    (cannot be overridden)

  --with-csv-storage-engine: Ensure that the CSV storage engine is
    included in all builds. Since CSV is required for log tables in
    MySQL 5.1, this option cannot be overridden.

    (Note that MyISAM support is always built into the MySQL Server; the
    server *cannot* be built without MyISAM.)

  --with-mysqld-ldflags=-static
  --with-client-ldflags=-static
    Ensures that binaries for, respectively, the MySQL server and client
    are built with static libraries except for the system libraries,
    which use dynamically loaded libraries provided by the operating
    system. Building with --developer sets these to all-static instead, 
    to build everything statically.

  In addition there are some configure options that are specific to
  Linux operating systems:

  --enable-assembler
    Include assembler code optimisations for a number of mostly string
    methods. Used for x86 processors only.

  Neither of the preceding options can be disabled.

  MySQL Cluster Carrier Grade edition also adds the following options
  (also used by the extended package):

  --with-ndbcluster
    Include the NDB Cluster storage engine, its kernel, management
    server, and client, as well as support for the NDB and MGM APIs.

  --without-ndb-debug
    Do not include specific NDB debug code, not even in debug versions
    (cannot be overridden).

  Package-specific options:
  -------------------------
  --with-innodb
    Specifically included in the "pro" and "extended" packages, and not 
    in any of the others.

  --with-comment
    Sets the comment for the MySQL version, by package, as described
    above.

  --with-server-suffix
    Sets the server suffix on the MySQL version, by package, as
    described above.

  Other options used:
  -------------------
  --with-readline
    Use the GPL readline library for command editing functions; not
    available with commercial packages.

  --with-libedit
    Use the BSD licensed library for command editing functions; used for
    commercial packages.

  --with-zlib-dir=bundled
    Use the zlib package bundled with MySQL.

  --with-mysqld-libs=-lmtmalloc
    Used on Solaris to ensure that the proper malloc library is used.
    Investigations have shown mtmalloc to be the best choice on Solaris,
    also umem has good performance on Solaris but better debugging
    capabilities.

  Compiler options:
  -----------------

    This section describes the compiler options for each of the different
    platforms supported by this script.

    The --fast option adds -mtune=cpu_arg to the C/C++ flags (provides
    support for Nocona, K8, and other processors), this option is valid
    when gcc is the compiler.

    Use of the --debug option adds -g to the C/C++ flags.

    In all cases it is possible to override the definition of CC and CXX
    by calling the script as follows:
    CC="/usr/local/bin/gcc" CXX="/usr/local/bin/gcc" BUILD/build_mccge.sh

  Feedback profiler on gcc
  ------------------------
  Using gcc --generate-feedback=path causes the following flags to be added
  to the compiler flags.

  --fprofile-generate
  --fprofile-dir=path

  Using gcc with --use-feedback=path causes the following flags to be added
  to the compiler flags. --fprofile-correction indicates MySQL is a multi-
  threaded application and thus counters can be inconsistent with each other
  and the compiler should take this into account.

  --fprofile-use
  --fprofile-dir=path
  --fprofile-correction

  Feedback compilation using Open64
  ---------------------------------

  Using Open64 with --generate-feedback=path causes the following flags to
  be added to the compiler flags.

  -fb-create path/feedback

  Using Open64 with --use-feedback=path causes the following flags to be
  added to the compiler flags.

  --fb-opt path/feedback

  Linux/x86+Itanium/gcc
  -------------
    For debug builds -O is used and otherwise -O3 is used.Discovery of a
    Nocona or Core 2 Duo CPU causes a 64-bit binary to be built;
    otherwise, the binary is 32-bit. To build a 64-bit binary, -m64 is
    added to the C/C++ flags. (To build a 32-bit binary on a 64-bit CPU,
    use the --32 option as described previously.)

    When gcc 4.5 is used and the user set --with-link-time-optimizer then
    also --flto is added to compiler flags and linker flags.

  Linux/x86+Itanium/icc
  -------------
    Flags used:
    CC  = icc -static-libgcc -static-intel
    C++ = icpc -static-libgcc -static-intel
    C/C++ flags = -mp -restrict

    On Itanium we also add -no-ftz and to CC and C++ flags.

    Note that if the user of this script sets CC or CXX explicitly then
    also -static-libgcc and -static-intel needs to be set in the CC and
    CXX.

    The non-debug versions also add the following:
      C/C++ flags += -O3 unroll2 -ip

    The fast version adds (if --with-link-time-optimizer is used):
      C/C++ flags += -ipo

    On discovery of a Core 2 Duo architecture while using icc, -xT is also 
    added to the C/C++ flags; this provides optimisations specific to Core 
    2 Duo. This is added only when the --fast flag is set.

  Linux/x86/Open64
  ----------------
    For normal builds use -O3, when fast flag is set one also adds
    --march=auto to generate optimized builds for the CPU used. If
    --with-link-time-optimizer is set also -ipa is set. There is also
    a special flag --with-mso which can be set to get --mso set which
    activates optimisation for multi-core scalability.

  FreeBSD/x86/gcc
  ---------------
    No flags are used. Instead, configure determines the proper flags to 
    use.

  Solaris/x86/gcc
  ---------------
    All builds on Solaris are by default 64-bit, so -m64 is always used in
    the C/C++ flags. LDFLAGS is set to -m64 -O/-O2/-O3. If for
    some reason a 32-bit Solaris is used it is necessary to add the flag
    --32 to the script invocation. Due to bugs in compiling with -O3 on
    Solaris only -O2 is used by default, when --fast flag is used -O3 will
    be used instead.

    Sets -m64 (default) or -m32 (if specifically set) in LDFLAGS and
    C/C++ flags.

  Solaris/Sparc/Forte
  -------------------
    Uses cc as CC and CC as CXX
    Note that SunStudio uses different binaries for C and C++ compilers.

    Set -m64 (default) or -m32 (if specifically set) in ASFLAGS,
    LDFLAGS and C/C++ flags.

    Sets ASFLAGS=LDFLAGS=compiler flags=xarch=sparc, so that we compile
    Sparc v9 binaries, also -mt is set in all those since we're always
    building a multithreaded program.

    C flags   = -xstrconst    This flag is set only on SPARC
    C++ flags = -noex

    Set the following C/C++ flags:
    -fsimple=1
    -ftrap=%none
    -nofstore          This flag is set only on x86
    -xbuiltin=%all
    -xlibmil
    -xlibmopt

    Set the C++ flag:
    -noex
    -features=no%except    This flag is set only on x86

    When compiling with fast we set (-ipo only used if we have
    set --with-link-time-optimizer):
    C/C++ flags: -xtarget=native -xunroll=3 -xipo
    LDFLAGS: -xipo

    When not compiling with fast we always set -xtarget=generic

    When compiling with fast on SPARC we also set:
    C/C++ flags: -xbinopt=prepare
    LDFLAGS: -xbinopt=prepare

    When compiling with fast on x86 we also set:
    C/C++ flags: -xregs=frameptr
    When not compiling with fast we set on x86
    C/C++ flags: -xregs=no%frameptr

    On SPARC we set
    ASFLAGS = LDFLAGS = C/C++ flags = -xarch=sparc

    The optimisation level is
    -xO         Debug builds
    -xO2        Production build on SPARC
    -xO3        Production build on x86
    -xO4        Fast builds on SPARC/x86

  MacOSX/x86/gcc
  --------------
    C/C++ flags include -fno-common -arch i386.
    When 64-bits builds then i386 is replaced by x86_64.

    Non-debug versions also add -Os -felide-constructors, where "-Os"
    means the build is space-optimised as long as the space optimisations
    do not negatively affect performance. Debug versions use -O.
  
    Mac OS X builds will always be 32-bit by default, when --64 is added
    the build will be 64 bit instead. Thus the flag --m64 is added only
    when specifically given as an option.
EOF
}

with_usage()
{
  cat <<EOF

  To obtain extended help on the --with-* options available, run this
  script with --configure-only to create a configuration file. Then
  issue the command ./configure --help to get an extensive list of
  possible configure options.

  The remainder of this text focuses on those options which are useful
  in building binaries for MySQL Cluster Carrier Grade Edition.

  --with-ndb-sci=/opt/DIS
    Used to build a MySQL Cluster Carrier Grade Edition that can use the
    SCI Transporter. The Dolphin SCI installation must be completed
    first (see 
    http://dev.mysql.com/doc/refman/5.1/en/mysql-cluster-interconnects.html
    for more information).

  --with-ndb-test
    Compile the MySQL Cluster test programs.

  --with-ndb-port=PORT
    Change the default port for the MySQL Cluster management server.

  --with-ndb-port-base=PORT
    Change the default port base for MySQL Cluster data nodes.

  --without-query-cache
    Build the MySQL Server without the query cache, which is often not
    of value in MySQL Cluster applications.

  --with-atomic-ops=rwlocks|smp|up
                          Implement atomic operations using pthread
                          rwlocks or atomic CPU instructions for
                          multi-processor (default) or single-processor
                          configurations.

  --without-geometry      Do not build geometry-related portions of the
                          MySQL Server. Seldom used in MySQL Cluster
                          applications.

  --with-ndb-cc-flags=FLAGS
    This option can be used to build MySQL Cluster with error injection
    on the data nodes. It can be used to pass special options to
    programs in the NDB kernel for special test builds.
    The option for enabling data node error injection is -DERROR_INSERT.
EOF
}

parse_package()
{
  case "$package" in
    classic )
      package="classic"
      ;;
    pro )
      package="pro"
      ;;
    extended )
      package="extended"
      ;;
    cge )
      package="cge"
      ;;
    *)
      echo "Unknown package '$package'"
      exit 1
      ;;
  esac
}

parse_warning_mode()
{
  case "$warning_mode" in
    pedantic )
      warning_mode="pedantic"
      ;;
    extra_warnings | extra-warnings | extra )
      warning_mode="extra"
      ;;
    no )
      warning_mode=
      ;;
    normal )
      warning_mode="normal"
      ;;
    *)
      echo "Unknown warning mode '$warning_mode'"
      exit 1
      ;;
  esac
}

#
# We currently only support x86, Itanium and UltraSparc processors.
#
parse_cpu_type()
{
  case "$cpu_type" in
    x86 )
      cpu_type="x86"
      if test "x$m64" = "x" ; then
        m64="no"
      fi
      ;;
    x86_64 )
      cpu_type="x86"
      if test "x$m64" = "x" ; then
        m64="yes"
      fi
      ;;
    itanium )
      cpu_type="itanium"
      ;;
    sparc )
      cpu_type="sparc"
      ;;
    * )
      echo "Unknown CPU type $cpu_type"
      exit 1
      ;;
  esac
  return
}

#
# We currently only support gcc, icc and Forte.
#
parse_compiler()
{
  case "$compiler" in
    gcc )
      compiler="gcc"
      ;;
    icc )
      compiler="icc"
      ;;
    forte | SunStudio | sunstudio )
      compiler="forte"
      ;;
    open64 | Open64 )
      compiler="open64"
      ;;
    *)
      echo "Unknown compiler '$compiler'"
      exit 1
      ;;
  esac
}

parse_options()
{
  while test $# -gt 0
  do
    case "$1" in
    --prefix=*)
      prefix=`get_key_value "$1"`
      ;;
    --datadir=*)
      datadir=`get_key_value "$1"`
      ;;
    --with-link-time-optimizer)
      with_link_time_optimizer="yes"
      ;;
    --without-debug)
      with_debug_flag="no"
      if test "x$fast_flag" != "xyes" ; then
        fast_flag="generic"
      fi
      ;;
    --use-comment)
      without_comment="no"
      ;;
    --with-fast-mutexes)
      with_fast_mutexes="yes"
      ;;
    --without-fast-mutexes)
      with_fast_mutexes="no"
      ;;
    --without-perfschema)
      with_perfschema="no"
      ;;
    --with-mso)
      with_mso="yes"
      ;;
    --use-tcmalloc)
      use_tcmalloc="yes"
      ;;
    --with-debug)
      with_debug_flag="yes"
      fast_flag="no"
      ;;
    --extra-debug-flag)
      shift
      extra_debug_flags="$extra_debug_flags -D$1"
      ;;
    --debug)
      compile_debug_flag="yes"
      ;;
    --no-developer)
      developer_flag="no"
      ;;
    --developer)
      developer_flag="yes"
      ;;
    --commercial)
      gpl="no"
      ;;
    --gpl)
      gpl="yes"
      ;;
    --compiler=*)
      compiler=`get_key_value "$1"`
      parse_compiler
      ;;
    --generate-feedback)
      shift
      GENERATE_FEEDBACK_PATH="$1"
      ;;
    --use-feedback)
      shift
      USE_FEEDBACK_PATH="$1"
      ;;
    --cpu=*)
      cpu_type=`get_key_value "$1"`
      parse_cpu_type
      ;;
    --warning-mode=*)
      warning_mode=`get_key_value "$1"`
      parse_warning_mode
      ;;
    --warnings)
      warning_mode="normal"
      ;;
    --32)
      if test "x$explicit_size_set" != "x" ; then
        echo "Cannot set both --32 and --64"
        exit 1
      fi
      explicit_size_set="yes"
      m64="no"
      ;;
    --64)
      if test "x$explicit_size_set" != "x" ; then
        echo "Cannot set both --32 and --64"
        exit 1
      fi
      explicit_size_set="yes"
      m64="yes"
      ;;
    --package=*)
      package=`get_key_value "$1"`
      parse_package
      ;;
    --parallelism=*)
      parallelism=`get_key_value "$1"`
      ;;
    --configure-only)
      just_configure="yes"
      ;;
    --print-only)
      just_print="yes"
      ;;
    --static-linking)
      static_linking_flag="yes"
      ;;
    --strip)
      strip_flag="yes"
      ;;
    --error-inject)
      error_inject_flag="yes"
      ;;
    --valgrind)
      valgrind="yes"
      ;;
    --fast)
      fast_flag="yes"
      ;;
    --with-flags)
      shift
      break
      ;;
    --with-help)
      with_usage
      exit 0
      ;;
    --sysadmin-help)
      sysadmin_usage
      exit 0
      ;;
    --developer-help)
      developer_usage
      exit 0
      ;;
    --extended-help)
      extended_usage
      exit 0
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option '$1'"
      exit 1
      ;;
    esac
    shift
  done
  for flag in $@
  do
    with_flags="$with_flags $flag"
  done
}

#
# We currently only support Linux, FreeBSD/OpenBSD, Mac OS X and Solaris
#
check_os()
{
  case "`uname -s`" in
    Linux)
      os="linux"
      ;;
    FreeBSD|OpenBSD)
      os="bsd"
      ;;
    Darwin)
      os="MacOSX"
      ;;
    SunOS)
      os="Solaris"
      ;;
    *)
      os="Unknown"
      ;;
  esac

}

set_cpu_base()
{
  if test "x$cpu_type" = "x" ; then
    if test "x$cpu_arg" = "x" ; then
      usage "CPU type not discovered, cannot proceed"
      exit 1
    fi
    case "$cpu_arg" in
      core2 | nocona | prescott | pentium* | i*86 )
        # Intel CPU
        cpu_base_type="x86"
        ;;
      athlon* | opteron* | k6 | k8 )
        # AMD CPU
        cpu_base_type="x86"
        ;;
      sparc )
        cpu_base_type="sparc"
        ;;
      itanium )
        cpu_base_type="itanium"
        ;;
      * )
        usage "CPU type $cpu_arg not handled by this script"
        exit 1
        ;;
    esac
  else
    cpu_base_type="$cpu_type"
    check_cpu_cflags=""
  fi
  if test "x$os" = "xMacOSX" ; then
    if test "x$m64" = "x" ; then
      m64="no"
    fi
  elif test "x$os" = "xSolaris" ; then
    if test "x$m64" = "x" ; then
      m64="yes"
    fi
  elif test "x$m64" = "x" ; then
    if test "x$cpu_arg" = "xnocona" || test "x$cpu_arg" = "xcore2" || \
       test "x$cpu_arg" = "xathlon64" || test "x$cpu_arg" = "xopteron" ; then
      m64="yes"
    else
      m64="no"
    fi
  fi
  echo "Discovered CPU of type $cpu_base_type ($cpu_arg) on $os"
  if test "x$m64" = "xyes" ; then
    echo "Will compile 64-bit binaries"
  else
    echo "Will compile 32-bit binaries"
  fi
  return 0
}

#
# Add to the variable commands with the configure command
# 
init_configure_commands()
{
  path=`dirname $0`
  cp $path/cmake_configure.sh $path/../configure
  chmod +x $path/../configure
  cflags="$c_warnings $base_cflags $compiler_flags"
  cxxflags="$cxx_warnings $base_cxxflags $compiler_flags"
  configure="./configure $base_configs $with_flags"

  env_flags="CC=\"$CC\" CFLAGS=\"$cflags\" CXX=\"$CXX\" CXXFLAGS=\"$cxxflags\""
  if test "x$LDFLAGS" != "x" ; then
    env_flags="$env_flags LDFLAGS=\"$LDFLAGS\""
  fi
  if test "x$ASFLAGS" != "x" ; then
    env_flags="$env_flags ASFLAGS=\"$ASFLAGS\""
  fi
  commands="$commands
    $env_flags $configure"
} 

#
# Initialise the variable commands with the commands needed to generate
# the configure script.
#
init_auto_commands()
{
  set_libtoolize_version
  commands="\
  $make -k maintainer-clean || true
  /bin/rm -rf */.deps/*.P configure config.cache
  /bin/rm -rf  storage/*/configure storage/*/config.cache autom4te.cache
  /bin/rm -rf storage/*/autom4te.cache;"
#
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
#
  commands="$commands
  aclocal || die \"Can't execute aclocal\"
  autoheader || die \"Can't execute autoheader\"
  $LIBTOOLIZE --automake --copy --force || die \"Can't execute libtoolize\"
  automake --add-missing --copy --force || die \"Can't execute automake\"
  autoconf || die \"Can't execute autoconf\""
}

#
# Add to the variable commands the make command and possibly also
# strip commands
#
add_make_commands()
{
  AM_MAKEFLAGS="-j $parallelism"
  commands="$commands
  $make $AM_MAKEFLAGS"

  if test "x$strip_flag" = "xyes" ; then
    commands="$commands
    mkdir -p tmp
    nm --numeric-sort sql/mysqld  > tmp/mysqld.sym
    objdump -d sql/mysqld > tmp/mysqld.S
    strip sql/mysqld
    strip storage/ndb/src/kernel/ndbd
    strip storage/ndb/src/mgmsrv/ndb_mgmd
    strip storage/ndb/src/mgmclient/ndb_mgm"
  fi
}

#
# Set make version, but only gmake is supported :)
#
set_make_version()
{
  if gmake --version > /dev/null 2>&1
  then
    make=gmake
  else
    make=make
  fi
  if test "x`$make --version | grep GNU`" = "x" ; then
    die "Only gmake is supported"
  fi
}

#
# Find a libtoolize binary, both libtoolize and glibtoolize are
# ok, use first found.
#
set_libtoolize_version()
{
  LIBTOOLIZE=not_found
  save_ifs="$IFS"; IFS=':'
  for dir in $PATH
  do
    if test -x $dir/libtoolize
    then
      LIBTOOLIZE=libtoolize
      echo "Found libtoolize in $dir"
      break
    fi
    if test -x $dir/glibtoolize
    then
      LIBTOOLIZE=glibtoolize
      echo "Found glibtoolize in $dir"
      break
    fi
  done
  IFS="$save_ifs"
  if test "x$LIBTOOLIZE" = "xnot_found" ; then
    die "Found no libtoolize version, quitting here"
  fi
  return
}

#
# If ccache (a compiler cache which reduces build time)
# (http://samba.org/ccache) is installed, use it.
# We use 'grep' and hope that 'grep' works as expected
# (returns 0 if finds lines)
# We do not use ccache when gcov is used. Also only when
# gcc is used.
#
set_ccache_usage()
{
  if test "x$compiler" = "xgcc" ; then
    if ccache -V > /dev/null 2>&1 && test "$USING_GCOV" != "1"
    then
      echo "$CC" | grep "ccache" > /dev/null || CC="ccache $CC"
      echo "$CXX" | grep "ccache" > /dev/null || CXX="ccache $CXX"
    fi
  fi
}

#
# Set flags for various build configurations.
# Used in -valgrind builds
#
set_valgrind_flags()
{
  if test "x$valgrind_flag" = "xyes" ; then
    loc_valgrind_flags="-UFORCE_INIT_OF_VARS -DHAVE_purify "
    loc_valgrind_flags="$loc_valgrind_flags -DMYSQL_SERVER_SUFFIX=-valgrind-max"
    compiler_flags="$compiler_flags $loc_valgrind_flags"
    with_flags="$with_flags --with-valgrind"
  fi
}

#
# Set up warnings; default is to use no warnings, but if warning_mode
# is used a lot of warning flags are set up. These flags are valid only
# for gcc, so for other compilers we ignore the warning_mode.
#
set_warning_flags()
{
  if test "x$developer_flag" = "xyes" && test "x$warning_mode" = "x" ; then
    warning_mode="normal"
  fi
  if test "x$compiler" = "xgcc" ; then
    if test "x$warning_mode" = "normal" || test "x$warning_mode" = "extra" ; then
# Both C and C++ warnings
      warnings="$warnings -Wimplicit -Wreturn-type -Wswitch -Wtrigraphs"
      warnings="$warnings -Wcomment -W"
      warnings="$warnings -Wchar-subscripts -Wformat -Wparentheses -Wsign-compare"
      warnings="$warnings -Wwrite-strings -Wunused-function -Wunused-label"
      warnings="$warnings -Wunused-value -Wunused-variable"

      if test "x$warning_mode" = "extra" ; then
        warnings="$warnings -Wshadow"
      fi
# C warnings
      c_warnings="$warnings -Wunused-parameter"
# C++ warnings
      cxx_warnings="$warnings -Woverloaded-virtual -Wsign-promo -Wreorder"
      cxx_warnings="$warnings -Wctor-dtor-privacy -Wnon-virtual-dtor"
      compiler_flags="$compiler_flags -Wuninitialized"
    elif test "x$warning_mode" = "xpedantic" ; then
      warnings="-W -Wall -ansi -pedantic -Wno-long-long -D_POSIX_SOURCE"
      c_warnings="$warnings"
      cxx_warnings="$warnings -std=c++98"
# Reset CPU flags (-mtune), they don't work in -pedantic mode
     check_cpu_cflags=""
    fi
  fi
}

#
# Used in -debug builds
#
set_with_debug_flags()
{
  if test "x$with_debug_flag" = "xyes" ; then
    if test "x$developer_flag" = "xyes" ; then
      loc_debug_flags="-DUNIV_MUST_NOT_INLINE -DEXTRA_DEBUG -DFORCE_INIT_OF_VARS "
      compiler_flags="$compiler_flags $loc_debug_flags"
    fi
    compiler_flags="$compiler_flags $extra_debug_flags"
  fi
}

#
# Flag for optimizing builds for developers.
#
set_no_omit_frame_pointer_for_developers()
{
  if test "x$fast_flag" != "xno" ; then
    if test "x$developer_flag" = "xyes" && test "x$compiler" = "xgcc" ; then
# Be as fast as we can be without losing our ability to backtrace.
      compiler_flags="$compiler_flags -fno-omit-frame-pointer"
    fi
  fi
}

#
# Add -g to all builds that requested debug information in build
#
set_debug_flag()
{
  if test "x$compile_debug_flag" = "xyes" ; then
    compiler_flags="$compiler_flags -g"
  fi
}

#
# We compile in SSL support if we can, this isn't possible if CXX
# and CC aren't the same and we're not using GCC.
# 
set_ssl()
{
  if test "x$compiler" = "xgcc" && \
     test "x$CC" = "x$CXX" ; then
    base_configs="$base_configs --with-ssl"
  fi
}

#
# Base options used by all packages
#
# SSL library to use. --with-ssl selects the bundled yaSSL
# implementation of SSL. To use openSSL, you must point out the location
# of the openSSL headers and libs on your system.
# For example: --with-ssl=/usr
#
set_base_configs()
{
  base_configs="$base_configs --prefix=$prefix"
  base_configs="$base_configs --libexecdir=$prefix/bin"
  base_configs="$base_configs --with-zlib-dir=bundled"
  if test "x$datadir" = "x" ; then
    base_configs="$base_configs --localstatedir=$prefix/data"
  else
    base_configs="$base_configs --localstatedir=$datadir"
  fi
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-debug"
  fi
  base_configs="$base_configs --enable-local-infile"
  base_configs="$base_configs --enable-thread-safe-client"
  base_configs="$base_configs --with-big-tables"
  base_configs="$base_configs --with-extra-charsets=all"
  if test "x$with_fast_mutexes" = "xyes" ; then
    base_configs="$base_configs --with-fast-mutexes"
  fi
  base_configs="$base_configs --with-pic"
  base_configs="$base_configs --with-csv-storage-engine"
  if test "x$with_perfschema" != "xno" ; then
    base_configs="$base_configs --with-perfschema"
  fi
}

#
# Add all standard engines and partitioning (included as part of MySQL
# Cluster storage engine as well) as part of MySQL Server. These are 
# added in all packages except the classic package.
#
set_base_engines()
{
  engine_configs="--with-archive-storage-engine"
  engine_configs="$engine_configs --with-blackhole-storage-engine"
  engine_configs="$engine_configs --without-example-storage-engine"
  engine_configs="$engine_configs --with-federated-storage-engine"
  engine_configs="$engine_configs --with-partition"
  base_configs="$base_configs $engine_configs"
}

set_innodb_engine()
{
  base_configs="$base_configs --with-innodb"
}

set_ndb_engine()
{
  base_configs="$base_configs --with-ndbcluster"
  base_configs="$base_configs --without-ndb-debug"
}

set_pro_package()
{
  if test "x$without_comment" != "xyes" ; then
    base_configs="$base_configs --with-comment=\"MySQL Enterprise Pro $version_text built from source\""
  fi
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-debug\""
  fi
}

set_cge_extended_package()
{
  if test "x$without_comment" != "xyes" ; then
    base_configs="$base_configs --with-comment=\"MySQL Cluster Carrier Grade Extended Edition $version_text built from source\""
  fi
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-cge-extended-debug\""
  else
    base_configs="$base_configs --with-server-suffix=\"-cge-extended\""
  fi
}

set_cge_package()
{
  if test "x$without_comment" != "xyes" ; then
    base_configs="$base_configs --with-comment=\"MySQL Cluster Carrier Grade Edition $version_text built from source\""
  fi
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-cge-debug\""
  else
    base_configs="$base_configs --with-server-suffix=\"-cge\""
  fi
}

set_classic_package()
{
  if test "x$without_comment" != "xyes" ; then
    base_configs="$base_configs --with-comment=\"MySQL Classic $version_text built from source\""
  fi
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-debug\""
  fi
  base_configs="$base_configs --without-example-storage-engine"
}

#
# Special handling of readline; use readline from the MySQL
# distribution if building a GPL version, otherwise use libedit.
#
set_readline_package()
{
  if test -d "$path/../cmd-line-utils/readline" && test "x$gpl" = "xyes" ; then
    base_configs="$base_configs --with-readline"
  elif test -d "$path/../cmd-line-utils/libedit" ; then
    base_configs="$base_configs --with-libedit"
  fi
}

#
# If fast flag set by user we also add architecture as discovered to 
# compiler flags to make binary optimised for architecture at hand. 
# We use this feature on gcc compilers.
#
set_gcc_special_options()
{
  if test "x$fast_flag" = "xyes" && test "x$compiler" = "xgcc" ; then
    compiler_flags="$compiler_flags $check_cpu_cflags"
  fi
}

#
# If we discover a Core 2 Duo architecture and we have enabled the fast
# flag, we enable a compile especially optimised for Core 2 Duo. This
# feature is currently available on Intel's icc compiler only.
#
set_icc_special_options()
{
  if test "x$fast_flag" = "xyes" && test "x$cpu_arg" = "xcore2" && \
     test "x$compiler" = "xicc" ; then
    compiler_flags="$compiler_flags -xT"
  fi
}

set_cc_and_cxx_for_gcc()
{
  if test "x$CC" = "x" ; then
    CC="gcc -static-libgcc -fno-exceptions"
  fi
  if test "x$CXX" = "x" ; then
    CXX="gcc -static-libgcc -fno-exceptions"
  fi
}

set_cc_and_cxx_for_icc()
{
  if test "x$CC" = "x" ; then
    CC="icc -static-intel -static-libgcc"
  fi
  if test "x$CXX" = "x" ; then
    CXX="icpc -static-intel -static-libgcc"
  fi
}

set_cc_and_cxx_for_open64()
{
  if test "x$CC" = "x" ; then
    CC="opencc -static-libgcc -fno-exceptions"
  fi
  if test "x$CXX" = "x" ; then
    CXX="openCC -static-libgcc -fno-exceptions"
  fi
}

set_cc_and_cxx_for_forte()
{
  if test "x$CC" = "x" ; then
    CC="cc"
  fi
  if test "x$CXX" = "x" ; then
    CXX="CC"
  fi
}

#
# FreeBSD Section
#
set_bsd_configs()
{
  if test "x$cpu_base_type" != "xx86" ; then
    usage "Only x86 CPUs supported for FreeBSD"
    exit 1
  fi
  if test "x$compiler" != "xgcc" ; then
    usage "Only gcc supported for FreeBSD"
    exit 1
  fi
  base_configs="$base_configs --enable-assembler"
  if test "x$fast_flag" != "xno" ; then
    compiler_flags="$compiler_flags -O3"
  else
    compiler_flags="$compiler_flags -O0"
  fi
  set_cc_and_cxx_for_gcc
}

check_64_bits()
{
  echo "Checking for 32/64-bits compilation"
  echo "int main() { return 0; }" > temp_test.c
  if test "x$m64" = "xyes" ; then
    cmd="$CC $compile_flags -m64 temp_test.c"
    if ! $cmd 2>1 ; then
      m64="no"
      echo "Changing to 32-bits since 64-bits didn't work"
    else
      echo "Will use 64-bits"
    fi
  else
    cmd="$CC $compile_flags -m32 temp_test.c"
    if ! $cmd 2>1 ; then
      m64="yes"
      echo "Changing to 64-bits since 32-bits didn't work"
    else
      echo "Will use 32-bits"
    fi
  fi
  rm temp_test.c
}

#
# Get GCC version
#
get_gcc_version()
{
  # check if compiler is gcc and dump its version
  cc_verno=`$cc -dumpversion 2>/dev/null`
  if test "x$?" = "x0" ; then
    set -- `echo $cc_verno | tr '.' ' '`
    cc_ver="GCC"
    cc_major=$1
    cc_minor=$2
    cc_patch=$3
    gcc_version=`expr $cc_major '*' 100 '+' $cc_minor`
  fi
}

#
# Link time optimizer (interprocedural optimizations) for Open64
#
check_for_open64_link_time_optimizer()
{
  if test "x$with_link_time_optimizer" = "xyes" ; then
    compiler_flags="$compiler_flags -ipa"
    LDFLAGS="$LDFLAGS -ipa"
  fi
}

#
# Link time optimizer (interprocedural optimizations) for icc
#
check_for_icc_link_time_optimizer()
{
  if test "x$with_link_time_optimizer" = "xyes" ; then
    compiler_flags="$compiler_flags -ipo"
    LDFLAGS="$LDFLAGS -ipo"
  fi
}

#
# Link time optimizer (interprocedural optimizations) for forte
#
check_for_forte_link_time_optimizer()
{
  if test "x$with_link_time_optimizer" = "xyes" ; then
    compiler_flags="$compiler_flags -ipo"
    LDFLAGS="$LDFLAGS -ipo"
  fi
}

#
# Link Time Optimizer in GCC (LTO) uses a parameter -flto
# which was added to GCC 4.5, if --with-link-time-optimizer
# is set then use this feature
#
check_for_gcc_link_time_optimizer()
{
  get_gcc_version
  if test "$gcc_version" -ge 405 && \
     test "x$with_link_time_optimizer" = "xyes" ; then
    compiler_flags="$compiler_flags -flto"
    LDFLAGS="$LDFLAGS -flto"
  fi
}

set_feedback_for_gcc()
{
  if test "x$GENERATE_FEEDBACK_PATH" != "x" ; then
    compiler_flags="$compiler_flags -fprofile-generate"
    compiler_flags="$compiler_flags -fprofile-dir=$GENERATE_FEEDBACK_PATH"
  elif test "x$USE_FEEDBACK_PATH" != "x" ; then
    compiler_flags="$compiler_flags -fprofile-use"
    compiler_flags="$compiler_flags -fprofile-correction"
    compiler_flags="$compiler_flags -fprofile-dir=$USE_FEEDBACK_PATH"
  fi
}

set_feedback_for_open64()
{
  if test "x$GENERATE_FEEDBACK_PATH" != "x" ; then
    compiler_flags="$compiler_flags --fb-create=$GENERATE_FEEDBACK_PATH/feedback"
  elif test "x$USE_FEEDBACK_PATH" != "x" ; then
    compiler_flags="$compiler_flags --fb-opt=$USE_FEEDBACK_PATH/feedback"
  fi
}

#
# Linux Section
#
set_linux_configs()
{
# Default to use --with-fast-mutexes on Linux
  if test "x$with_fast_mutexes" = "x" ; then
    base_configs="$base_configs --with-fast-mutexes"
  fi
  if test "x$cpu_base_type" != "xx86" && \
     test "x$cpu_base_type" != "xitanium" ; then
    usage "Only x86 and Itanium CPUs supported for Linux"
    exit 1
  fi
  if test "x$use_tcmalloc" = "xyes" ; then
    base_configs="$base_configs --with-mysqld-libs=-ltcmalloc_minimal"
  fi
  if test "x$cpu_base_type" = "xx86" ; then
    base_configs="$base_configs --enable-assembler"
  fi
  if test "x$compiler" = "xgcc" ; then
    set_cc_and_cxx_for_gcc
    if test "x$fast_flag" != "xno" ; then
      if test "x$fast_flag" = "xyes" ; then
        compiler_flags="$compiler_flags -O3"
        check_for_gcc_link_time_optimizer
      else
        compiler_flags="$compiler_flags -O3"
      fi
    else
      compiler_flags="$compiler_flags -O0"
    fi
    set_feedback_for_gcc
# configure will set proper compiler flags for gcc on Linux
  elif test "x$compiler" = "xicc" ; then
    compiler_flags="$compiler_flags -mp -restrict"
    set_cc_and_cxx_for_icc
    if test "x$cpu_base_type" = "xitanium" ; then
      compiler_flags="$compiler_flags -no-ftz"
    fi
    if test "x$fast_flag" != "xno" ; then
      compiler_flags="$compiler_flags -O3 -unroll2 -ip"
      if test "x$fast_flag" = "xyes" ; then
        check_for_icc_link_time_optimizer
      fi
    fi
  elif test "x$compiler" = "xopen64" ; then
    set_cc_and_cxx_for_open64
    if test "x$fast_flag" != "xno" ; then
      if test "x$fast_flag" = "xyes" ; then
        compiler_flags="$compiler_flags -O3"
#       Generate code specific for the machine you run on
        compiler_flags="$compiler_flags -march=auto"
        check_for_open64_link_time_optimizer
        if test "x$with_mso" = "xyes" ; then
          compiler_flags="$compiler_flags -mso"
        fi
      else
        compiler_flags="$compiler_flags -O3"
      fi
    fi
    set_feedback_for_open64
  else
    usage "Only gcc,icc and Open64 compilers supported for Linux"
    exit 1
  fi
  check_64_bits
  if test "x$m64" = "xyes" ; then
    compiler_flags="$compiler_flags -m64"
  else
    compiler_flags="$compiler_flags -m32"
  fi
}

#
# Solaris Section
#
set_solaris_configs()
{
# Use mtmalloc as malloc, see Tim Cook blog
# For information on optimal compiler settings, see article at
# http://developers.sun.com/solaris/articles/mysql_perf_tune.html
# by Luojia Chen at Sun.
  base_configs="$base_configs --with-named-curses=-lcurses"
  case "`uname -a`" in
    *5.8* | *5.9* )
      ;;

    *5.10* | *5.11*)
      base_configs="$base_configs --with-mysqld-libs=-lmtmalloc"
      ;;
    *)
      usage "Only versions 8,9, 10 and 11 supported for Solaris"
      exit 1
  esac
  if test "x$cpu_base_type" != "xx86" && \
     test "x$cpu_base_type" != "xsparc" ; then
    usage "Only x86 and Sparc CPUs supported for Solaris"
    exit 1
  fi
  if test "x$compiler" != "xgcc" && \
     test "x$compiler" != "xforte" ; then
    usage "Only gcc and Forte compilers supported for Solaris"
    exit 1
  fi
  if test "x$m64" = "xyes" ; then
    compiler_flags="$compiler_flags -m64"
    LDFLAGS="-m64"
    ASFLAGS="$ASFLAGS -m64"
  else
    compiler_flags="$compiler_flags -m32"
    LDFLAGS="-m32"
    ASFLAGS="$ASFLAGS -m32"
  fi
  if test "x$compiler" = "xgcc" ; then
    set_cc_and_cxx_for_gcc
    if test "x$cpu_base_type" != "xx86" ; then
      usage "gcc currently not supported for Solaris on SPARC"
      exit 1
    fi
    if test "x$fast_flag" = "xyes" ; then
      LDFLAGS="$LDFLAGS -O3"
      compiler_flags="$compiler_flags -O3"
      check_for_gcc_link_time_optimizer
    else
      if test "x$fast_flag" = "xgeneric" ; then
        LDFLAGS="$LDFLAGS -O2"
        compiler_flags="$compiler_flags -O2"
      else
        LDFLAGS="$LDFLAGS -O0"
        compiler_flags="$compiler_flags -O0"
      fi
    fi
  else
#Using Forte compiler (SunStudio)
    set_cc_and_cxx_for_forte
    compiler_flags="$compiler_flags -mt"
    LDFLAGS="$LDFLAGS -mt"
    compiler_flags="$compiler_flags -fsimple=1"
    compiler_flags="$compiler_flags -ftrap=%none"
    compiler_flags="$compiler_flags -xbuiltin=%all"
    compiler_flags="$compiler_flags -xlibmil"
    compiler_flags="$compiler_flags -xlibmopt"
    if test "x$fast_flag" = "xyes" ; then
      compiler_flags="$compiler_flags -xtarget=native"
      compiler_flags="$compiler_flags -xunroll=3"
      check_for_forte_link_time_optimizer
    else
      compiler_flags="$compiler_flags -xtarget=generic"
    fi
    if test "x$cpu_base_type" = "xx86" ; then
      compiler_flags="$compiler_flags -nofstore"
      base_cxx_flags="$base_cxx_flags -features=no%except"
      if test "x$fast_flag" = "xyes" ; then
        compiler_flags="$compiler_flags -xregs=frameptr"
        compiler_flags="$compiler_flags -xO4"
      else
        compiler_flags="$compiler_flags -xregs=no%frameptr"
        if test "x$fast_flag" = "xgeneric" ; then
          compiler_flags="$compiler_flags -xO2"
        else
          compiler_flags="$compiler_flags -xO0"
        fi
      fi
    else
#Using SPARC cpu with SunStudio (Forte) compiler
      ASFLAGS="$ASFLAGS -xarch=sparc"
      LDFLAGS="$LDFLAGS -xarch=sparc"
      base_cxxflags="$base_cxxflags -noex"
      base_cflags="$base_cflags -xstrconst"
      compiler_flags="$compiler_flags -xarch=sparc"
      if test "x$fast_flag" = "xyes" ; then
        compiler_flags="$compiler_flags -xbinopt=prepare"
        LDFLAGS="$LDFLAGS -xbinopt=prepare"
        compiler_flags="$compiler_flags -xO4"
      elif test "x$fast_flag" = "xgeneric" ; then
        compiler_flags="$compiler_flags -xO3"
      else
        compiler_flags="$compiler_flags -xO0"
      fi
    fi
  fi
}

#
# Mac OS X Section
#
set_macosx_configs()
{
  if test "x$cpu_base_type" != "xx86" || test "x$compiler" != "xgcc" ; then
    usage "Only gcc/x86 supported for Mac OS X"
    exit 1
  fi
#
# Optimize for space as long as it doesn't affect performance, use some
# optimisations also when not in fast mode.
#
  base_cxxflags="$base_cxxflags -felide-constructors"
  compiler_flags="$compiler_flags -fno-common"
  if test "x$m64" = "xyes" ; then
    compiler_flags="$compiler_flags -m64"
    compiler_flags="$compiler_flags -arch x86_64"
  else
    compiler_flags="$compiler_flags -m32"
    compiler_flags="$compiler_flags -arch i386"
  fi
  if test "x$fast_flag" != "xno" ; then
    compiler_flags="$compiler_flags -Os"
  else
    compiler_flags="$compiler_flags -O0"
  fi
  set_cc_and_cxx_for_gcc
}

#
# Use static linking for own modules and dynamic linking for system
# modules unless specifically requested to do everything statically.
# Should normally not be used; static_linking_flag kept in case someone 
# really needs it. Available only if developer flag is also set.
#
set_static_link_configs()
{
  if test "x$static_linking_flag" = "xyes" && test "x$developer_flag" = "xyes" ; then
    loc_static_link="--with-mysqld-ldflags=\"-all-static\""
    loc_static_link="$loc_static_link --with-client-ldflags=\"-all-static\""
  else
    loc_static_link="--with-mysqld-ldflags=\"-static\""
    loc_static_link="$loc_static_link --with-client-ldflags=\"-static\""
  fi
  base_configs="$base_configs $loc_static_link"
}

#
# Enable error injection in MySQL Server (for developer build only -
# extra check for developer flag required).
#
set_error_inject_configs()
{
  if test "x$error_inject_flag" = "xyes" && test "x$developer_flag" = "xyes" ; then
    base_configs="$base_configs --with-error-inject"
    if test "x$package" = "xndb" || test "x$package" = "xextended" ; then
      base_configs="$base_configs --with-ndb-ccflags='-DERROR_INSERT'"
    fi
  fi
}

set_default_package()
{
  if test "x$package" = "x" ; then
    if test "x$developer_flag" = "xyes" ; then
      package="extended"
    else
      package="cge"
    fi
  fi
}

set_defaults_based_on_environment()
{
  if test ! -z "$MYSQL_DEVELOPER" ; then
    developer_flag="yes"
  fi
  if test ! -z "$MYSQL_DEVELOPER_DEBUG" ; then
    with_debug_flag="yes"
    fast_flag="no"
  fi
  if test ! -z "$MYSQL_DEVELOPER_PACKAGE" ; then
    package="$MYSQL_DEVELOPER_PACKAGE"
    parse_package
  fi
}

########################################################################

if test ! -f sql/mysqld.cc ; then
  die "You must run this script from the MySQL top-level directory"
fi

cpu_type=
package=
prefix="/usr/local/mysql"
parallelism="8"
fast_flag="generic"
compiler="gcc"
gpl="yes"
version_text=
developer_flag="no"
just_configure=
warning_mode=
with_flags=
error_inject_flag=
with_debug_flag=
compile_debug_flag=
strip_flag=
valgrind_flag=
static_linking_flag=
compiler_flags=
os=
cpu_base_type=
warnings=
c_warnings=
cflags=
base_cflags=
cxx_warnings=
base_cxxflags=
base_configs=
debug_flags=
cxxflags=
extra_debug_flags=
m64=
explicit_size_set=
datadir=
commands=
engine_configs=
ASFLAGS=
LDFLAGS=
use_tcmalloc=
without_comment="yes"
with_fast_mutexes=
with_perfschema="yes"
with_link_time_optimizer=
with_mso=
gcc_version="0"
generate_feedback_path=
use_feedback_path=

set_defaults_based_on_environment

parse_options "$@"

set_default_package

set -e

#
# Check for the CPU and set up CPU specific flags. We may reset them
# later.
# This call sets the cpu_arg and check_cpu_args parameters
#
path=`dirname $0`
if test "x$compiler" = "xgcc" ; then
  compiler=
fi
. "$path/check-cpu"
if test "x$compiler" = "x" ; then
  compiler="gcc"
fi
check_os
set_cpu_base
if test "x$?" = "x1" ; then
  exit 1
fi

#
# Set up c_warnings and cxx_warnings; add to compiler_flags.
# Possibly reset check_cpu_flags.
#
set_warning_flags

#
# Add to compiler_flags.
#
set_valgrind_flags
set_with_debug_flags
set_no_omit_frame_pointer_for_developers
set_debug_flag
set_gcc_special_options
set_icc_special_options

#
# Definitions of various packages possible to compile. The default is to
# build a source variant of MySQL Cluster Carrier Grade Edition 
# including all storage engines except InnoDB, and to use GPL libraries.
#
set_base_configs
if test "x$gpl" = "xyes" ; then
  version_text="GPL version"
else
  version_text="Commercial version"
fi
if test "x$package" = "xpro" ; then
  set_base_engines
  set_innodb_engine
  set_pro_package
elif test "x$package" = "xextended" ; then
  set_base_engines
  set_ndb_engine
  set_innodb_engine
  set_cge_extended_package
elif test "x$package" = "xcge" ; then
  set_base_engines
  set_ndb_engine
  set_cge_package
elif test "x$package" = "xclassic" ; then
  set_classic_package
else
  die "No supported package was used, internal error"
fi
set_readline_package
set_static_link_configs
set_error_inject_configs

#
# This section handles flags for specific combinations of compilers,
# operating systems, and processors.
#

if test "x$os" = "xlinux" ; then
  set_linux_configs
elif test "x$os" = "xSolaris" ; then
  set_solaris_configs
elif test "x$os" = "xMacOSX" ; then
  set_macosx_configs
elif test "x$os" = "xbsd" ; then
  set_bsd_configs
else
  die "Operating system not supported by this script"
fi
set_ssl
#
# Final step before setting up commands is to set up proper make and
# proper libtoolize versions, and to determine whether to use ccache.
#
set_make_version
set_ccache_usage

#
# Set up commands variable from variables prepared for base 
# configurations, compiler flags, and warnings flags.
# 
init_configure_commands

if test "x$just_configure" != "xyes" ; then
  add_make_commands
fi

#
# The commands variable now contains the entire command to be run for
# the build; we either execute it, or merely print it out.
#
echo "Running command:"
echo "$commands"
if test "x$just_print" != "xyes" ; then
  eval "set -x; $commands"
fi
