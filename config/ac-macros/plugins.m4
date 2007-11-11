dnl ===========================================================================
dnl Support for mysql server plugins
dnl ===========================================================================
dnl
dnl WorkLog#3201
dnl
dnl Framework for pluggable static and dynamic plugins for mysql
dnl
dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN([name],[Plugin name],
dnl                [Plugin description],
dnl                [group,group...])
dnl   
dnl DESCRIPTION
dnl   First declaration for a plugin (mandatory).
dnl   Adds plugin as member to configuration groups (if specified)
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN],[
 _MYSQL_PLUGIN(
  [$1],
  [__MYSQL_PLUGIN_]AS_TR_CPP([$1])[__],
  m4_default([$2], [$1 plugin]),
  m4_default([$3], [plugin for $1]),
  m4_default([$4], []),
 )
])

AC_DEFUN([_MYSQL_PLUGIN],[
 m4_ifdef([$2], [
  AC_FATAL([Duplicate MYSQL_PLUGIN declaration for $3])
 ],[
  m4_define([$2], [$1])
  _MYSQL_PLUGAPPEND([__mysql_plugin_list__],[$1])
  m4_define([MYSQL_PLUGIN_NAME_]AS_TR_CPP([$1]), [$3])
  m4_define([MYSQL_PLUGIN_DESC_]AS_TR_CPP([$1]), [$4])
  _MYSQL_PLUGAPPEND_META([$1], $5)
  ifelse(m4_bregexp(__mysql_include__,[/plug\.in$]),-1,[],[
     MYSQL_PLUGIN_DIRECTORY([$1],
         m4_bregexp(__mysql_include__,[^\(.*\)/plug\.in$],[\1]))
  ])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_STORAGE_ENGINE
dnl
dnl SYNOPSIS
dnl   MYSQL_STORAGE_ENGINE([name],[legacy-option],[Storage engine name],
dnl                        [Storage engine description],[group,group...])
dnl
dnl DESCRIPTION
dnl   Short cut for storage engine declarations
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_STORAGE_ENGINE],[
 MYSQL_PLUGIN([$1], [$3], [$4], [[$5]])
 MYSQL_PLUGIN_DEFINE([$1], [WITH_]AS_TR_CPP([$1])[_STORAGE_ENGINE])
 ifelse([$2],[no],[],[
  _MYSQL_LEGACY_STORAGE_ENGINE(
      m4_bpatsubst([$1], -, _),
      m4_bpatsubst(m4_default([$2], [$1-storage-engine]), -, _))
 ])
])

AC_DEFUN([_MYSQL_LEGACY_STORAGE_ENGINE],[
if test "[${with_]$2[+set}]" = set; then
  [with_plugin_]$1="[$with_]$2"
fi
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_DEFINE
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_DEFINE([name],[MYSQL_CPP_DEFINE])
dnl
dnl DESCRIPTION
dnl   When a plugin is to be statically linked, define the C macro
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_DEFINE],[
 MYSQL_REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_PLUGIN_DEFINE_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_DIRECTORY
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_DIRECTORY([name],[plugin/dir])
dnl
dnl DESCRIPTION
dnl   Adds a directory to the build process
dnl   if it contains 'configure' it will be picked up automatically
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_DIRECTORY],[
 MYSQL_REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_PLUGIN_DIRECTORY_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_STATIC
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_STATIC([name],[libmyplugin.a])
dnl
dnl DESCRIPTION
dnl   Declare the name for the static library 
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_STATIC],[
 MYSQL_REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_PLUGIN_STATIC_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_DYNAMIC
dnl
dnl SYNOPSIS
dnl  MYSQL_PLUGIN_DYNAMIC([name],[myplugin.la])
dnl
dnl DESCRIPTION
dnl   Declare the name for the shared library
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_DYNAMIC],[
 MYSQL_REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_PLUGIN_DYNAMIC_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_MANDATORY
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_MANDATORY([name])
dnl
dnl DESCRIPTION
dnl   Marks the specified plugin as a mandatory plugin
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_MANDATORY],[
 MYSQL_REQUIRE_PLUGIN([$1])
 _MYSQL_PLUGIN_MANDATORY([$1],
  [MYSQL_PLUGIN_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1])
 )
])

AC_DEFUN([_MYSQL_PLUGIN_MANDATORY],[
 m4_define([$2], [yes])
 m4_ifdef([$3], [
  AC_FATAL([mandatory plugin $1 has been disabled])
  m4_undefine([$2])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_DISABLED
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_DISABLED([name])
dnl
dnl DESCRIPTION
dnl   Marks the specified plugin as a disabled plugin
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_DISABLED],[
 MYSQL_REQUIRE_PLUGIN([$1])
 _MYSQL_PLUGIN_DISABLED([$1], 
  [MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_MANDATORY_]AS_TR_CPP([$1])
 )
])

AC_DEFUN([_MYSQL_PLUGIN_DISABLED],[
 m4_define([$2], [yes])
 m4_ifdef([$3], [
  AC_FATAL([attempt to disable mandatory plugin $1])
  m4_undefine([$2])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_DEPENDS
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_DEPENDS([name],[prereq,prereq...])
dnl
dnl DESCRIPTION
dnl   Enables other plugins neccessary for the named plugin
dnl   Dependency checking is not recursive so if any 
dnl   required plugin requires further plugins, list them
dnl   here too!
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_DEPENDS],[
 MYSQL_REQUIRE_PLUGIN([$1])
 ifelse($#, 2, [
  _MYSQL_PLUGIN_DEPEND([$1], $2)
 ], [
  AC_FATAL([bad number of arguments])
 ])
])

AC_DEFUN([_MYSQL_PLUGIN_DEPEND],[
 ifelse($#, 1, [], [$#:$2], [2:], [], [
  MYSQL_REQUIRE_PLUGIN([$2])
  _MYSQL_PLUGAPPEND([__mysql_plugdepends_$1__],[$2])
  _MYSQL_PLUGIN_DEPEND([$1], m4_shift(m4_shift($@)))
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_ACTIONS
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_ACTIONS([name],[PLUGIN_CONFIGURE_STUFF])
dnl
dnl DESCRIPTION
dnl   Declares additional autoconf actions required to configure the plugin
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_ACTIONS],[
 MYSQL_REQUIRE_PLUGIN([$1])
 m4_ifdef([$2],[
   m4_define([MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1]),m4_defn([$2]))
 ],[
   m4_define([MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1]), [$2])
 ])
])

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_PLUGIN_DEPENDS_ON_MYSQL_INTERNALS
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_DEPENDS_ON_MYSQL_INTERNALS([name],[file name])
dnl
dnl DESCRIPTION
dnl   Some modules in plugins keep dependance on structures
dnl   declared in sql/ (THD class usually)
dnl   That has to be fixed in the future, but until then
dnl   we have to recompile these modules when we want to
dnl   to compile server parts with the different #defines
dnl   Normally it happens when we compile the embedded server
dnl   Thus one should mark such files in his handler using this macro
dnl    (currently only one such a file per plugin is supported)
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_PLUGIN_DEPENDS_ON_MYSQL_INTERNALS],[
 MYSQL_REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_PLUGIN_DEPENDS_ON_MYSQL_INTERNALS_]AS_TR_CPP([$1]), [$2])
])

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CONFIGURE_PLUGINS
dnl
dnl SYNOPSIS
dnl   MYSQL_PLUGIN_DEPENDS([name,name...])
dnl
dnl DESCRIPTION
dnl   Used last, emits all required shell code to configure the plugins
dnl   Argument is a list of default plugins or meta-plugin
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CONFIGURE_PLUGINS],[
 m4_ifdef([__mysql_plugin_configured__],[
   AC_FATAL([cannot use [MYSQL_CONFIGURE_PLUGINS] multiple times])
 ],[
   m4_define([__mysql_plugin_configured__],[done])
   _MYSQL_INCLUDE_LIST(
   m4_bpatsubst(m4_esyscmd([ls plugin/*/plug.in storage/*/plug.in 2>/dev/null]),
[[ 
]],[,]))
   m4_ifdef([__mysql_plugin_list__],[
    _MYSQL_CHECK_PLUGIN_ARGS([$1])
    _MYSQL_CONFIGURE_PLUGINS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    _MYSQL_EMIT_PLUGIN_ACTIONS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    AC_SUBST([mysql_se_dirs])
    AC_SUBST([mysql_pg_dirs])
    AC_SUBST([mysql_se_unittest_dirs])
    AC_SUBST([mysql_pg_unittest_dirs])
    AC_SUBST([condition_dependent_plugin_modules])
    AC_SUBST([condition_dependent_plugin_objects])
    AC_SUBST([condition_dependent_plugin_links])
    AC_SUBST([condition_dependent_plugin_includes])
   ])
 ])
])

