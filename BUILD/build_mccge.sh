#!/bin/sh

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
  Extended Edition, and the default build behaviour is to build with
  autotools. If you want to skip autotools and start from a source code
  release you can use the --no-autotools flag.

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
  based on a source code release you received from MySQL.

  It is assumed that you are building on a computer which is of the 
  same type as that on which you intend to run MySQL Cluster.

  The simplest possible way to run this script is to allow it to use the 
  built-in defaults everywhere, invoking it simply as:

  shell> ./build_mccge.sh

  This performs the following operations:
    1) Detects the operating system. Currently, Linux, FreeBSD, Solaris 
      10/11, and Mac OS X are supported by this script.
    2) Detect the type of CPU being used. Currently supported processors 
      are: x86 for all supported operating systems, Itanium for Linux 
      with GCC, and SPARC for Solaris using the Forte compiler.
    3) Invokes the GCC compiler.
    4) Builds a set of MySQL Cluster Carrier Grade Edition binaries; for
      more information about these, see --extended-help.
  
  The default version assumes that you have a source code tarball from 
  which you are building, and thus autoconf and automake do not need to 
  be run. If you have downloaded a BitKeeper tree then you should read 
  --developer-help.

  If you are building MySQL Cluster Carrier Grade Edition for commercial 
  use then you need to set the --commercial flag to ensure that the 
  commercial libraries are compiled in, rather than the GPL-only 
  libraries. The default is to build a GPL version of MySQL Cluster 
  Carrier Grade Edition.

  If your building on a Solaris SPARC machine you must set 
  --compiler=forte; if you want to build using the Intel compiler on 
  Linux, you need to set --compiler=icc.

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
  can also use this script to build MySQL Classic and MySQL Pro 
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
  --developer-help        Show help for developers trying to build MySQL
  --with-help             Show extended help on --with-xxx options to 
                          configure
  --extended-help         Show extended help message
  --without-debug         Build non-debug version
  --with-debug            Build debug version
  --with-debug=full       Build with full debug.
  --configure-only        Stop after running configure.
  --use-autotools         Start by running autoconf, automake,.. tools
  --no-autotools          Start from configure
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
  --compiler=[gcc|icc|forte]                  Select compiler
  --cpu=[x86|x86_64|sparc]                    Select CPU type
                          x86 => 32-bit binary
                          x86_64 => 64 bit binary unless Mac OS X
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
  --fast                  Optimise for CPU architecture buildt on
  --static-linking        Statically link system libraries into binaries
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
  Cluster Carrier Grade Edition to build the product from source on 
  these platforms/compilers: Linux/x86 (32-bit and 64-bit),
  Solaris 10 and 11/x86/gcc, Solaris 9/Sparc/Forte, and MacOSX/x86/gcc. 
  The script automatically detects CPU type and operating system; in 
  most cases this also determines which compiler to use, the exception 
  being Linux/x86 where you can choose between gcc and icc (gcc is the
  default).

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
      ARCHIVE, BLACKHOLE, CSV, EXAMPLE, FEDERATED, MYISAM, NDB
      (All storage engines except InnoDB)
    comment: MySQL Cluster Carrier Grade Edition GPL/Commercial version 
             built from source
    version string suffix: -cge

  --package=extended
    storage engines:
      ARCHIVE, BLACKHOLE, CSV, EXAMPLE, FEDERATED, MYISAM, INNODB, NDB
      (All storage engines)
    comment: MySQL Cluster Carrier Grade Extended Edition GPL/Commercial 
             version built from source
    version string suffix: -cge-extended

  --package=pro
    storage engines:
      ARCHIVE, BLACKHOLE, CSV, EXAMPLE, FEDERATED, INNODB, MYISAM
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
  partitioning.

  If --with-debug is used, an additional "-debug" is appended to the
  version string.

  --commercial
    This flag prevents the use of GPL libraries which cannot be used
    under a commercial license, such as the readline library.

  --with-debug[=full]
    This option will ensure that the version is built with debug
    information enabled; the optimisation level is decreased to -O.

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
    (cannot be overridden).

  --with-pic: Build all binaries using position independent assembler
    to avoid problems with dynamic linkers (cannot be overridden).

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

  --with-fast-mutexes
    Include an alternative implementation of mutexes that is faster on
    Linux systems

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

  Compiler options:
  -----------------

  This section describes the compiler options for each of the different
  platforms supported by thisscript.

  The --fast option adds -mtune=cpu_arg to the C/C++ flags (provides
  support for Nocona, K8, and other processors).

  Use of the --debug option adds -g to the C/C++ flags.

  FreeBSD/x86/gcc
  ---------------
    No flags are used. Instead, configure determines the proper flags to 
    use.

  Linux/x86+Itanium/gcc
  -------------
    No flags are used. Instead the configure script determines the
    proper flags to use for both normal and debug builds. Discovery of a
    Nocona or Core 2 Duo CPU causes a 64-bit binary to be built;
    otherwise, the binary is 32-bit. To build a 64-bit binary, -m64 is
    added to the C/C++ flags. (To build a 32-bit binary on a 64-bit CPU,
    use the --32 option as described previously.)

  Linux/x86+Itanium/icc
  -------------
    Flags used:
    CC  = icc -static-libgcc -static-libcxa -i-static
    C++ = icpc -static-libgcc -static-libcxa -i-static
    C/C++ flags = -mp -restrict

    On Itanium we also add -no-ftz and -no-prefetch to CC and C++ flags.

  The non-debug versions also add the following:
    C/C++ flags += -O3 unroll2 -ip

  The fast version adds:
    C/C++ flags += -ipo

  On discovery of a Core 2 Duo architecture while using icc, -xT is also 
  added to the C/C++ flags; this provides optimisations specific to Core 
  2 Duo. This is added only when the --fast flag is set.

  Solaris/x86/gcc
  ---------------
    All builds on Solaris are 64-bit, so -m64 is always used in the
    C/C++ flags. LDFLAGS is set to -m64 -static-libgcc -O/-O2.

  Solaris/Sparc/Forte
  -------------------
    Uses cc-5.0 as CC
    Sets ASFLAGS=LDFLAGS=xarch=v9, so that we compile Sparc v9 binaries
    C flags   = -Xa -strconst -xc99=none
    C++ flags = -noex
    C/C++ flags = -mt -D_FORTEC -xarch=v9

    For non-debug builds, the following flags are also used:

    C/C++ flags = -xO3

  MacOSX/x86/gcc
  --------------
  C/C++ flags include -fno-common -arch i386.

  Non-debug versions also add -Os -felide-constructors, where "-Os"
  means the build is space-optimised as long as the space optimisations
  do not negatively affect performance. Debug versions use -O.
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
      package=""
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
      m32="yes"
      ;;
    x86_64 )
      cpu_type="x86"
      m64="yes"
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
    forte )
      compiler="forte"
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
    --with-debug=full)
      full_debug="=full"
      with_debug_flag="yes"
      fast_flag="no"
      ;;
    --without-debug)
      with_debug_flag="no"
      if test "x$fast_flag" != "xyes" ; then
        fast_flag="generic"
      fi
      ;;
    --with-debug)
      with_debug_flag="yes"
      fast_flag="no"
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
      if test "x$m64" != "x" ; then
        echo "Cannot set both --32 and --64"
        exit 1
      fi
      m32="yes"
      ;;
    --64)
      if test "x$m32" != "x" ; then
        echo "Cannot set both --32 and --64"
        exit 1
      fi
      m64="yes"
      ;;
    --package=*)
      package=`get_key_value "$1"`
      parse_package
      ;;
    --parallelism=*)
      parallelism=`get_key_value "$1"`
      ;;
    --use-autotools)
      use_autotools="yes"
      ;;
    --no-autotools)
      use_autotools="no"
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
      return 1
    fi
    case "$cpu_arg" in
      core2 | nocona | prescott | pentium* | i*86 )
        cpu_base_type="x86"
        ;;
      athlon* | opteron* )
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
    m64="no"
  elif test "x$os" = "xSolaris" ; then
    m64="yes"
  elif test "x$m32" = "x" ; then
    if test "x$cpu_arg" = "xnocona" || test "x$cpu_arg" = "xcore2" || \
       test "x$cpu_arg" = "xathlon64" || test "x$cpu_arg" = "xopteron" ; then
      m64="yes"
    elif test "x$m64" != "xyes" ; then
      m64="no"
    fi
  else
    m64="no"
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
  cflags="$c_warnings $base_cflags $compiler_flags"
  cxxflags="$cxx_warnings $base_cxxflags $compiler_flags"
  configure="./configure $base_configs $with_flags"

  commands="$commands
    CC=\"$CC\" CFLAGS=\"$cflags\" CXX=\"$CXX\" CXXFLAGS=\"$cxxflags\""
  if test "x$LDFLAGS" != "x" ; then
    commands="$commands
      LDFLAGS=\"$LDFLAGS\""
  fi
  if test "x$ASFLAGS" != "x" ; then
    commands="$commands
      ASFLAGS=\"$ASFLAGS\""
  fi
  commands="$commands
    $configure"
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
set_up_ccache()
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
    loc_valgrind_flags="-USAFEMALLOC -UFORCE_INIT_OF_VARS -DHAVE_purify "
    loc_valgrind_flags="$loc_valgrind_flags -DMYSQL_SERVER_SUFFIX=-valgrind-max"
    compiler_flags="$compiler_flags $loc_valgrind_flags"
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
# Added unless --with-debug=full
      if test "x$full_debug" = "x" ; then
        compiler_flags="$compiler_flags -Wuninitialized"
      fi
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
      loc_debug_flags="$loc_debug_cflags -DSAFEMALLOC -DPEDANTIC_SAFEMALLOC"
      compiler_flags="$compiler_flags $loc_debug_flags"
    fi
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
  if test "x$compile_debug_flags" = "xyes" ; then
    compiler_flags="$compiler_flags -g"
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
    base_configs="$base_configs --with-debug$full_debug"
  fi
  base_configs="$base_configs --enable-local-infile"
  base_configs="$base_configs --enable-thread-safe-client"
  base_configs="$base_configs --with-big-tables"
  base_configs="$base_configs --with-extra-charsets=all"
  base_configs="$base_configs --with-ssl"
  base_configs="$base_configs --with-pic"
  base_configs="$base_configs --with-csv-storage-engine"
}

