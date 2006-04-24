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

AC_DEFUN([MYSQL_MODULE],[ dnl
 _MYSQL_MODULE(
  [$1],
  [__MYSQL_MODULE_]AS_TR_CPP([$1])[__],
  m4_default([$2], [$1 plugin]),
  m4_default([$3], [plugin for $1]),
  m4_default([$4], []),
 ) dnl
])

AC_DEFUN([_MYSQL_MODULE],[ dnl
 m4_ifdef([$2], [ dnl
  AC_FATAL([[Duplicate MYSQL_MODULE declaration for ]][$3]) dnl
 ],[ dnl 
  m4_define([$2], [$1]) dnl
  _MYSQL_PLUGAPPEND([__mysql_plugin_list__],[$1]) dnl
  m4_define([MYSQL_MODULE_NAME_]AS_TR_CPP([$1]), [$3]) dnl
  m4_define([MYSQL_MODULE_DESC_]AS_TR_CPP([$1]), [$4]) dnl
  ifelse([$5], [], [], [ dnl
   _MYSQL_PLUGAPPEND_OPTS([$1], $5) dnl
  ]) dnl
 ]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_STORAGE_ENGINE
dnl
dnl What it does:
dnl   Short cut for storage engine declarations
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_STORAGE_ENGINE],[ dnl
 MYSQL_MODULE([$1], [$3], [$4], [[$5]]) dnl
 MYSQL_MODULE_DEFINE([$1], [WITH_]AS_TR_CPP([$1])[_STORAGE_ENGINE]) dnl
 ifelse([$2],[no],[],[ dnl
  _MYSQL_LEGACY_STORAGE_ENGINE([$1],m4_default([$2], [$1-storage-engine])) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_LEGACY_STORAGE_ENGINE],[
if test "[${with_]m4_bpatsubst($2, -, _)[+set}]" = set; then
  [with_module_]m4_bpatsubst($1, -, _)="[$with_]m4_bpatsubst($2, -, _)"
fi dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DEFINE
dnl
dnl What it does:
dnl   When a plugin module is to be statically linked, define the C macro
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DEFINE],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 m4_define([MYSQL_MODULE_DEFINE_]AS_TR_CPP([$1]), [$2]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DIRECTORY
dnl
dnl What it does:
dnl   Adds a directory to the build process
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DIRECTORY],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 m4_define([MYSQL_MODULE_DIRECTORY_]AS_TR_CPP([$1]), [$2]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_STATIC
dnl
dnl What it does:
dnl   Declare the name for the static library 
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_STATIC],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 m4_define([MYSQL_MODULE_STATIC_]AS_TR_CPP([$1]), [$2]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DYNAMIC
dnl
dnl What it does:
dnl   Declare the name for the shared library
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DYNAMIC],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 m4_define([MYSQL_MODULE_DYNAMIC_]AS_TR_CPP([$1]), [$2]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_MANDATORY
dnl
dnl What it does:
dnl   Marks the specified plugin as a mandatory module
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_MANDATORY],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 _MYSQL_MODULE_MANDATORY([$1],
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1])
 ) dnl
])

AC_DEFUN([_MYSQL_MODULE_MANDATORY],[ dnl
 m4_define([$2], [yes]) dnl
 m4_ifdef([$3], [ dnl
  AC_WARNING([syntax],[Mandatory plugin $1 has been disabled]) dnl
  m4_undefine([$2]) dnl
 ]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_DISABLED
dnl
dnl What it does:
dnl   Marks the specified plugin as a disabled module
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_DISABLED],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 _MYSQL_MODULE_DISABLED([$1], 
  [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
  [MYSQL_MODULE_MANDATORY_]AS_TR_CPP([$1])
 ) dnl
])

AC_DEFUN([_MYSQL_MODULE_DISABLED],[ dnl
 m4_define([$2], [yes]) dnl
 m4_ifdef([$3], [ dnl
  AC_FATAL([attempt to disable mandatory plugin $1]) dnl
  m4_undefine([$2]) dnl
 ]) dnl
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

AC_DEFUN([MYSQL_MODULE_DEPENDS],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  AC_FATAL([[bad number of arguments]]) dnl
 ], $#, 2, [ dnl
  _MYSQL_MODULE_DEPEND([$1],[$2]) dnl
 ],[ dnl
  _MYSQL_MODULE_DEPEND([$1],[$2]) dnl
  MYSQL_MODULE_DEPENDS([$1], m4_shift(m4_shift($@))) dnl
 ])
])

