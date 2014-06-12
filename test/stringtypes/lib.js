/*
 Copyright (c) 2014 , Oracle and/or its affiliates. All rights
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

"use strict";

function ErrorVerifier(testCase, sqlState) {
  this.run = function onRead(err, rowRead) {
    var message = "Expected SQLState " + sqlState;
    if(testCase.errorIfUnset(message, err && err.cause && err.cause.sqlstate)) 
    {
      testCase.errorIfNotEqual("Expected sqlstate", sqlState, err.cause.sqlstate);
    }
    testCase.failOnError();
  };
}

function ValueVerifier(testCase, field, value) {
  this.run = function onRead(err, rowRead) {
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    try {
      if(value !== rowRead[field]) {
        testCase.errorIfNotEqual("length", value.length, rowRead[field].length);
        testCase.errorIfNotEqual(field, value, rowRead[field]);
      }
    }
    catch(e) {
      testCase.appendErrorMessage(e);
    }
    testCase.failOnError();
  };
}

function BufferVerifier(testCase, field, expectedValue) {
  this.run = function onRead(err, rowRead) {
    var buffer, len, i;
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    len = expectedValue.length;
    try {  
      buffer = rowRead[field];
      testCase.errorIfNotEqual("length", len, buffer.length);
      if(len == buffer.length) {
        for(i = 0; i < len ; i++) {
          testCase.errorIfNotEqual("mismatch at position " + i, 
                                  expectedValue[i], buffer[i]);
          if(expectedValue[i] !== buffer[i]) break;
        }
      }
    }
    catch(e) {
      testCase.appendErrorMessage(e);
    }
    testCase.failOnError();
  };
}

exports.ErrorVerifier = ErrorVerifier;
exports.BufferVerifier = BufferVerifier;
exports.ValueVerifier = ValueVerifier;
