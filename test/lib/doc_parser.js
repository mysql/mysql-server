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

var udebug = unified_debug.getLogger("doc_parser.js"),
    fs = require("fs");

/* Returns a list of function definitions 
*/
function scan(text) { 
  var i = 0;                  // the index of the current character 
  var c = text.charAt(i);     // the current character
  var list = [];              // functions found in the file
  var constructor = "";       // constructor function found in file
  var tok;                    // the current token

  function isUpper(c)   { return (c >= 'A' && c <= 'Z'); }
  function isLower(c)   { return (c >= 'a' && c <= 'z'); }
  function isAlpha(c)   { return (isUpper(c) || isLower(c)); }
  function isNumeric(c) { return (c >= '0' && c <= '9'); }
  function isJsFunctionName(c) { 
    return( isAlpha(c) || isNumeric(c) || (c == '_'));
  }
  
  function peek() {
    return text.charAt(i + 1);
  }

  function advance(n) {       // Advance to next character
    var amt = n || 1;
    if(i + amt >= text.length) {
      i = text.length;
      c = '';
    }
    else { 
      i += amt;
      c = text.charAt(i);
    }
  }

  function Token() {
    this.str = c;
    advance();
  }
    
  Token.prototype.consume = function() {
    this.str += c;
    advance();
  };
    
  Token.prototype.commit = function() {
    udebug.log("found function:", this.str);
    list.push(this.str);
    if(isUpper(this.str.charAt(0))) {     constructor = this.str;   }
  };

  // Start scanning
  while(c) {
  
    while(c != '' && c <= ' ') { advance(); }          // whitespace
     
    if(c == '/' && peek() == '/') {                    // comment to EOL  
      advance(2);
      while(c !== '\n' && c !== '\r' && c !== '') {
        advance();
      }
    }
    
    else if (c === '/' && peek() === '*') {            // comment to */
      advance(2); 
      while(! (c == '*' && peek() == '/')) {
        advance();
      }
      if(c === '') { throw Error("Unterminated comment"); }
      advance(2);
    }
 
    else if(isAlpha(c)) {                              // candidate functions
      tok = new Token();
      while(isJsFunctionName(c)) {
        tok.consume();
      }
      if(c == '(') {  // IT WAS A FUNCTION
        tok.commit();
        advance();   
        /* Now, there may be more functions (callbacks) defined as arguments,
           so we skip to the next semicolon */
        while(c && c !== ';') {
          advance();
        }
      }
      delete tok;
    }
    
    else {
      advance();
    }
  }
  list._found_constructor = constructor;
  return list;
}


function listFunctions(docFileName) {
  var text = fs.readFileSync(docFileName, 'utf8');
  return scan(text);
}

function ClassTester(obj, docClassName) {
  this.class = obj;
  this.file = docClassName;
}

ClassTester.prototype.test = function(functionList, testCase) {
  var func, name;
  var msg = "";
  var missing = 0;
  var firstMissing = null;
  var documentedFunctions = {};
  var i;

  udebug.log("verifying",functionList.length,"functions");

  // Test missing functions 
  for(i = 0 ; i < functionList.length ; i++) { 
    name = functionList[i];
    func = this.class[name];
    documentedFunctions[name] = func;
    if(typeof func !== 'function' && functionList._found_constructor !== name) {
      udebug.log("MISSING FUNCTION", this.file, name);
      if(! firstMissing) { firstMissing = name; }
      missing += 1;
    }
  }
  if(missing) {
    msg = "Missing " + firstMissing;
    if(missing > 1)  { msg += " and " + (missing-1) + " other function"; }
    if(missing > 2)  { msg += "s"; }
  }
  
  // Test undocumented functions
  for(name in this.class) {
    if(this.class.hasOwnProperty(name)) {
      if(typeof this.class[name] === 'function') {
        if(! documentedFunctions[name]) {
          if(msg.length) msg += "; "
          msg += name + " undocumented";
        }
      }
    }
  }

  if(msg) {
    if(testCase) {   testCase.appendErrorMessage(msg);    }
    else         {   throw new Error(msg);  }
  }
}

exports.listFunctions = listFunctions;
exports.ClassTester   = ClassTester;

