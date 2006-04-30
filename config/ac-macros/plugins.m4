dnl ===========================================================================
dnl Support for plugable mysql server modules
dnl ===========================================================================
dnl
dnl WorkLog#3201
dnl
dnl Framework for pluggable static and dynamic modules for mysql
dnl
dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE
dnl
dnl Syntax:
dnl   MYSQL_MODULE([name],[Plugin module name],
dnl                [Plugin module description],
dnl                [group,group...])
dnl   
dnl What it does:
dnl   First declaration for a plugin module (mandatory).
dnl   Adds module as member to configuration groups (if specified)
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE],[
 _MYSQL_MODULE(
  [$1],
  [__MYSQL_MODULE_]AS_TR_CPP([$1])[__],
  m4_default([$2], [$1 plugin]),
  m4_default([$3], [plugin for $1]),
  m4_default([$4], []),
 )
])

AC_DEFUN([_MYSQL_MODULE],[
 m4_ifdef([$2], [
  AC_FATAL([Duplicate MYSQL_MODULE declaration for $3])
 ],[
  m4_define([$2], [$1])
  _MYSQL_PLUGAPPEND([__mysql_plugin_list__],[$1])
  m4_define([MYSQL_MODULE_NAME_]AS_TR_CPP([$1]), [$3])
  m4_define([MYSQL_MODULE_DESC_]AS_TR_CPP([$1]), [$4])
  ifelse([$5], [], [], [
   _MYSQL_PLUGAPPEND_OPTS([$1], $5)
  ])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_STORAGE_ENGINE
dnl
dnl What it does:
dnl   Short cut for storage engine declarations
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_STORAGE_ENGINE],[
 MYSQL_MODULE([$1], [$3], [$4], [[$5]])
 MYSQL_MODULE_DEFINE([$1], [WITH_]AS_TR_CPP([$1])[_STORAGE_ENGINE])
 ifelse([$2],[no],[],[
  _MYSQL_LEGACY_STORAGE_ENGINE(
      m4_bpatsubst(m4_default([$2], [$1-storage-engine]), -, _))
 ])
])

AC_DEFUN([_MYSQL_LEGACY_STORAGE_ENGINE],[
if test "[${with_]$1[+set}]" = set; then
  [with_module_]$1="[$with_]$1"
fi
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DEFINE
dnl
dnl What it does:
dnl   When a plugin module is to be statically linked, define the C macro
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DEFINE],[
 REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_MODULE_DEFINE_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DIRECTORY
dnl
dnl What it does:
dnl   Adds a directory to the build process
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DIRECTORY],[
 REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_MODULE_DIRECTORY_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_STATIC
dnl
dnl What it does:
dnl   Declare the name for the static library 
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_STATIC],[
 REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_MODULE_STATIC_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DYNAMIC
dnl
dnl What it does:
dnl   Declare the name for the shared library
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DYNAMIC],[
 REQUIRE_PLUGIN([$1])
 m4_define([MYSQL_MODULE_DYNAMIC_]AS_TR_CPP([$1]), [$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_MANDATORY
dnl
dnl What it does:
dnl   Marks the specified plugin as a mandatory module
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_MANDATORY],[
 REQUIRE_PLUGIN([$1])
 _MYSQL_MODULE_MANDATORY([$1],
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1])
 )
])

AC_DEFUN([_MYSQL_MODULE_MANDATORY],[
 m4_define([$2], [yes])
 m4_ifdef([$3], [
  AC_WARNING([syntax],[Mandatory plugin $1 has been disabled])
  m4_undefine([$2])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DISABLED
dnl
dnl What it does:
dnl   Marks the specified plugin as a disabled module
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DISABLED],[
 REQUIRE_PLUGIN([$1])
 _MYSQL_MODULE_DISABLED([$1], 
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1])
 )
])

AC_DEFUN([_MYSQL_MODULE_DISABLED],[
 m4_define([$2], [yes])
 m4_ifdef([$3], [
  AC_FATAL([attempt to disable mandatory plugin $1])
  m4_undefine([$2])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DEPENDS
dnl
dnl What it does:
dnl   Enables other modules neccessary for this module
dnl   Dependency checking is not recursive so if any 
dnl   required module requires further modules, list them
dnl   here too!
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DEPENDS],[
 REQUIRE_PLUGIN([$1])
 ifelse($#, 0, [], $#, 1, [
  AC_FATAL([bad number of arguments])
 ], $#, 2, [
  _MYSQL_MODULE_DEPEND([$1],[$2])
 ],[
  _MYSQL_MODULE_DEPEND([$1],[$2])
  MYSQL_MODULE_DEPENDS([$1], m4_shift(m4_shift($@)))
 ])
])

AC_DEFUN([_MYSQL_MODULE_DEPEND],[
 REQUIRE_PLUGIN([$2])
 _MYSQL_PLUGAPPEND([__mysql_plugdepends_$1__],[$2])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_ACTIONS
dnl
dnl What it does:
dnl   Declares additional actions required to configure the module
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_ACTIONS],[
 REQUIRE_PLUGIN([$1])
 m4_ifdef([$2],[
   m4_define([MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]),m4_defn([$2]))
 ],[
   m4_define([MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]), [$2])
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CONFIGURE_PLUGINS
dnl
dnl What it does:
dnl   Called last, emits all required shell code to configure the modules
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CONFIGURE_PLUGINS],[
 m4_ifdef([__mysql_plugin_configured__],[
   AC_FATAL([cannot call [MYSQL_CONFIGURE_PLUGINS] multiple times])
 ],[
   m4_define([__mysql_plugin_configured__],[done])
   m4_ifdef([__mysql_plugin_list__],[
    _MYSQL_CHECK_PLUGIN_ARGS([$1])
    _MYSQL_CONFIGURE_PLUGINS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    _MYSQL_DO_PLUGIN_ACTIONS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    AC_SUBST([mysql_se_dirs])
    AC_SUBST([mysql_pg_dirs])
   ])
 ])
])

AC_DEFUN([_MYSQL_CONFIGURE_PLUGINS],[
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_CHECK_PLUGIN([$1])
 ],[
  _MYSQL_CHECK_PLUGIN([$1])
  _MYSQL_CONFIGURE_PLUGINS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN],[
 _DO_MYSQL_CHECK_PLUGIN(
  [$1],
  m4_bpatsubst([$1], -, _),
  [MYSQL_MODULE_NAME_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DESC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DEFINE_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DIRECTORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_STATIC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DYNAMIC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1])
 )
])

AC_DEFUN([_DO_MYSQL_CHECK_PLUGIN],[
 m4_ifdef([$5],[
  AH_TEMPLATE($5, [Include ]$4[ into mysqld])
 ])
 AC_MSG_CHECKING([whether to use ]$3)
 mysql_use_plugin_dir=""
 m4_ifdef([$10],[
  if test "[$mysql_module_]$2" = yes -a \
          "[$with_module_]$2" != no -o \
          "[$with_module_]$2" = yes; then
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([disabled])
  fi
  AC_MSG_RESULT([no])
 ],[
  m4_ifdef([$9],[
   if test "[$with_module_]$2" = no; then
     AC_MSG_RESULT([error])
     AC_MSG_ERROR([cannot disable mandatory module])
   fi
   [mysql_module_]$2=yes
  ])
  if test "[$with_module_]$2" = no; then
    AC_MSG_RESULT([no])
  else
    if test "[$mysql_module_]$2" != yes -a \
            "[$with_module_]$2" != yes; then
      m4_ifdef([$8],[
       m4_ifdef([$6],[
         if test -d "$srcdir/$6" ; then
           mysql_use_plugin_dir="$6"
       ])
       AC_SUBST([plugin_]$2[_shared_target], "$8")
       AC_SUBST([plugin_]$2[_static_target], [""])
       [with_module_]$2=yes
       AC_MSG_RESULT([plugin])
       m4_ifdef([$6],[
         else
           [mysql_module_]$2=no
           AC_MSG_RESULT([no])
         fi
       ])
      ],[
       [with_module_]$2=no
       AC_MSG_RESULT([no])
      ])
    else
      m4_ifdef([$7],[
       ifelse(m4_bregexp($7, [^lib[^.]+\.a$]), -2, [
        m4_ifdef([$6],[
         mysql_use_plugin_dir="$6"
         mysql_plugin_libs="$mysql_plugin_libs -L[\$(top_builddir)]/$6"
        ])
        mysql_plugin_libs="$mysql_plugin_libs
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
      [with_module_]$2=yes
      AC_MSG_RESULT([yes])
    fi
    m4_ifdef([$6],[
      if test -n "$mysql_use_plugin_dir" ; then
        mysql_plugin_dirs="$mysql_plugin_dirs $6"
        if test -f "$srcdir/$6/configure" ; then
          other_configures="$other_configures $6/configure"
        else
          AC_CONFIG_FILES($6/Makefile)
        fi
        ifelse(m4_substr($6, 0, 8), [storage/],
          [mysql_se_dirs="$mysql_se_dirs ]m4_substr($6, 8)",
          m4_substr($6, 0, 7), [plugin/],
          [mysql_pg_dirs="$mysql_pg_dirs ]m4_substr($6, 7)",
          [AC_FATAL([don't know how to handle plugin dir ]$6)])
      fi
    ])
  fi
 ])
])

AC_DEFUN([_MYSQL_DO_PLUGIN_ACTIONS],[
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_PLUGIN_ACTIONS([$1])
 ],[
  _MYSQL_PLUGIN_ACTIONS([$1])
  _MYSQL_DO_PLUGIN_ACTIONS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_PLUGIN_ACTIONS],[
 _DO_MYSQL_PLUGIN_ACTIONS(
  [$1],
  m4_bpatsubst([$1], -, _),
  [MYSQL_MODULE_NAME_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DESC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DEFINE_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DIRECTORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_STATIC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DYNAMIC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1])
 )
])


AC_DEFUN([_DO_MYSQL_PLUGIN_ACTIONS],[
 m4_ifdef([$10], [], [
  if test "[$with_module_]$2" = yes; then
    if test -z "[$plugin_]$2[_static_target]" -a \
            -z "[$plugin_]$2[_shared_target]"; then
      AC_MSG_ERROR([that's strange, $1 failed sanity check])
    fi
    $11
  fi
 ])
])



dnl ===========================================================================
dnl  Private helper macros
dnl ===========================================================================


AC_DEFUN([REQUIRE_PLUGIN],[
 _REQUIRE_PLUGIN([$1], [__MYSQL_MODULE_]AS_TR_CPP([$1])[__])
])

define([_REQUIRE_PLUGIN],[
 ifdef([$2],[
  ifelse($2, [$1], [], [
   AC_FATAL([Misspelt MYSQL_MODULE declaration for $1])
  ])
 ],[
  AC_FATAL([Missing MYSQL_MODULE declaration for $1])
 ])
])


dnl ---------------------------------------------------------------------------


AC_DEFUN([_MYSQL_MODULE_META_CHECK], [ifelse($#, 0, [], $#, 1,
[_MYSQL_CHECK_PLUGIN_META([$1], [__mysql_]m4_bpatsubst($1, -, _)[_plugins__])
],
[_MYSQL_CHECK_PLUGIN_META([$1], [__mysql_]m4_bpatsubst($1, -, _)[_plugins__])
_MYSQL_MODULE_META_CHECK(m4_shift($@))])
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN_META], [
  [$1] )
m4_ifdef([$2], [
    mysql_modules='m4_bpatsubst($2, :, [ ])'
],[
    mysql_modules=''
])
    ;;
])


dnl ---------------------------------------------------------------------------


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

AC_DEFUN([_MYSQL_PLUGAPPEND_OPTS],[
 ifelse($#, 0, [], $#, 1, [
  AC_FATAL([bad number of args])
 ], $#, 2, [
  _MYSQL_PLUGAPPEND_OPTONE([$1],[$2])
 ],[
  _MYSQL_PLUGAPPEND_OPTONE([$1],[$2])
  _MYSQL_PLUGAPPEND_OPTS([$1], m4_shift(m4_shift($@)))
 ])
])

AC_DEFUN([_MYSQL_PLUGAPPEND_OPTONE],[
 ifelse([$2], [all], [
  AC_FATAL([protected plugin group: all])
 ],[
  ifelse([$2], [none], [
   AC_FATAL([protected plugin group: none])
  ],[
   _MYSQL_PLUGAPPEND([__mysql_$1_configs__],[$2])
   _MYSQL_PLUGAPPEND([__mysql_]m4_bpatsubst($2, -, _)[_plugins__],[$1], [
    _MYSQL_PLUGAPPEND([__mysql_metaplugin_list__],[$2])
   ])
  ])
 ])
])


dnl ---------------------------------------------------------------------------


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
  [MYSQL_MODULE_NAME_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DESC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DEFINE_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DIRECTORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_STATIC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DYNAMIC_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]),
  __mysql_[$1]_configs__,
 )
])

AC_DEFUN([_MYSQL_SHOW_PLUGIN],[dnl
  === $3 ===
  Module Name:      [$1]
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
[m4_ifdef([$1],[ifelse($1,[no],[],[static ]m4_ifdef([$2],[and dnl
]))])[]m4_ifdef([$2],[dynamic],[m4_ifdef([$1],[],[static])])])


dnl ---------------------------------------------------------------------------


AC_DEFUN([_MYSQL_MODULE_ARGS_CHECK],[
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_CHECK_PLUGIN_ARG([$1],
   [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
   [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]))
 ],[
  _MYSQL_CHECK_PLUGIN_ARG([$1],
   [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
   [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]))
  _MYSQL_MODULE_ARGS_CHECK(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN_ARG],[
 m4_ifdef([$3], [], [m4_define([$3],[ ])])
    [$1] )
 m4_ifdef([$2],[
      AC_MSG_ERROR([plugin $1 is disabled])
 ],[
      [mysql_module_]m4_bpatsubst([$1], -, _)=yes
 ])
      ;;
])

AC_DEFUN([_MYSQL_SANE_VARS], [
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_SANEVAR([$1])
 ],[
  _MYSQL_SANEVAR([$1])
  _MYSQL_SANE_VARS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_SANEVAR], [
   test -z "[$mysql_module_]m4_bpatsubst([$1], -, _)" &&
[mysql_module_]m4_bpatsubst([$1], -, _)='.'
   test -z "[$with_module_]m4_bpatsubst([$1], -, _)" &&
[with_module_]m4_bpatsubst([$1], -, _)='.'
])

AC_DEFUN([_MYSQL_CHECK_DEPENDENCIES], [
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_CHECK_DEPENDS([$1],[__mysql_plugdepends_$1__])
 ],[
  _MYSQL_CHECK_DEPENDS([$1],[__mysql_plugdepends_$1__])
  _MYSQL_CHECK_DEPENDENCIES(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_CHECK_DEPENDS], [
 m4_ifdef([$2], [
   if test "[$mysql_module_]m4_bpatsubst([$1], -, _)" = yes -a \
           "[$with_module_]m4_bpatsubst([$1], -, _)" != no -o \
           "[$with_module_]m4_bpatsubst([$1], -, _)" = yes; then
     _MYSQL_GEN_DEPENDS(m4_bpatsubst($2, :, [,]))
   fi
 ])
])

AC_DEFUN([_MYSQL_GEN_DEPENDS], [
 ifelse($#, 0, [], $#, 1, [
  _MYSQL_GEN_DEPEND([$1])
 ],[
  _MYSQL_GEN_DEPEND([$1])
  _MYSQL_GEN_DEPENDS(m4_shift($@))
 ])
])

AC_DEFUN([_MYSQL_GEN_DEPEND], [
 m4_ifdef([MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),[
      AC_MSG_ERROR([depends upon disabled module $1])
 ],[
      [mysql_module_]m4_bpatsubst([$1], -, _)=yes
      if test "[$with_module_]m4_bpatsubst([$1], -, _)" = no; then
        AC_MSG_ERROR([depends upon disabled module $1])
      fi
 ])
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN_ARGS],[
 AC_ARG_WITH([modules],
AS_HELP_STRING([--with-modules=PLUGIN[[[[[,PLUGIN..]]]]]],
 [Plugin modules to include in mysqld. (default is: $1) Must be a
 configuration name or a comma separated list of modules.])
AS_HELP_STRING([],[Available configurations are:] dnl
m4_bpatsubst([none:all]m4_ifdef([__mysql_metaplugin_list__],
__mysql_metaplugin_list__), :, [ ])[.])
AS_HELP_STRING([],[Available plugin modules are:] dnl
m4_bpatsubst(__mysql_plugin_list__, :, [ ])[.])
AS_HELP_STRING([--without-module-PLUGIN],
                [Disable the named module from being built. Otherwise, for
                modules which are not selected for inclusion in mysqld will be
                built dynamically (if supported)])
AS_HELP_STRING([--with-module-PLUGIN],
               [Forces the named module to be linked into mysqld statically.]),
 [mysql_modules="`echo $withval | tr ',.:;*[]' '       '`"],
 [mysql_modules=['$1']])

m4_divert_once([HELP_VAR_END],[
Description of plugin modules:
MYSQL_LIST_PLUGINS])

  case "$mysql_modules" in
  all )
    mysql_modules='m4_bpatsubst(__mysql_plugin_list__, :, [ ])'
    ;;
  none )
    mysql_modules=''
    ;;
m4_ifdef([__mysql_metaplugin_list__],[
_MYSQL_MODULE_META_CHECK(m4_bpatsubst(__mysql_metaplugin_list__, :, [,]))
])
  esac

  for plugin in $mysql_modules; do
    case "$plugin" in
    all | none )
      AC_MSG_ERROR([bad module name: $plugin])
      ;;
_MYSQL_MODULE_ARGS_CHECK(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    * )
      AC_MSG_ERROR([unknown plugin module: $plugin])
      ;;
    esac
  done

  _MYSQL_SANE_VARS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))  
  _MYSQL_CHECK_DEPENDENCIES(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
])

dnl ===========================================================================