#
# Add all standard engines and partitioning (included as part of MySQL
# Cluster storage engine as well) as part of MySQL Server. These are 
# added in all packages except the classic package.
#
set_base_engines()
{
  engine_configs="$engine_configs --with-archive-storage-engine"
  engine_configs="$engine_configs --with-blackhole-storage-engine"
  engine_configs="$engine_configs --with-example-storage-engine"
  engine_configs="$engine_configs --with-federated-storage-engine"
  engine_configs="$engine_configs --with-partition"
}

set_pro_package()
{
  base_configs="$base_configs $engine_configs"
  base_configs="$base_configs --with-innodb"
  base_configs="$base_configs --with-comment=\"MySQL Pro $version_text built from source\""
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-debug\""
  fi
}

set_cge_extended_package()
{
  if test "x$gpl" = "xno" ; then
    echo "Cannot build Extended Carrier Grade Edition as Commercial version"
  fi
  base_configs="$base_configs --with-ndbcluster"
  base_configs="$base_configs --without-ndb-debug"
  base_configs="$base_configs $engine_configs"
  base_configs="$base_configs --with-innodb"
  base_configs="$base_configs --with-comment=\"MySQL Cluster Carrier Grade Extended Edition $version_text built from source\""
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-cge-extended-debug\""
  else
    base_configs="$base_configs --with-server-suffix=\"-cge-extended"\"
  fi
}

