# Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/cmd-line-utils)

ADD_DEFINITIONS(-DHAVE_CONFIG_H -DNO_KILL_INTR)

INCLUDE_DIRECTORIES(${CURSES_INCLUDE_PATH})

ADD_LIBRARY(readline
  readline.c 
  funmap.c 
  keymaps.c
  vi_mode.c
  parens.c
  rltty.c
  complete.c
  bind.c
  isearch.c
  display.c
  signals.c
  util.c
  kill.c
  undo.c
  macro.c
  input.c
  callback.c
  terminal.c
  xmalloc.c
  history.c
  histsearch.c
  histexpand.c
  histfile.c
  nls.c
  search.c
  shell.c
  tilde.c
  misc.c
  text.c
  mbutil.c
  compat.c
  savestring.c
)

# Declare dependency
# so every executable that links with readline links with curses as well
TARGET_LINK_LIBRARIES(readline ${CURSES_LIBRARY})
