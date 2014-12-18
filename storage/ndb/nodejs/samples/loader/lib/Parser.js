/*
 Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights
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


/*
   This is a simple top-down recursive descent parser.
   When the grammar defines a set of alternatives, the parser iterates through
   them, and chooses the first one which matches the input.
   It cannot handle left recursion in the grammar, where a NonTerminal
   LHS appears first in its own RHS expansion. 
 */


/////////////// NonTerminal //////////////

function NonTerminal(parser, elements) {
  var i;
  this.parser = parser;
  if(elements) {
    this.members = new Array(elements.length);
    for(i = 0 ; i < elements.length ; i++) {
      this.members[i] = elements[i];
    }
  }
}

NonTerminal.prototype = {
  isNonTerminal : true,
  strict        : false,
  parser        : null,
  ebnf          : null,
  members       : null,
  innerEval     : null,
  isRepeat      : null,
  visitorFunc   : null,
  eval          : function unset() { throw { message : "UNDEF EVAL" }; }
};

NonTerminal.prototype.set = function(that) {
  this.strict    = that.strict;
  this.ebnf      = that.ebnf;
  this.members   = that.members;
  this.innerEval = that.innerEval;
  this.isRepeat  = that.isRepeat;
  this.eval      = that.eval;
};

NonTerminal.prototype.describe = function() {
  var i, m, str;
  str = this.ebnf[0] + " ";
  for(i = 0 ; i < this.members.length ; i++) {
    m = this.members[i];
    if(i) { str += this.ebnf[1]; }
    str += this.parser.describe(m) + " ";
  }
  str += this.ebnf[2];
  return str;
};


/////////////// Node of parsed tree ///////////////

var nextNodeId = 0;

function Node(nonTerminal) {
  this.nonTerminal = nonTerminal;
  this.children = [];
  this.id = ++nextNodeId;
}

Node.prototype.isNode = true;

/* Node.visit() : takes a visitor and an object that 
   holds the semantic "transform" of the parsed tree.
   If a node has a defined visitor function, call it;
   otherwise call the generic visitor.
*/
Node.prototype.visit = function(visitor, transform) {
  var i;
  if(typeof visitor[this.nonTerminal.visitorFunc] === 'function') {
    visitor[this.nonTerminal.visitorFunc](this, transform);
  } else {
    this.visitChildNodes(visitor, transform);
  }
};

/* This is the generic visitor.  It will do two things:
    (a) visit all NonTerminal child Nodes.
    (b) if the transform is actually an array (a "collector"),
        push terminal token values onto the collector array.
*/
Node.prototype.visitChildNodes = function(visitor, collector) {
  var i;
  for(i = 0 ; i < this.children.length ; i++) {
    if(this.children[i]) {
      if(this.children[i].isNode) {
        this.children[i].visit(visitor, collector);
      } else if(Array.isArray(collector)) {
        collector.push(this.children[i].value);
      }
    }
  }
};

function _getTerminal(array, type, index) {
  var i, j;
  j = 0;
  for(i = 0 ; i < array.length ; i++) {
    if(array[i] !== null
       && !(array[i].isNode)
       && ((array[i].type === type) || !type)
       && j++ === index)                           { return array[i].value;   }
  }
  return null;
}

Node.prototype.getString = function(index) {
  return _getTerminal(this.children, 'string', index);
};

Node.prototype.getNumber = function(index) {
  return _getTerminal(this.children, 'number', index);
};

Node.prototype.getName = function(index) {
  return _getTerminal(this.children, 'name', index);
};

Node.prototype.getVariable = function(index) {
  return _getTerminal(this.children, 'variable', index);
};

Node.prototype.getToken = function(index) {
  return _getTerminal(this.children, null, index);
};

/////////////// Parser //////////////

function Parser() {
}

/* The "text" here is the raw input text for the scanner; 
   it will be used to produce error messages.
*/
Parser.prototype.setText = function(txt) {
  this.text = txt;
};

Parser.prototype.Nonterminal = function(visitor) {
  var nt = new NonTerminal(this);
  if(typeof visitor === 'string' ) {
    nt.visitorFunc = visitor;
  }
  return nt;
};

Parser.prototype.begin = function(tokens) {
  this.index = 0;        // index of the current token
  this.tokens = tokens;  // array of tokens created by the tokenizer
};

Parser.prototype.fail = function(msg) {
  var start_line = 0,
      lines = [],
      token = this.tokens[this.index],
      i = 0,
      ctx = "";
  /* Excerpt original text into the error message */
  if(this.text) {
    start_line = (token.line > 2 ? token.line-1 : 1);
    lines = this.text.split('\n');    
    ctx += lines[start_line-1] + "\n";
    if(token.line > start_line) ctx += lines[token.line-1] + "\n";
    /* Now indent up to a caret at the errant token */
    while(++i < token.column) ctx += ' ';
    ctx += "^^^\n";
  }
  var err = new Error("\n" + ctx + msg);
  err.token = token;
  throw err;
};

Parser.prototype.done = function() {
  if(this.index < this.tokens.length) {
    this.fail("Extra text after parsed statement.");
  }
  this.final_line = this.tokens[this.tokens.length-1].line;
};

