/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/


#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "NdbJsConverters.h"

using namespace v8;

void NdbTransaction_initOnLoad(Handle<Object> target) {
  DEFINE_JS_INT(target, "NoCommit", NdbTransaction::NoCommit);
  DEFINE_JS_INT(target, "Commit", NdbTransaction::Commit);
  DEFINE_JS_INT(target, "Rollback", NdbTransaction::Rollback);
  DEFINE_JS_INT(target, "DefaultAbortOption", NdbOperation::DefaultAbortOption);
  DEFINE_JS_INT(target, "AbortOnError", NdbOperation::AbortOnError);
  DEFINE_JS_INT(target, "AO_IgnoreError", NdbOperation::AO_IgnoreError);
  DEFINE_JS_INT(target, "NotStarted", NdbTransaction::NotStarted);
  DEFINE_JS_INT(target, "Started", NdbTransaction::Started);
  DEFINE_JS_INT(target, "Committed", NdbTransaction::Committed);
  DEFINE_JS_INT(target, "Aborted", NdbTransaction::Aborted);
  DEFINE_JS_INT(target, "NeedAbort", NdbTransaction::NeedAbort);
}
