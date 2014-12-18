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

"use strict";

var scanner   = require("./Scanner.js"),
    Parser    = require("./Parser.js").Parser,
    LoaderJob = require("./LoaderJob.js").LoaderJob,
    util      = require("util"),
    assert    = require("assert"),
    P         = new Parser();


/* Declare NonTerminals and their semantic visitors */
var LoadDataStatement   = P.Nonterminal("visitLoadDataStatement"),
    StringOrName        = P.Nonterminal("visitStringOrName"),
    DataSource          = P.Nonterminal("visitDataSource"),
    RandomData          = P.Nonterminal("visitRandomData"),
    SpecificData        = P.Nonterminal("visitSpecificData"),
    FileData            = P.Nonterminal("visitFileData"),
    Charset             = P.Nonterminal("visitCharset"),
    CommonDataFormat    = P.Nonterminal("visitCommonDataFormat"),
    DataJSON            = P.Nonterminal("visitDataJSON"),
    DataCSV             = P.Nonterminal("visitDataCSV"),
    LogSpec             = P.Nonterminal("visitLogSpec"),
    InsertMode          = P.Nonterminal("visitInsertMode"),
    Destination         = P.Nonterminal("visitDestination"),
    SQLObject           = P.Nonterminal("visitSQLObject"),
    FieldSpec           = P.Nonterminal("visitFieldSpec"),
    FieldOption         = P.Nonterminal("visitFieldOption"),
    FieldsSeparated     = P.Nonterminal("visitFieldsSeparated"),
    FieldSepOpt         = P.Nonterminal("visitFieldSepOpt"),
    FieldQuoteOpt       = P.Nonterminal("visitFieldQuoteOpt"),
    FieldEscapeOpt      = P.Nonterminal("visitFieldEscapeOpt"),
    Lines               = P.Nonterminal("visitLines"),
    LineSpec            = P.Nonterminal("visitLineSpec"),
    LineStartSpec       = P.Nonterminal("visitLineStartSpec"),
    LineEndSpec         = P.Nonterminal("visitLineEndSpec"),
    Options             = P.Nonterminal("visitOptions"),
    OptComments         = P.Nonterminal("visitOptComments"),
    OptAtomic           = P.Nonterminal("visitOptAtomic"),
    OptIgnore           = P.Nonterminal("visitOptIgnore"),
    OptDolines          = P.Nonterminal("visitOptDolines"),
    OptWorker           = P.Nonterminal("visitOptWorker"),
    OptSpeed            = P.Nonterminal("visitOptSpeed"),
    OptSpeedMeasure     = P.Nonterminal("visitOptSpeedMeasure"),
    ColumnListSpec      = P.Nonterminal("visitColumnListSpec"),
    ColumnsInHeader     = P.Nonterminal("visitColumnsInHeader"),
    ColumnList          = P.Nonterminal("visitColumnList"),
    ColumnDefn          = P.Nonterminal("visitColumnDefn"),
    ColumnPosition      = P.Nonterminal("visitColumnPosition"),
    IgnoredOptions      = P.Nonterminal("visitIgnoredOptions"),
    BeginData           = P.Nonterminal("visitBeginData")
    ;