AC_DEFUN([_MYSQL_MODULE_DEPEND],[ dnl
 REQUIRE_PLUGIN([$2]) dnl
 _MYSQL_PLUGAPPEND([__mysql_plugdepends_$1__],[$2]) dnl
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_MODULE_ACTIONS
dnl
dnl What it does:
dnl   Declares additional actions required to configure the module
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_MODULE_ACTIONS],[ dnl
 REQUIRE_PLUGIN([$1]) dnl
 m4_ifdef([$2],[ dnl
   m4_define([MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]),m4_defn([$2])) dnl
 ],[ dnl
   m4_define([MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1]), [$2]) dnl
 ])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CONFIGURE_PLUGINS
dnl
dnl What it does:
dnl   Called last, emits all required shell code to configure the modules
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CONFIGURE_PLUGINS],[ dnl
 m4_ifdef([__mysql_plugin_configured__],[ dnl
   AC_FATAL([cannot call [MYSQL_CONFIGURE_PLUGINS] multiple times]) dnl
 ],[ dnl
   m4_define([__mysql_plugin_configured__],[done]) dnl
   m4_ifdef([__mysql_plugin_list__],[ dnl
    _MYSQL_CHECK_PLUGIN_ARGS([$1])
    _MYSQL_CONFIGURE_PLUGINS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    _MYSQL_DO_PLUGIN_ACTIONS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    _MYSQL_POST_PLUGIN_FIXUP()
   ]) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_CONFIGURE_PLUGINS],[ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  _MYSQL_CHECK_PLUGIN([$1]) dnl
 ],[ dnl
  _MYSQL_CHECK_PLUGIN([$1]) dnl
  _MYSQL_CONFIGURE_PLUGINS(m4_shift($@)) dnl
 ])
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN],[ dnl
 _DO_MYSQL_CHECK_PLUGIN(
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
  [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1])
 ) dnl
])

AC_DEFUN([_DO_MYSQL_CHECK_PLUGIN],[ dnl
 m4_ifdef([$5],[ dnl
  AH_TEMPLATE($5, [Include ]$4[ into mysqld])
 ])
 AC_MSG_CHECKING([whether to use ]$3) dnl
 m4_ifdef([$10],[
  if test "[$mysql_module_]m4_bpatsubst([$1], -, _)" = yes -a \
          "[$with_module_]m4_bpatsubst([$1], -, _)" != no -o \
          "[$with_module_]m4_bpatsubst([$1], -, _)" = yes; then
    AC_MSG_ERROR([disabled])
  fi
  AC_MSG_RESULT([no]) dnl
 ],[ dnl
  m4_ifdef([$9],[
   if test "[$with_module_]m4_bpatsubst([$1], -, _)" = no; then
     AC_MSG_ERROR([cannot disable mandatory module])
   fi
   [mysql_module_]m4_bpatsubst([$1], -, _)=yes dnl
  ])
  if test "[$with_module_]m4_bpatsubst([$1], -, _)" != no; then
    if test "[$mysql_module_]m4_bpatsubst([$1], -, _)" != yes -a \
            "[$with_module_]m4_bpatsubst([$1], -, _)" != yes; then dnl
      m4_ifdef([$8],[ dnl
       m4_ifdef([$6],[
        mysql_plugin_dirs="$mysql_plugin_dirs $6" dnl
       ])
       AC_SUBST([plugin_]m4_bpatsubst([$1], -, _)[_shared_target], "$8")
       AC_SUBST([plugin_]m4_bpatsubst([$1], -, _)[_static_target], [""])
       [with_module_]m4_bpatsubst([$1], -, _)=yes
       AC_MSG_RESULT([plugin]) dnl
      ],[
       [with_module_]m4_bpatsubst([$1], -, _)=no
       AC_MSG_RESULT([no]) dnl
      ])
    else dnl
      m4_ifdef([$7],[
       ifelse(m4_bregexp($7, [^lib[^.]+\.a$]), -2, [ dnl
        m4_ifdef([$6],[
         mysql_plugin_dirs="$mysql_plugin_dirs $6"
         mysql_plugin_libs="$mysql_plugin_libs -L[\$(top_builddir)]/$6" dnl
        ])
        mysql_plugin_libs="$mysql_plugin_libs dnl
[-l]m4_bregexp($7, [^lib\([^.]+\)], [\1])" dnl
       ], m4_bregexp($7, [^\\\$]), 0, [ dnl
        m4_ifdef([$6],[
         mysql_plugin_dirs="$mysql_plugin_dirs $6" dnl
        ])
        mysql_plugin_libs="$mysql_plugin_libs $7" dnl
       ], [ dnl
        m4_ifdef([$6],[
         mysql_plugin_dirs="$mysql_plugin_dirs $6"
         mysql_plugin_libs="$mysql_plugin_libs \$(top_builddir)/$6/$7" dnl
        ],[
         mysql_plugin_libs="$mysql_plugin_libs $7" dnl
        ]) dnl
       ]) dnl
       m4_ifdef([$5],[
        AC_DEFINE($5) dnl
       ])
       AC_SUBST([plugin_]m4_bpatsubst([$1], -, _)[_static_target], "$7")
       AC_SUBST([plugin_]m4_bpatsubst([$1], -, _)[_shared_target], [""]) dnl
      ],[ dnl
       m4_ifdef([$6],[
        AC_FATAL([plugin directory specified without library for ]$3) dnl
       ],[ dnl
        m4_ifdef([$5],[
         AC_DEFINE($5)
         AC_SUBST([plugin_]m4_bpatsubst([$1], -, _)[_static_target], ["yes"])
         AC_SUBST([plugin_]m4_bpatsubst([$1], -, _)[_shared_target], [""]) dnl
        ]) dnl
       ]) dnl
      ])
      mysql_plugin_defs="$mysql_plugin_defs, [builtin_]m4_bpatsubst([$2], -, _)"
      [with_module_]m4_bpatsubst([$1], -, _)=yes
      AC_MSG_RESULT([yes])
    fi
  else
    AC_MSG_RESULT([no])
  fi dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_DO_PLUGIN_ACTIONS],[ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  _MYSQL_PLUGIN_ACTIONS([$1]) dnl
 ],[ dnl
  _MYSQL_PLUGIN_ACTIONS([$1]) dnl
  _MYSQL_DO_PLUGIN_ACTIONS(m4_shift($@)) dnl
 ])
])