AC_DEFUN([_MYSQL_CONFIGURE_PLUGINS],[
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_EMIT_CHECK_PLUGIN([$1])
 ],[
  _MYSQL_EMIT_CHECK_PLUGIN([$1])
  _MYSQL_CONFIGURE_PLUGINS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_EMIT_CHECK_PLUGIN],[
 __MYSQL_EMIT_CHECK_PLUGIN(
  [$1],
  m4_bpatsubst([$1], -, _),
  [MYSQL_PLUGIN_NAME_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DESC_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DEFINE_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DIRECTORY_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_STATIC_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DYNAMIC_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DEPENDS_ON_MYSQL_INTERNALS_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1])
 )
])

AC_DEFUN([__MYSQL_EMIT_CHECK_PLUGIN],[
 m4_ifdef([$5],[
  AH_TEMPLATE($5, [Include ]$4[ into mysqld])
 ])
 AC_MSG_CHECKING([whether to use ]$3)
 mysql_use_plugin_dir=""
 m4_ifdef([$10],[
  if test "X[$mysql_plugin_]$2" = Xyes -a \
          "X[$with_plugin_]$2" != Xno -o \
          "X[$with_plugin_]$2" = Xyes; then
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([disabled])
  fi
  AC_MSG_RESULT([no])
 ],[
  m4_ifdef([$9],[
   if test "X[$with_plugin_]$2" = Xno; then
     AC_MSG_RESULT([error])
     AC_MSG_ERROR([cannot disable mandatory plugin])
   fi
   [mysql_plugin_]$2=yes
  ],[
   case "$with_mysqld_ldflags " in
     *"-all-static "*)
       # No need to build shared plugins when mysqld is linked with
       # -all-static as it won't be able to load them.
       if test "X[$mysql_plugin_]$2" != Xyes -a \
               "X[$with_plugin_]$2" != Xyes; then
	     [with_plugin_]$2=no
	   fi
     ;;
   esac
  ])
  if test "X[$with_plugin_]$2" = Xno; then
    AC_MSG_RESULT([no])
  else
    m4_ifdef([$8],m4_ifdef([$7],[],[[with_plugin_]$2='']))
    if test "X[$mysql_plugin_]$2" != Xyes -a \
            "X[$with_plugin_]$2" != Xyes; then
      m4_ifdef([$8],[
       m4_ifdef([$6],[
         if test -d "$srcdir/$6" ; then
           mysql_use_plugin_dir="$6"
       ])
       AC_SUBST([plugin_]$2[_shared_target], "$8")
       AC_SUBST([plugin_]$2[_static_target], [""])
       [with_plugin_]$2=yes
       AC_MSG_RESULT([plugin])
       m4_ifdef([$6],[
         else
           [mysql_plugin_]$2=no
           AC_MSG_RESULT([no])
         fi
       ])
      ],[
       [with_plugin_]$2=no
       AC_MSG_RESULT([no])
      ])
    else
      m4_ifdef([$7],[
       ifelse(m4_bregexp($7, [^lib[^.]+\.a$]), -2, [
dnl change above "-2" to "0" to enable this section
dnl Although this is "pretty", it breaks libmysqld build
        m4_ifdef([$6],[
         mysql_use_plugin_dir="$6"
         mysql_plugin_libs="$mysql_plugin_libs -L[\$(top_builddir)]/$6"
        ])
        mysql_plugin_libs="$mysql_plugin_libs dnl
[-l]m4_bregexp($7, [^lib\([^.]+\)], [\1])"
       ], m4_bregexp($7, [^\\\$]), 0, [
        m4_ifdef([$6],[
         mysql_use_plugin_dir="$6"
        ])
        mysql_plugin_libs="$mysql_plugin_libs $7"
       ], [
        m4_ifdef([$6],[
         mysql_use_plugin_dir="$6"
         mysql_plugin_libs="$mysql_plugin_libs \$(top_builddir)/$6/$7"
        ],[
         mysql_plugin_libs="$mysql_plugin_libs $7"
        ])
       ])
       m4_ifdef([$5],[
        AC_DEFINE($5)
       ])
       AC_SUBST([plugin_]$2[_static_target], "$7")
       AC_SUBST([plugin_]$2[_shared_target], [""])
      ],[
       m4_ifdef([$6],[
        AC_MSG_RESULT([error])
        AC_MSG_ERROR([Plugin $1 does not support static linking])
       ],[
        m4_ifdef([$5],[
         AC_DEFINE($5)
         AC_SUBST([plugin_]$2[_static_target], ["yes"])
         AC_SUBST([plugin_]$2[_shared_target], [""])
        ])
       ])
      ])
      mysql_plugin_defs="$mysql_plugin_defs, [builtin_]$2[_plugin]"
      [with_plugin_]$2=yes
      AC_MSG_RESULT([yes])
      m4_ifdef([$11],[
       condition_dependent_plugin_modules="$condition_dependent_plugin_modules m4_bregexp($11, [[^/]+$], [\&])"
       condition_dependent_plugin_objects="$condition_dependent_plugin_objects m4_bregexp($11, [[^/]+\.], [\&o])"
       condition_dependent_plugin_links="$condition_dependent_plugin_links $6/$11"
       condition_dependent_plugin_includes="$condition_dependent_plugin_includes -I[\$(top_srcdir)]/$6/m4_bregexp($11, [^.+[/$]], [\&])"
      ])
    fi
    m4_ifdef([$6],[
      if test -n "$mysql_use_plugin_dir" ; then
        mysql_plugin_dirs="$mysql_plugin_dirs $6"
        m4_syscmd(test -f "$6/configure")
        ifelse(m4_sysval, 0,
          [AC_CONFIG_SUBDIRS($6)],
          [AC_CONFIG_FILES($6/Makefile)]
        )
        ifelse(m4_substr($6, 0, 8), [storage/],
          [
            [mysql_se_dirs="$mysql_se_dirs ]m4_substr($6, 8)"
             mysql_se_unittest_dirs="$mysql_se_unittest_dirs ../$6"
          ],
          m4_substr($6, 0, 7), [plugin/],
          [
            [mysql_pg_dirs="$mysql_pg_dirs ]m4_substr($6, 7)"
             mysql_pg_unittest_dirs="$mysql_pg_unittest_dirs ../$6"
          ],
          [AC_FATAL([don't know how to handle plugin dir ]$6)])
      fi
    ])
  fi
 ])
])

AC_DEFUN([_MYSQL_EMIT_PLUGIN_ACTIONS],[
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_EMIT_PLUGIN_ACTION([$1])
 ],[
  _MYSQL_EMIT_PLUGIN_ACTION([$1])
  _MYSQL_EMIT_PLUGIN_ACTIONS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_EMIT_PLUGIN_ACTION],[
 __MYSQL_EMIT_PLUGIN_ACTION(
  [$1],
  m4_bpatsubst([$1], -, _),
  [MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1])
 )
])


AC_DEFUN([__MYSQL_EMIT_PLUGIN_ACTION],[
 m4_ifdef([$3], [], [
  if test "X[$with_plugin_]$2" = Xyes; then
    if test "X[$plugin_]$2[_static_target]" = X -a \
            "X[$plugin_]$2[_shared_target]" = X; then
      AC_MSG_ERROR([that's strange, $1 failed sanity check])
    fi
    $4
  fi
 ])
])



dnl ===========================================================================
dnl  Private helper macros
dnl ===========================================================================


dnl SYNOPSIS
dnl   MYSQL_REQUIRE_PLUGIN([name])
dnl
dnl DESCRIPTION
dnl   Checks that the specified plugin does exist

AC_DEFUN([MYSQL_REQUIRE_PLUGIN],[
 _MYSQL_REQUIRE_PLUGIN([$1], [__MYSQL_PLUGIN_]AS_TR_CPP([$1])[__])
])

define([_MYSQL_REQUIRE_PLUGIN],[
 ifdef([$2],[
  ifelse($2, [$1], [], [
   AC_FATAL([Misspelt MYSQL_PLUGIN declaration for $1])
  ])
 ],[
  AC_FATAL([Missing MYSQL_PLUGIN declaration for $1])
 ])
])


dnl ---------------------------------------------------------------------------


dnl SYNOPSIS
dnl   _MYSQL_EMIT_METAPLUGINS([name,name...])
dnl
dnl DESCRIPTION
dnl   Emits shell code for metaplugins

AC_DEFUN([_MYSQL_EMIT_METAPLUGINS], [ifelse($#, 0, [], $#, 1,
[_MYSQL_EMIT_METAPLUGIN([$1], [__mysql_]m4_bpatsubst($1, -, _)[_plugins__])
],
[_MYSQL_EMIT_METAPLUGIN([$1], [__mysql_]m4_bpatsubst($1, -, _)[_plugins__])
_MYSQL_EMIT_METAPLUGINS(m4_shift($@))])
])

AC_DEFUN([_MYSQL_EMIT_METAPLUGIN], [
  [$1] )
m4_ifdef([$2], [
    mysql_plugins='m4_bpatsubst($2, :, [ ])'
],[
    mysql_plugins=''
])
    ;;
])


dnl ---------------------------------------------------------------------------


dnl SYNOPSIS
dnl   _MYSQL_PLUGAPPEND([name],[to-append])
dnl
dnl DESCRIPTION
dnl   Helper macro for appending to colon-delimited lists
dnl   Optinal 3rd argument is for actions only required when defining
dnl   macro named for the first time.

AC_DEFUN([_MYSQL_PLUGAPPEND],[
 m4_ifdef([$1],[
  m4_define([__plugin_append_tmp__], m4_defn([$1]))
  m4_undefine([$1])
  m4_define([$1], __plugin_append_tmp__[:$2])
  m4_undefine([__plugin_append_tmp__])
 ],[
  m4_define([$1], [$2])
  $3
 ])
])


dnl SYNOPSIS
dnl   _MYSQL_PLUGAPPEND_META([name],[meta,meta...])
dnl
dnl DESCRIPTION
dnl   Helper macro for adding plugins to meta plugins

AC_DEFUN([_MYSQL_PLUGAPPEND_META],[
 ifelse($#, 1, [], [$#:$2], [2:], [], [$2], [all], [
  AC_FATAL([protected plugin group: all])
 ], [$2], [none], [
  AC_FATAL([protected plugin group: none])
 ],[
  _MYSQL_PLUGAPPEND([__mysql_$1_configs__],[$2])
  _MYSQL_PLUGAPPEND([__mysql_]m4_bpatsubst($2, -, _)[_plugins__],[$1], [
   _MYSQL_PLUGAPPEND([__mysql_metaplugin_list__],[$2])
  ])
  _MYSQL_PLUGAPPEND_META([$1], m4_shift(m4_shift($@)))
 ])
])


dnl ---------------------------------------------------------------------------


dnl SYNOPSIS
dnl   MYSQL_LIST_PLUGINS
dnl
dnl DESCRIPTION
dnl   Emits formatted list of declared plugins

AC_DEFUN([MYSQL_LIST_PLUGINS],[dnl
 m4_ifdef([__mysql_plugin_list__],[dnl
  _MYSQL_LIST_PLUGINS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))dnl
 ])dnl
])

AC_DEFUN([_MYSQL_LIST_PLUGINS],[dnl
 ifelse($#, 0, [], $#, 1, [dnl
  MYSQL_SHOW_PLUGIN([$1])dnl
 ],[dnl
  MYSQL_SHOW_PLUGIN([$1])dnl
  _MYSQL_LIST_PLUGINS(m4_shift($@))dnl
 ])dnl
])

AC_DEFUN([MYSQL_SHOW_PLUGIN],[
 _MYSQL_SHOW_PLUGIN(
  [$1],
  [$1-plugin],
  [MYSQL_PLUGIN_NAME_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DESC_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DEFINE_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DIRECTORY_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_STATIC_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DYNAMIC_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1]),
  __mysql_[$1]_configs__,
 )
])