set_cge_package()
{
  base_configs="$base_configs --with-ndbcluster"
  base_configs="$base_configs --without-ndb-debug"
  base_configs="$base_configs $engine_configs"
  base_configs="$base_configs --with-comment=\"MySQL Cluster Carrier Grade Edition $version_text built from source\""
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-cge-debug\""
  else
    base_configs="$base_configs --with-server-suffix=\"-cge"\"
  fi
}

set_classic_package()
{
  base_configs="$base_configs --with-comment=\"MySQL Classic $version_text built from source\""
  if test "x$with_debug_flag" = "xyes" ; then
    base_configs="$base_configs --with-server-suffix=\"-debug\""
  fi
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
  CC="gcc"
  CXX="gcc"
}

#
# Linux Section
#
set_linux_configs()
{
  if test "x$cpu_base_type" != "xx86" && \
     test "x$cpu_base_type" != "xitanium" ; then
    usage "Only x86 and Itanium CPUs supported for 32-bit Linux"
    exit 1
  fi
  base_configs="$base_configs --with-fast-mutexes"
  if test "x$cpu_base_type" = "xx86" ; then
    base_configs="$base_configs --enable-assembler"
  fi
  if test "x$compiler" = "xgcc" ; then
    CC="gcc"
    CXX="gcc"
    if test "x$m64" = "xyes" ; then
      compiler_flags="$compiler_flags -m64"
    fi
# configure will set proper compiler flags for gcc on Linux
  elif test "x$compiler" = "xicc" ; then
    compiler_flags="$compiler_flags -mp -restrict"
    CC="icc -static-intel"
    CXX="icpc -static-intel"
    if test "x$cpu_base_type" = "xitanium" ; then
      compiler_flags="$compiler_flags -no-ftz"
    fi
    if test "x$fast_flag" != "xno" ; then
      compiler_flags="$compiler_flags -O3 -unroll2 -ip"
      if test "x$fast_flag" = "xyes" ; then
        compiler_flags="$compiler_flags -ipo"
      fi
    fi
  else
    usage "Only gcc and icc compilers supported for Linux"
    exit 1
  fi
}

