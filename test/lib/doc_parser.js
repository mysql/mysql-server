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

/*global fs */

/* Returns a list of function definitions 
*/
function scan(text) { 
  var i = 0;                  // the index of the current character 
  var c = text.charAt(i);     // the current character
  var list = [];              // functions found in the file
  var tok;                    // the current token

  function isAlpha() { 
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c === '_'));
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
    list.push(this.str);
  };

  // Start scanning
  while(c) {
  
    if(c <= ' ') { advance(); }                       // whitespace
     
    else if(c == '/' && peek() == '/') {              // comment to EOL  
      advance(2);
      while(c !== '\n' && c !== '\r' && c !== '') {
        advance();
      }
    }
    
    else if (c === '/' && peek() === '*') {           // comment to */
      advance(2); 
      while(c && c !== '*' && peek() !== '/') {
        advance();
      }
      advance(2);
      if(c === '') { throw Error("Unterminated comment"); }
    }
 
    else if(isAlpha()) {                              // candidate functions
      tok = new Token();
      while(isAlpha()) {
        tok.consume();
      }
      if(c == '(') { tok.commit(); }  // IT WAS A FUNCTION
      /* Now, there may be more functions (callbacks) defined as arguments,
         so we skip to the next semicolon */
      while(c && c !== ';') {
        advance();
      }
    }
    
    else advance();
  }
  return list;
}


function listFunctions(docFileName) {
  var text = fs.readFileSync(docFileName, 'utf8');
  return scan(text);
}

function ClassTester(obj) {
  this.class = obj;
}

ClassTester.prototype.assertTest = function(functionList) {
  var func, name;
  while(name = functionList.pop()) {
    func = this.class[name];
    assert.equal(typeof func, 'function', "No Function: " + name);
  }
};

ClassTester.prototype.test = function(functionList, testCase) {
  var func, name, missing = 0, firstMissing = null, msg;
  while(name = functionList.pop()) {
    func = this.class[name];
    if(typeof func !== 'function') {
      if(! firstMissing) { firstMissing = name; }
      missing += 1;
    }
  }
  if(missing) {
    msg = "Missing " + firstMissing;
    if(missing > 1)  { msg += " and " + (missing-1) + " other function"; }
    if(missing > 2)  { msg += "s"; }
    testCase.fail(msg);
  }
  else {
    testCase.pass();
  }
}

exports.listFunctions = listFunctions;
exports.ClassTester   = ClassTester;

