/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "HandleDbc.hpp"

HandleDbc::FuncTab
HandleDbc::m_funcTab[] = {
    {   SQL_API_SQLALLOCCONNECT       , 1  },
    {   SQL_API_SQLALLOCENV           , 1  },
    {   SQL_API_SQLALLOCHANDLE        , 1  },
    {   SQL_API_SQLALLOCHANDLESTD     , 0  },
    {   SQL_API_SQLALLOCSTMT          , 1  },
    {   SQL_API_SQLBINDCOL            , 1  },
    {   SQL_API_SQLBINDPARAM          , 1  },
    {   SQL_API_SQLBINDPARAMETER      , 1  },
    {   SQL_API_SQLBROWSECONNECT      , 0  },
    {   SQL_API_SQLBULKOPERATIONS     , 0  },
    {   SQL_API_SQLCANCEL             , 1  },
    {   SQL_API_SQLCLOSECURSOR        , 1  },
    {   SQL_API_SQLCOLATTRIBUTE       , 1  },
    {   SQL_API_SQLCOLATTRIBUTES      , 1  },
    {   SQL_API_SQLCOLUMNPRIVILEGES   , 0  },
    {   SQL_API_SQLCOLUMNS            , 1  },
    {   SQL_API_SQLCONNECT            , 1  },
    {   SQL_API_SQLCOPYDESC           , 0  },
    {   SQL_API_SQLDATASOURCES        , 0  },
    {   SQL_API_SQLDESCRIBECOL        , 1  },
    {   SQL_API_SQLDESCRIBEPARAM      , 0  },
    {   SQL_API_SQLDISCONNECT         , 1  },
    {   SQL_API_SQLDRIVERCONNECT      , 1  },
    {   SQL_API_SQLDRIVERS            , 0  },
    {   SQL_API_SQLENDTRAN            , 1  },
    {   SQL_API_SQLERROR              , 1  },
    {   SQL_API_SQLEXECDIRECT         , 1  },
    {   SQL_API_SQLEXECUTE            , 1  },
    {   SQL_API_SQLEXTENDEDFETCH      , 0  },
    {   SQL_API_SQLFETCH              , 1  },
    {   SQL_API_SQLFETCHSCROLL        , 0  },
    {   SQL_API_SQLFOREIGNKEYS        , 0  },
    {   SQL_API_SQLFREECONNECT        , 1  },
    {   SQL_API_SQLFREEENV            , 1  },
    {   SQL_API_SQLFREEHANDLE         , 1  },
    {   SQL_API_SQLFREESTMT           , 1  },
    {   SQL_API_SQLGETCONNECTATTR     , 1  },
    {   SQL_API_SQLGETCONNECTOPTION   , 1  },
    {   SQL_API_SQLGETCURSORNAME      , 1  },
    {   SQL_API_SQLGETDATA            , 1  },
    {   SQL_API_SQLGETDESCFIELD       , 1  },
    {   SQL_API_SQLGETDESCREC         , 1  },
    {   SQL_API_SQLGETDIAGFIELD       , 1  },
    {   SQL_API_SQLGETDIAGREC         , 1  },
    {   SQL_API_SQLGETENVATTR         , 1  },
    {   SQL_API_SQLGETFUNCTIONS       , 1  },
    {   SQL_API_SQLGETINFO            , 1  },
    {   SQL_API_SQLGETSTMTATTR        , 1  },
    {   SQL_API_SQLGETSTMTOPTION      , 1  },
    {   SQL_API_SQLGETTYPEINFO        , 1  },
    {   SQL_API_SQLMORERESULTS        , 1  },
    {   SQL_API_SQLNATIVESQL          , 0  },
    {   SQL_API_SQLNUMPARAMS          , 1  },
    {   SQL_API_SQLNUMRESULTCOLS      , 1  },
    {   SQL_API_SQLPARAMDATA          , 1  },
    {   SQL_API_SQLPARAMOPTIONS       , 0  },
    {   SQL_API_SQLPREPARE            , 1  },
    {   SQL_API_SQLPRIMARYKEYS        , 1  },
    {   SQL_API_SQLPROCEDURECOLUMNS   , 0  },
    {   SQL_API_SQLPROCEDURES         , 0  },
    {   SQL_API_SQLPUTDATA            , 1  },
    {   SQL_API_SQLROWCOUNT           , 1  },
    {   SQL_API_SQLSETCONNECTATTR     , 1  },
    {   SQL_API_SQLSETCONNECTOPTION   , 1  },
    {   SQL_API_SQLSETCURSORNAME      , 1  },
    {   SQL_API_SQLSETDESCFIELD       , 1  },
    {   SQL_API_SQLSETDESCREC         , 1  },
    {   SQL_API_SQLSETENVATTR         , 1  },
    {   SQL_API_SQLSETPARAM           , 1  },
    {   SQL_API_SQLSETPOS             , 0  },
    {   SQL_API_SQLSETSCROLLOPTIONS   , 0  },
    {   SQL_API_SQLSETSTMTATTR        , 1  },
    {   SQL_API_SQLSETSTMTOPTION      , 1  },
    {   SQL_API_SQLSPECIALCOLUMNS     , 0  },
    {   SQL_API_SQLSTATISTICS         , 0  },
    {   SQL_API_SQLTABLEPRIVILEGES    , 0  },
    {   SQL_API_SQLTABLES             , 1  },
    {   SQL_API_SQLTRANSACT           , 1  },
    {   0                             , -1 }
};