AC_DEFUN([_MYSQL_SHOW_PLUGIN],[dnl
  === $3 ===
  Plugin Name:      [$1]
  Description:      $4
  Supports build:   _PLUGIN_BUILD_TYPE([$7],[$8])[]dnl
m4_ifdef([$12],[
  Configurations:   m4_bpatsubst($12, :, [, ])])[]dnl
m4_ifdef([$10],[
  Status:           disabled])[]dnl
m4_ifdef([$9],[
  Status:           mandatory])[]dnl
])

AC_DEFUN([_PLUGIN_BUILD_TYPE],
[m4_ifdef([$1],[static ]m4_ifdef([$2],[and dnl
]))[]m4_ifdef([$2],[dynamic],[m4_ifdef([$1],[],[static])])])


dnl ---------------------------------------------------------------------------


AC_DEFUN([_MYSQL_EMIT_PLUGINS],[
 ifelse($#, 0, [], [$#:$1], [1:], [], [
  m4_ifdef([MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1]), [], [
   m4_define([MYSQL_PLUGIN_ACTIONS_]AS_TR_CPP([$1]),[ ])
  ])
    [$1] )
  m4_ifdef([MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1]),[
      AC_MSG_ERROR([plugin $1 is disabled])
  ],[
    _MYSQL_EMIT_PLUGIN_ENABLE([$1], m4_bpatsubst([$1], -, _),
      [MYSQL_PLUGIN_NAME_]AS_TR_CPP([$1]),
      [MYSQL_PLUGIN_STATIC_]AS_TR_CPP([$1]),
      [MYSQL_PLUGIN_DYNAMIC_]AS_TR_CPP([$1]))
  ])
      ;;
  _MYSQL_EMIT_PLUGINS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_EMIT_PLUGIN_ENABLE],[
    m4_ifdef([$5],m4_ifdef([$4],[
      [mysql_plugin_]$2=yes
    ],[
      AC_MSG_WARN([$3 can only be built as a plugin])
    ]),[
      [mysql_plugin_]$2=yes
    ])      
])