/* Productions */
P.defineProductions(
  LoadDataStatement , P.Series( "LOAD",
                                P.Option(CommonDataFormat),
                                DataSource,
                                P.Option(LogSpec),
                                P.Option(InsertMode),
                                Destination,
                                P.Option(Charset),
                                P.Option(FieldSpec),
                                P.Option(LineSpec),
                                P.Several(Options),
                                P.Option(ColumnListSpec),
                                P.Option(";"),
                                P.Option(BeginData)
                              ),

  StringOrName     , P.Alts("{string}", "{name}"),

  /* Common Data Format */
  CommonDataFormat , P.Alts(DataJSON, DataCSV),
  DataJSON         , P.Series("JSON"),
  DataCSV          , P.Series("CSV"),

  /* Data Source */
  DataSource       , P.Alts(RandomData, SpecificData),
  RandomData       , P.Series("RANDOM", "DATA"),
  SpecificData     , P.Series("DATA", P.Option(IgnoredOptions),
                               P.Option(FileData)),
  IgnoredOptions   , P.Alts("LOCAL", "CONCURRENT", "LOW_PRIORITY"),
  FileData         , P.Series("INFILE", "{string}"),

  /* Log Spec */
  LogSpec          , P.Series("BADFILE", "{string}"),

  /* Insert Mode */
  InsertMode       , P.Alts("INSERT", "REPLACE", "APPEND", "TRUNCATE", "IGNORE"),

  /* Destination Table */
  Destination      , P.Series("INTO", "TABLE", SQLObject),
  SQLObject        , P.Series(StringOrName, P.Option(".", StringOrName)),

  /* Data Encoding */
  Charset          , P.Series("CHARACTER", "SET", StringOrName),

  /* Field options. */
  FieldSpec        , P.Series(P.Alts("FIELDS","COLUMNS"), FieldOption,
                              P.Several(FieldOption)),
  FieldOption      , P.Alts(FieldsSeparated, FieldQuoteOpt, FieldEscapeOpt),
  FieldsSeparated  , P.Series(P.Alts("TERMINATED","SEPARATED"), "BY", FieldSepOpt),
  FieldSepOpt      , P.Alts("WHITESPACE","{string}"),
  FieldQuoteOpt    , P.Series(P.Option("OPTIONALLY"), "ENCLOSED", "BY",
                              "{string}", P.Option("AND", "{string}")),
  FieldEscapeOpt   , P.Series("ESCAPED", "BY", "{string}"),

  /* Line Options */
  LineSpec         , P.Series("LINES", P.Option(LineStartSpec),
                              P.Option(LineEndSpec)),
  LineStartSpec    , P.Series("STARTING", "BY", "{string}"),
  LineEndSpec      , P.Series("TERMINATED", "BY", "{string}"),

  /* Misc. Options */
  Options          , P.Alts(OptComments, OptAtomic, OptIgnore, OptDolines,
                            OptWorker, OptSpeed),
  OptComments      , P.Series("COMMENTS", "STARTING", "BY", "{string}"),
  OptAtomic        , P.Series("IN", "ONE", "TRANSACTION"),
  Lines            , P.Alts("LINE", "LINES", "ROW", "ROWS"),
  OptIgnore        , P.Series(P.Alts("IGNORE","SKIP"), "{number}", Lines),
  OptDolines       , P.Series("DO", "{number}", Lines),
  OptWorker        , P.Series("WORKER", "{number}", "OF", "{number}"),
  OptSpeed         , P.Series("SPEED", P.Alts("FAST", "SLOW", OptSpeedMeasure)),
  OptSpeedMeasure  , P.Series("{number}", 
                              P.Alts("KB", "MB", "GB", "ROWS"), "PER",
                              P.Alts("HOUR", "MINUTE", "SECOND", "SEC")),

  /* Column Description */
  ColumnListSpec   , P.Alts(ColumnList, ColumnsInHeader),
  ColumnsInHeader  , P.Series("COLUMNS","FROM","HEADER"),
  ColumnList       , P.Series("(" , ColumnDefn, P.Several("," , ColumnDefn),
                              ")" ),
  ColumnDefn       , P.Series(StringOrName, P.Option(ColumnPosition)),
  ColumnPosition   , P.Series("POSITION","(","{number}",":","{number}",")"),

  BeginData        , P.Series("BEGINDATA")
);



/* Visit the parse tree and generate a Loader Job Spec
*/
function SqlVisitor() {
}

// The generic visitor simply visits all children of a node.
// You can declare the methods that override that behavior.


// WORKER 1 OF 3
SqlVisitor.prototype.visitOptWorker = function(node, job) {
  job.setWorkerId(node.getNumber(0), node.getNumber(1));
};

// LOAD RANDOM DATA
SqlVisitor.prototype.visitRandomData = function(node, job) {
  job.generateRandomData();
};

// INFILE {string}
SqlVisitor.prototype.visitFileData = function(node, job) {
  job.setDataFile(node.getString(0));
};

// LOAD DATA ... BEGINDATA
SqlVisitor.prototype.visitBeginData = function(node, job) {
  // Use the control file as the data file; skip all lines up to BEGINDATA
  job.BeginDataAtControlFileLine(node.nonTerminal.parser.final_line);
};

// JSON
SqlVisitor.prototype.visitDataJSON = function(node, job) {
  job.dataSourceIsJSON();
};

SqlVisitor.prototype.visitDataCSV = function(node, job) {
  job.dataSourceIsCSV();
};