AC_DEFUN([_MYSQL_PLUGIN_ACTIONS],[ dnl
 _DO_MYSQL_PLUGIN_ACTIONS(
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
  [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1])
 ) dnl
])


AC_DEFUN([_DO_MYSQL_PLUGIN_ACTIONS],[ dnl
 m4_ifdef([$10], [], [
  if test "[$with_module_]m4_bpatsubst([$1], -, _)" = yes; then
    if test -z "[$plugin_]m4_bpatsubst([$1], -, _)[_static_target]" -a \
            -z "[$plugin_]m4_bpatsubst([$1], -, _)[_shared_target]"; then
      AC_MSG_ERROR([thats strange, $1 failed sanity check])
    fi
    $11
  fi dnl
 ]) dnl
])



dnl ===========================================================================
dnl  Private helper macros
dnl ===========================================================================


AC_DEFUN([REQUIRE_PLUGIN],[ dnl
 _REQUIRE_PLUGIN([$1], [__MYSQL_MODULE_]AS_TR_CPP([$1])[__]) dnl
])

define([_REQUIRE_PLUGIN],[ dnl
 ifdef([$2],[ dnl
  ifelse($2, [$1], [], [ dnl
   AC_FATAL([[Misspelt MYSQL_MODULE declaration for ]][$1]) dnl
  ]) dnl
 ],[ dnl
  AC_FATAL([[Missing MYSQL_MODULE declaration for ]][$1]) dnl
 ])
])


dnl ---------------------------------------------------------------------------


AC_DEFUN([_MYSQL_MODULE_META_CHECK], [ifelse($#, 0, [], $#, 1, dnl
[_MYSQL_CHECK_PLUGIN_META([$1], [__mysql_]m4_bpatsubst($1, -, _)[_plugins__]) dnl
], dnl
[_MYSQL_CHECK_PLUGIN_META([$1], [__mysql_]m4_bpatsubst($1, -, _)[_plugins__]) dnl
_MYSQL_MODULE_META_CHECK(m4_shift($@))]) dnl
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN_META], [
  [$1] ) dnl
m4_ifdef([$2], [
    mysql_modules='m4_bpatsubst($2, :, [ ])' dnl
],[
    mysql_modules='' dnl
])
    ;; dnl
])


dnl ---------------------------------------------------------------------------