#
# Solaris Section
#
set_solaris_configs()
{
  base_configs="$base_configs --with-mysqld-libs=-lmtmalloc"
  case "`uname -a`" in
    *5.10*|*5.11*)
      ;;
    *)
      die "Only versions 10 and 11 supported for Solaris"
  esac
  if test "x$cpu_base_type" != "xx86" && \
     test "x$cpu_base_type" != "xsparc" ; then
    usage "Only x86 and Sparc CPUs supported for Solaris"
    exit 1
  fi
  if test "x$compiler" = "xgcc" ; then
    CC="gcc"
    CXX="gcc"
    if test "x$cpu_base_type" != "xx86" ; then
      usage "Only gcc supported for Solaris 10/11 on SPARC"
    fi
    compiler_flags="$compiler_flags -m64 -DMY_ATOMIC_MODE_RWLOCKS"
    LDFLAGS="-m64 -static-libgcc"
    if test "x$fast_flag" != "xno" ; then
      LDFLAGS="$LDFLAGS -O2"
      compiler_flags="$compiler_flags -O2"
    else
      LDFLAGS="$LDFLAGS -O"
      compiler_flags="$compiler_flags -O"
    fi
  elif test "x$compiler" = "xforte" ; then
    if test "x$cpu_base_type" = "xx86" ; then
      usage "Only gcc supported for Solaris/x86"
    fi
    if test "x$cpu_base_type" != "xsparc" ; then
      usage "Forte compiler supported for Solaris 9/SPARC only"
    fi
    CC="cc-5.0"
    CXX=CC
    ASFLAGS="xarch=v9"
    LDFLAGS="xarch=v9"
    base_cflags="$base_cflags -Xa -xstrconst -xc99=none"
    base_cxxflags="$base_cxxflags -noex"
    compiler_flags="$compiler_flags -mt -D_FORTEC -xarch=v9"
    if test "x$fast_flag" != "xno" ; then
      compiler_flags="$compiler_flags -xO3"
    fi
  else
    usage "Only gcc and Forte compilers supported for Solaris"
    exit 1
  fi
}

#
# Mac OS X Section
#
set_macosx_configs()
{
  base_cxxflags="$base_cxxflags -fno-common"
  if test "x$cpu_base_type" = "xx86" && test "x$compiler" = "xgcc" ; then
    compiler_flags="$compiler_flags -arch i386"
  else
    usage "Only gcc/x86 supported for Mac OS X"
    exit 1
  fi
#
# Optimize for space as long as it doesn't affect performance, use some
# optimisations also when not in fast mode.
#
  if test "x$fast_flag" != "xno" ; then
    compiler_flags="$compiler_flags -Os"
    base_cxxflags="$base_cxxflags -felide-constructors"
  else
    compiler_flags="$compiler_flags -O"
  fi
  CC="gcc"
  CXX="gcc"
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

set_autotool_flags()
{
  if test "x$use_autotools" = "x" ; then
    if test "x$developer_flag" = "xno" ; then
      use_autotools="no"
    else
      use_autotools="yes"
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
parallelism="4"
fast_flag="generic"
compiler="gcc"
gpl="yes"
version_text=
developer_flag="no"
just_configure=
full_debug=
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
m32=
m64=
datadir=
commands=
use_autotools=

set_defaults_based_on_environment

parse_options "$@"

set_autotool_flags
set_default_package

set -e

#
# Check for the CPU and set up CPU specific flags. We may reset them
# later.
# This call sets the cpu_arg and check_cpu_args parameters
#
path=`dirname $0`
. "$path/check-cpu"
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
set_base_engines
if test "x$gpl" = "xyes" ; then
  version_text="GPL version"
else
  version_text="Commercial version"
fi
if test "x$package" = "xpro" ; then
  set_pro_package
elif test "x$package" = "xextended" ; then
  set_cge_extended_package
elif test "x$package" = "xcge" ; then
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

check_os
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

#
# Final step before setting up commands is to set up proper make and
# proper libtoolize versions, and to determine whether to use ccache.
#
set_make_version
set_up_ccache

#
# Set up commands variable from variables prepared for base 
# configurations, compiler flags, and warnings flags.
# 
if test "x$use_autotools" = "xyes" ; then
  init_auto_commands
fi
init_configure_commands

if test "x$just_configure" != "xyes" ; then
  add_make_commands
fi

#
# The commands variable now contains the entire command to be run for
# the build; we either execute it, or merely print it out.
#
if test "x$just_print" = "xyes" ; then
  echo "$commands"
else
  eval "set -x; $commands"
fi