SqlVisitor.prototype.visitOptSpeed = function(node, job) {
  var speed;
  speed = node.getName(1);
  if(speed === null) {
    job.controller.speedFast = false;
    job.controller.speedMeasure = [];
    node.visitChildNodes(this, job.controller.speedMeasure);
  } else {
    job.controller.speedFast = (speed.toUpperCase() === 'FAST');
  }
};

// INTO TABLE SqlObject
SqlVisitor.prototype.visitDestination = function(node, job) {
  var collector = [];
  node.visitChildNodes(this, collector);
  switch(collector.length) {
    case 3:                                        // INTO TABLE a
      job.destination.table = collector[2];
      break;
    case 5:                                        // INTO TABLE a . b
      job.destination.database = collector[2];
      job.destination.table = collector[4];
  }
};

// BADFILE {string}
SqlVisitor.prototype.visitLogSpec = function(node, job) {
  job.setBadFile(node.getString(0));
};

// IN ONE TRANSACTION
SqlVisitor.prototype.visitOptAtomic = function(node, job) {
  job.inOneTransaction();
};

// IGNORE n LINES
SqlVisitor.prototype.visitOptIgnore = function(node, job) {
  job.setSkipRows(node.getNumber(0));
};

// DO n LINES
SqlVisitor.prototype.visitOptDolines = function(node, job) {
  job.setMaxRows(node.getNumber(0));
};

// COMMENTS STARTING BY {string}
SqlVisitor.prototype.visitOptComments = function(node, job) {
  job.setCommentStart(node.getString(0));
};

// LINES STARTING BY {string}
SqlVisitor.prototype.visitLineStartSpec = function(node, job) {
  job.setLineStart(node.getString(0));
};

// LINES TERMINATED BY {string}
SqlVisitor.prototype.visitLineEndSpec = function(node, job) {
  job.setLineEnd(node.getString(0));
};


/* Field Options */

// [OPTIONALLY] ENCLOSED BY string [ AND string ]
SqlVisitor.prototype.visitFieldQuoteOpt = function(node, job) {
  var collector = [];
  node.visitChildNodes(this, collector);
  if(collector[0].toUpperCase() === "OPTIONALLY") {
    collector.shift();
    job.setFieldQuoteOptional();
  }

  if(collector[4]) {
    job.setFieldQuoteStartAndEnd(collector[2], collector[4]);
  } else {
    job.setFieldQuoteStartAndEnd(collector[2], collector[2]);
  }
};

// ESCAPED BY {string}
SqlVisitor.prototype.visitFieldEscapeOpt = function(node, job) {
  job.setFieldQuoteEsc(node.getString(0));
};

// TERMINATED BY whitespace | {string}
SqlVisitor.prototype.visitFieldSepOpt = function(node, job) {
  var fieldSep = node.getString(0);
  if(fieldSep) {
    job.setFieldSeparator(fieldSep);
  } else {
    // getString(0) is null, so the token holds the name "whitespace"
    job.setFieldSeparatorToWhitespace();
  }
};


/* Column Definitions: */

// COLUMNS FROM HEADER
SqlVisitor.prototype.visitColumnsInHeader = function(node, job) {
  job.setColumnsInHeader();
};

// P.Series(StringOrName, P.Option(ColumnPosition))
/* Visit child nodes twice: first to fetch the column name on a collector,
   and then to pass the ColumnDefinition down to a ColumnPosition node.
*/
SqlVisitor.prototype.visitColumnDefn = function(node, job) {
  var name, defn, collector;
  collector = [];
  node.visitChildNodes(this, collector);
  name = collector[0];
  defn = job.destination.addColumnDefinition(name);
  node.visitChildNodes(this, defn);
};

SqlVisitor.prototype.visitColumnPosition = function(node, defn) {
  defn.startPos = node.getNumber(0);
  defn.endPos = node.getNumber(1);
};

SqlVisitor.prototype.visitInsertMode = function(node, job) {
  job.setInsertMode(node.getToken(0));
};

////////////////// EXPORTED FUNCTIONS 

exports.scan = function scanSourceFile(str) {
  P.setText(str);
  return scanner.tokenize(str);
};

exports.parse = function ParseLoaderString(tokens) {
  var tree = {};

  if(tokens.length) {
    P.begin(tokens);
    tree = P.eval(LoadDataStatement);
    P.done();
  }
  return tree;
};

exports.analyze = function(tree, loaderJob) {
  var sqlVisitor = new SqlVisitor();
  if(tree) {
    tree.visit(sqlVisitor, loaderJob);
  }
};