AC_DEFUN([_MYSQL_EMIT_PLUGIN_DEPENDS], [
 ifelse($#, 0, [], [$#:$1], [1:], [], [
  _MYSQL_EMIT_CHECK_DEPENDS(m4_bpatsubst([$1], -, _), 
                            [__mysql_plugdepends_$1__])
  _MYSQL_EMIT_PLUGIN_DEPENDS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_EMIT_CHECK_DEPENDS], [
 m4_ifdef([$2], [
   if test "X[$mysql_plugin_]$1" = Xyes -a \
           "X[$with_plugin_]$1" != Xno -o \
           "X[$with_plugin_]$1" = Xyes; then
     _MYSQL_EMIT_PLUGIN_DEPENDENCIES(m4_bpatsubst($2, :, [,]))
   fi
 ])
])

AC_DEFUN([_MYSQL_EMIT_PLUGIN_DEPENDENCIES], [
 ifelse([$1], [], [], [
  m4_ifdef([MYSQL_PLUGIN_DISABLED_]AS_TR_CPP([$1]),[
       AC_MSG_ERROR([depends upon disabled plugin $1])
  ],[
       [mysql_plugin_]m4_bpatsubst([$1], -, _)=yes
       if test "X[$with_plugin_]m4_bpatsubst([$1], -, _)" = Xno; then
         AC_MSG_ERROR([depends upon disabled plugin $1])
       fi
  ])
  _MYSQL_EMIT_PLUGIN_DEPENDENCIES(m4_shift($@))
 ])
])

dnl SYNOPSIS
dnl   _MYSQL_CHECK_PLUGIN_ARGS([plugin],[plugin]...)
dnl
dnl DESCRIPTION
dnl   Emits shell script for checking configure arguments
dnl   Arguments to this macro is default value for selected plugins

AC_DEFUN([_MYSQL_CHECK_PLUGIN_ARGS],[
 __MYSQL_CHECK_PLUGIN_ARGS(m4_default([$1], [none]))
])

AC_DEFUN([__MYSQL_CHECK_PLUGIN_ARGS],[
 AC_ARG_WITH([plugins],
AS_HELP_STRING([--with-plugins=PLUGIN[[[[[,PLUGIN..]]]]]],
               [Plugins to include in mysqld. (default is: $1) Must be a
                configuration name or a comma separated list of plugins.])
AS_HELP_STRING([],
               [Available configurations are:] dnl
m4_bpatsubst([none:]m4_ifdef([__mysql_metaplugin_list__],
             __mysql_metaplugin_list__:)[all], :, [ ])[.])
AS_HELP_STRING([],
               [Available plugins are:] dnl
m4_bpatsubst(__mysql_plugin_list__, :, [ ])[.])
AS_HELP_STRING([--without-plugin-PLUGIN],
               [Disable the named plugin from being built. Otherwise, for
                plugins which are not selected for inclusion in mysqld will be
                built dynamically (if supported)])
AS_HELP_STRING([--with-plugin-PLUGIN],
               [Forces the named plugin to be linked into mysqld statically.]),
 [mysql_plugins="`echo $withval | tr ',.:;*[]' '       '`"],
 [mysql_plugins=['$1']])

m4_divert_once([HELP_VAR_END],[
Description of plugins:
MYSQL_LIST_PLUGINS])

  case "$mysql_plugins" in
  all )
    mysql_plugins='m4_bpatsubst(__mysql_plugin_list__, :, [ ])'
    ;;
  none )
    mysql_plugins=''
    ;;
m4_ifdef([__mysql_metaplugin_list__],[
_MYSQL_EMIT_METAPLUGINS(m4_bpatsubst(__mysql_metaplugin_list__, :, [,]))
])
  esac

  for plugin in $mysql_plugins; do
    case "$plugin" in
    all | none )
      AC_MSG_ERROR([bad plugin name: $plugin])
      ;;
_MYSQL_EMIT_PLUGINS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    * )
      AC_MSG_ERROR([unknown plugin: $plugin])
      ;;
    esac
  done

  _MYSQL_EMIT_PLUGIN_DEPENDS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
])

dnl ---------------------------------------------------------------------------
dnl Macro: _MYSQL_INCLUDE_LIST
dnl
dnl SYNOPSIS
dnl   _MYSQL_INCLUDE_LIST([filename,filename...])
dnl
dnl DESCRIPTION
dnl   includes all files from the list
dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([_MYSQL_INCLUDE_LIST],[
 ifelse([$1], [], [], [
  m4_define([__mysql_include__],[$1])
  dnl We have to use builtin(), because sinclude would generate an error
  dnl "file $1 does not exists" in aclocal-1.8 - which is a bug, clearly
  dnl violating m4 specs, and which is fixed in aclocal-1.9
  builtin([include],$1)
  m4_undefine([__mysql_include__])
  _MYSQL_INCLUDE_LIST(m4_shift($@))
 ])
])

dnl ===========================================================================