AC_DEFUN([_MYSQL_PLUGAPPEND],[ dnl
 m4_ifdef([$1],[ dnl
  m4_define([__plugin_append_tmp__], m4_defn([$1])) dnl
  m4_undefine([$1]) dnl
  m4_define([$1], __plugin_append_tmp__[:$2]) dnl
  m4_undefine([__plugin_append_tmp__]) dnl
 ],[ dnl
  m4_define([$1], [$2]) dnl
  $3 dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_PLUGAPPEND_OPTS],[ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  AC_FATAL([[bad number of args]])
 ], $#, 2, [ dnl
  _MYSQL_PLUGAPPEND_OPTONE([$1],[$2]) dnl
 ],[ dnl
  _MYSQL_PLUGAPPEND_OPTONE([$1],[$2]) dnl
  _MYSQL_PLUGAPPEND_OPTS([$1], m4_shift(m4_shift($@)))
 ])
])

AC_DEFUN([_MYSQL_PLUGAPPEND_OPTONE],[ dnl
 ifelse([$2], [all], [ dnl
  AC_FATAL([[protected plugin group: all]]) dnl
 ],[ dnl
  ifelse([$2], [none], [ dnl
   AC_FATAL([[protected plugin group: none]]) dnl
  ],[ dnl
   _MYSQL_PLUGAPPEND([__mysql_$1_configs__],[$2]) dnl
   _MYSQL_PLUGAPPEND([__mysql_]m4_bpatsubst($2, -, _)[_plugins__],[$1], [ dnl
    _MYSQL_PLUGAPPEND([__mysql_metaplugin_list__],[$2]) dnl
   ]) dnl
  ]) dnl
 ]) dnl
])


dnl ---------------------------------------------------------------------------


AC_DEFUN([MYSQL_LIST_PLUGINS],[ dnl
 m4_ifdef([__mysql_plugin_list__],[ dnl
  _MYSQL_LIST_PLUGINS(m4_bpatsubst(__mysql_plugin_list__, :, [,])) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_LIST_PLUGINS],[ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  MYSQL_SHOW_PLUGIN([$1]) dnl
 ],[ dnl
  MYSQL_SHOW_PLUGIN([$1]) dnl
  _MYSQL_LIST_PLUGINS(m4_shift($@)) dnl
 ]) dnl
])

