/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef X_CLIENT_MYSQLXCLIENT_MYSQLXCLIENT_ERROR_H_
#define X_CLIENT_MYSQLXCLIENT_MYSQLXCLIENT_ERROR_H_

#define CR_X_ERROR_FIRST                   2500
#define CR_X_READ_TIMEOUT                  2500
#define CR_X_WRITE_TIMEOUT                 2501
#define CR_X_INTERNAL_ABORTED              2502
#define CR_X_TLS_WRONG_CONFIGURATION       2503
#define CR_X_INVALID_AUTH_METHOD           2504
#define CR_X_UNSUPPORTED_OPTION_VALUE      2505
#define CR_X_UNSUPPORTED_CAPABILITY_VALUE  2506
#define CR_X_UNSUPPORTED_OPTION            2507
#define CR_X_LAST_COMMAND_UNFINISHED       2508
#define CR_X_RECEIVE_BUFFER_TO_SMALL       2509
#define CR_X_ERROR_LAST                    2509

#endif  // X_CLIENT_MYSQLXCLIENT_MYSQLXCLIENT_ERROR_H_