/* def(lhs, rhs)
   Sets the stub NonTerminal on the left hand side
   to behave as the fully formed one from the right hand side
*/
Parser.prototype.def = function(lhs, rhs) {
  lhs.set(rhs);
};

/* Call def() on a list of RHS, LHS pairs 
*/
Parser.prototype.defineProductions = function() {
  var i;
  for(i = 0 ; i < arguments.length ; i += 2) {
    this.def(arguments[i], arguments[ i+1 ]);
  }
}


/* Describe Terminals, and let NonTerminals describe themselves
*/
Parser.prototype.describe = function(sym) {
  if(typeof sym === 'string')       { return sym;            }
  if(typeof sym === 'undefined')    { return "undefined";    } 
  if(sym === null)                  { return "null";         }
  if(sym.isNonTerminal)             { return sym.describe(); } 
};


/* evalSeries() 
   Evaluate a series of tokens.
   If any strict token fails to match, return null. 
   After one match, any subsequent strict token that fails to match 
   should cause the parser to fail.
*/
function evalSeries() {  
  var i = 0,       /* Iterator */
      sym,         /* The current element */
      r = null,    /* Result of evaluating sym */ 
      s = 0,       /* Number of succesful evaluations */
      strict_sym,  /* Strictness of sym */
      node = new Node(this);

  do {
    sym = this.members[i];
    strict_sym = sym.isNonTerminal ? sym.strict : true;
    node.children[i] = r =
      this.parser.eval(sym, (this.strict && strict_sym && (s > 0) ));
    if(r !== null) { s += 1; } // Enable strictness
    i += 1;
  } while(i < this.members.length && (! (r === null && strict_sym)));

  return (s == 0 ? null : node);
}

/* evalAlternates()
   Evaluate a list of alternatives.
   Return the first match found in the grammar, or null if none match.
*/
function evalAlternates() {
  var r, i, node ;
  node = new Node(this);
  for(i = 0 ; i < this.members.length ; i++) {
    r = this.parser.eval(this.members[i]);
    if(r !== null) {
      node.children[0] = r;
      return node;
    } 
  }
  return null;
}

/* evalSeveral()
   Evaluate a series of tokens for 0 or more matches against an inner eval.
   Return null if zero matches; return a Node if 1 or more matches.
*/
function evalSeveral() {
  var a = null;
  var r = this.innerEval();
  if(r !== null) {
    a = new Node(this);
    while(r !== null) {
      a.children.push(r);
      r = this.innerEval();
    }
  }
  return a;
};


///////    NonTerminal Factories: Series, Several, Alts, Option      ///////

/* Series: A sequence of tokens, e.g. "LOAD" "DATA" "INFILE"
*/
Parser.prototype.Series = function() {
  var nt  = new NonTerminal(this, arguments);
  nt.strict = true;
  nt.ebnf  = [ '','','' ];
  nt.eval = evalSeries;
  return nt;
};

/* Alternatives:  ( a | b | c )
*/
Parser.prototype.Alts = function() {
  var nt = new NonTerminal(this, arguments);
  nt.strict = true;
  nt.ebnf = [ '(' , '| ' , ')' ];
  nt.eval = evalAlternates;
  return nt;
};

/* "Option" means 0 or 1.  "[a] b" could be either ab or b.
   Option is like Series, but non-strict.
*/
Parser.prototype.Option = function() {
  var nt  = new NonTerminal(this, arguments);
  nt.ebnf = [ '[' , '', ']' ];
  nt.eval = evalSeries;
  return nt;
};

/* "Several" means 0 or more, as in the EBNF production "list := item { , item }"
*/
Parser.prototype.Several = function() {
  var nt = new NonTerminal(this, arguments);
  nt.ebnf = [ '{', '', '}' ];
  nt.isRepeat = true;
  nt.innerEval = evalSeries;
  nt.eval = evalSeveral;
  return nt;
};


/* Parser.eval()
   Evaluate the current token against a parser symbol.
   If the token matches and is a Terminal, advance the token stream,
   and return the matching token.
   If the symbol is a NonTerminal, delegate to its own eval() method.
   Return null if no match.
   If strict mode is set, and no match, fail the parser. 
*/
Parser.prototype.eval = function(sym, strict) {
  if(debug) console.log("Parser eval", this.describe(sym));
  var r = null;
  var t = this.tokens[this.index];
  if(t) {
    if(typeof sym === 'string') {  /* Terminal */
      if( ('{' + t.type + '}' === sym)   ||
            (t.value === sym)            || 
            (typeof t.value === 'string' && 
              (t.value.toUpperCase() === sym.toUpperCase())))         { r = t; }

      if(r !== null) {
        if(debug) console.log("Parser Advance", t.value);
        this.index += 1;   // advance to the next token
      }
    }
    else if(sym.isNonTerminal) {
      r = sym.eval();
    }
   if(strict === true && r === null) {
      this.fail("Expected " + this.describe(sym));
    }
  }
  return r;
};

module.exports.Parser = Parser;