AC_DEFUN([MYSQL_SHOW_PLUGIN],[ dnl
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

AC_DEFUN([_MYSQL_SHOW_PLUGIN],[
  === Plug-in: $3 ===
  Module Name:      [$1]
  Description:      $4
  Supports build:   _PLUGIN_BUILD_TYPE([$7],[$8]) dnl
m4_ifdef([$12],[
  Configurations:   m4_bpatsubst($12, :, [, ])]) dnl
m4_ifdef([$10],[
  Status:           disabled], [ dnl
m4_ifdef([$9],[
  Status:           mandatory])])])

AC_DEFUN([_PLUGIN_BUILD_TYPE], dnl
[m4_ifdef([$1],[ifelse($1,[no],[],[static ]m4_ifdef([$2],[and dnl
]))])[]m4_ifdef([$2],[dynamic],[m4_ifdef([$1],[],[static])])])


dnl ---------------------------------------------------------------------------


AC_DEFUN([_MYSQL_MODULE_ARGS_CHECK],[ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  _MYSQL_CHECK_PLUGIN_ARG([$1],
   [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
   [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1])) dnl
 ],[ dnl
  _MYSQL_CHECK_PLUGIN_ARG([$1],
   [MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),
   [MYSQL_MODULE_ACTIONS_]AS_TR_CPP([$1])) dnl
  _MYSQL_MODULE_ARGS_CHECK(m4_shift($@)) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN_ARG],[ dnl
 m4_ifdef([$3], [], [m4_define([$3],[ ])])
    [$1] ) dnl
 m4_ifdef([$2],[
      AC_MSG_ERROR([plugin $1 is disabled]) dnl
 ],[
      [mysql_module_]m4_bpatsubst([$1], -, _)=yes dnl
 ])
      ;; dnl
])

AC_DEFUN([_MYSQL_SANE_VARS], [ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  _MYSQL_SANEVAR([$1]) dnl
 ],[ dnl
  _MYSQL_SANEVAR([$1]) dnl
  _MYSQL_SANE_VARS(m4_shift($@)) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_SANEVAR], [
   test -z "[$mysql_module_]m4_bpatsubst([$1], -, _)" && dnl
[mysql_module_]m4_bpatsubst([$1], -, _)='.'
   test -z "[$with_module_]m4_bpatsubst([$1], -, _)" && dnl
[with_module_]m4_bpatsubst([$1], -, _)='.' dnl
])

AC_DEFUN([_MYSQL_CHECK_DEPENDENCIES], [ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  _MYSQL_CHECK_DEPENDS([$1],[__mysql_plugdepends_$1__]) dnl
 ],[ dnl
  _MYSQL_CHECK_DEPENDS([$1],[__mysql_plugdepends_$1__]) dnl
  _MYSQL_CHECK_DEPENDENCIES(m4_shift($@)) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_CHECK_DEPENDS], [ dnl
 m4_ifdef([$2], [
   if test "[$mysql_module_]m4_bpatsubst([$1], -, _)" = yes -a \
           "[$with_module_]m4_bpatsubst([$1], -, _)" != no -o \
           "[$with_module_]m4_bpatsubst([$1], -, _)" = yes; then dnl
     _MYSQL_GEN_DEPENDS(m4_bpatsubst($2, :, [,]))
   fi dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_GEN_DEPENDS], [ dnl
 ifelse($#, 0, [], $#, 1, [ dnl
  _MYSQL_GEN_DEPEND([$1]) dnl
 ],[ dnl
  _MYSQL_GEN_DEPEND([$1]) dnl
  _MYSQL_GEN_DEPENDS(m4_shift($@)) dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_GEN_DEPEND], [ dnl
 m4_ifdef([MYSQL_MODULE_DISABLED_]AS_TR_CPP([$1]),[
      AC_MSG_ERROR([depends upon disabled module $1]) dnl
 ],[
      [mysql_module_]m4_bpatsubst([$1], -, _)=yes
      if test "[$with_module_]m4_bpatsubst([$1], -, _)" = no; then
        AC_MSG_ERROR([depends upon disabled module $1])
      fi dnl
 ]) dnl
])

AC_DEFUN([_MYSQL_CHECK_PLUGIN_ARGS],[
 AC_ARG_WITH([modules], [
   --with-modules=PLUGIN[[,PLUGIN..]]
m4_text_wrap([Plugin modules to include in mysqld. (default is: $1)
Must be configuration name or a comma seperated list of modules.],
[                          ])
m4_text_wrap([Available configurations are: ]
m4_bpatsubst(m4_ifdef([__mysql_metaplugin_list__], dnl
none:all:__mysql_metaplugin_list__,none:all), :, [ ])[.],
[                          ])
m4_text_wrap([Available plugin modules are: ] dnl
m4_bpatsubst(__mysql_plugin_list__, :, [ ])[.], [                          ])
  --without-module-PLUGIN
m4_text_wrap([Disable the named module from being built. Otherwise, 
for modules which are not selected for inclusion in mysqld will be 
built dynamically (if supported)],[                          ])
],[mysql_modules="`echo $withval | tr ',.:;*[]' '       '`"], 
  [mysql_modules=['$1']])

m4_divert_once([HELP_VAR_END],[
Description of plugin modules:
m4_indir([MYSQL_LIST_PLUGINS])
])

  case "$mysql_modules" in
  all )
    mysql_modules='m4_bpatsubst(__mysql_plugin_list__, :, [ ])'
    ;;
  none )
    mysql_modules=''
    ;; dnl
m4_ifdef([__mysql_metaplugin_list__],[ dnl
_MYSQL_MODULE_META_CHECK(m4_bpatsubst(__mysql_metaplugin_list__, :, [,])) dnl
])
  esac

  for plugin in $mysql_modules; do
    case "$plugin" in
    all )
      AC_MSG_ERROR([bad module name: $plugin])
      ;;
    none )
      AC_MSG_ERROR([bad module name: $plugin])
      ;; dnl
_MYSQL_MODULE_ARGS_CHECK(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
    * )
      AC_MSG_ERROR([unknown plugin module: $plugin])
      ;;
    esac
  done

  _MYSQL_SANE_VARS(m4_bpatsubst(__mysql_plugin_list__, :, [,]))  
  _MYSQL_CHECK_DEPENDENCIES(m4_bpatsubst(__mysql_plugin_list__, :, [,]))
])

AC_DEFUN([_MYSQL_POST_PLUGIN_FIXUP],[
  for plugdir in $mysql_plugin_dirs; do
    case "$plugdir" in
    storage/* )
      mysql_se_dirs="$mysql_se_dirs `echo $plugdir | sed -e 's@^storage/@@'`"
      ;;
    plugin/* )
      mysql_pg_dirs="$mysql_pg_dirs `echo $plugdir | sed -e 's@^plugin/@@'`"
      ;;
    *)
      AC_MSG_ERROR([don't know how to handle plugin dir $plugdir])      
      ;;    
    esac
  done
  AC_SUBST(mysql_se_dirs)
  AC_SUBST(mysql_pg_dirs)
])

dnl ===========================================================================
