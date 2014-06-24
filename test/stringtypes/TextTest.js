/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

// Domain Object Constructors
var test_id = 1;

var ValueVerifier = require("./lib.js").ValueVerifier;
var ErrorVerifier = require("./lib.js").ErrorVerifier;
var BufferVerifier = require("./lib.js").BufferVerifier;


function TextBlobData() {           // id, blob_col, text_col
  if(this.id === undefined) { this.id = test_id++; }
}

function TextCharsetData() {        // id, ascii_text, latin1_txt, utf16_text
  if(this.id === undefined) { this.id = test_id++; }
}

new mynode.TableMapping("test.text_blob_test").applyToClass(TextBlobData);
new mynode.TableMapping("test.text_charset_test").applyToClass(TextCharsetData);


// Insert and Read a blob
var t1 = new harness.ConcurrentTest("t1:WriteAndReadBlob");
t1.run = function() {
  var data, value, i, verifier;
  data = new TextBlobData();
  value = new Buffer(20000);
  for(i = 0 ; i < 20000 ; i++) {
    value[i] = Math.ceil(Math.random() * 256);
  }
  data.blob_col = value;
  verifier = new BufferVerifier(this, "blob_col", value);
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      t1.errorIfError(err);
      session.find(TextBlobData, data.id, verifier.run);
    });
  });
};

var t2 = new harness.ConcurrentTest("t2:WriteAndReadText");
t2.run = function() {
  var data, verifier;
  data = new TextBlobData();
  data.text_col = "String.";
  verifier = new ValueVerifier(this, "text_col", data.text_col);
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      t2.errorIfError(err);
      session.find(TextBlobData, data.id, verifier.run);
    });
  });
};

/* Write & Read both TEXT and BLOB columns of a single record */
var t3 = new harness.ConcurrentTest("t3:TextAndBlob");
t3.run = function() { 
  var data = new TextBlobData();
  data.blob_col = new Buffer([1,2,3,4,5,6,7,8,9,10,90,89,88,87,86,85,84]);
  data.text_col = "// {{ ?? }} \\";
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      session.find(TextBlobData, data.id, function(err, obj) {
        testCase.errorIfError(err);
        testCase.errorIfNotEqual("BLOB value", data.blob_col, obj.blob_col);
        testCase.errorIfNotEqual("TEXT value", data.text_col, obj.text_col);
        testCase.failOnError();
      });
    });
  });
};

var t4 = new harness.ConcurrentTest("t4:AsciiText");
t4.run = function() {
  var i, data, verifier;
  data = new TextCharsetData();
  data.ascii_text = "";
  for(i = 1 ; i < 127 ; i++) { data.ascii_text += String.fromCharCode(i); } 
  verifier = new ValueVerifier(this, "ascii_text", data.ascii_text);
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      testCase.errorIfError(err);
      session.find(TextCharsetData, data.id, verifier.run);
    });
  });
};

var t5 = new harness.ConcurrentTest("t5:Utf16Text");
t5.run = function() {
  var i, data, verifier;
  data = new TextCharsetData();
  data.utf16_text = "  Send a ☃ to college!  ";
  verifier = new ValueVerifier(this, "utf16_text", data.utf16_text);
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      testCase.errorIfError(err);
      session.find(TextCharsetData, data.id, verifier.run);
    });
  });
};  

var t6 = new harness.ConcurrentTest("t6:Latin1Text");
t6.run = function() {
  var i, data, verifier;
  data = new TextCharsetData();
  data.latin1_text = "gøød b¥te-stream éncØding of multi-byte Çhâracter sets.";
  verifier = new ValueVerifier(this, "latin1_text", data.latin1_text);
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      testCase.errorIfError(err);
      session.find(TextCharsetData, data.id, verifier.run);
    });
  });
};  

var t7 = new harness.ConcurrentTest("t7:AsciiAndUtf16Test");
t7.run = function() {
  var data = new TextCharsetData();
  data.ascii_text = "  #$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLM  ";
  data.latin1_text = null;
  data.utf16_text = "  Send a ☃ to college!  ";
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      session.find(TextCharsetData, data.id, function(err, obj) {
        testCase.errorIfError(err);
        testCase.errorIfNotEqual("ASCII value", data.ascii_text, obj.ascii_text);
        testCase.errorIfNotNull("Latin1 value should be null", obj.latin1_text);
        testCase.errorIfNotEqual("UTF16 value", data.utf16_text, obj.utf16_text);
        testCase.failOnError();
      });
    });
  });
};

var t8 = new harness.ConcurrentTest("t8:PersistReadModifyUpdateText");
t8.run = function() {
  var data = new TextCharsetData(13);
  data.ascii_text = "  #$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLM  ";
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      session.find(TextCharsetData, data.id, function(err, obj) {
        var new_text = "  #$%&'()*+,-./012345678";
        obj.ascii_text = new_text;
        session.update(obj, function(err) {
          testCase.errorIfError(err);
          var verifier = new ValueVerifier(testCase, "ascii_text", new_text);
          session.find(TextCharsetData, data.id, verifier.run);
        });
      });
    });
  });
};


module.exports.tests = [ t1 , t2 , t3 , t4 , t5 , t6 , t7 , t8 ];
