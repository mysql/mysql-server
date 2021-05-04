create schema xtest default charset 'utf8mb4';
RUN use xtest

0 rows affected
RUN CREATE TABLE `city` (
  `ID` int(11) NOT NULL auto_increment,
  `Name` char(35) NOT NULL default '',
  `CountryCode` char(3) NOT NULL default '',
  `District` char(20) NOT NULL default '',
  `Population` int(11) NOT NULL default '0',
  PRIMARY KEY  (`ID`)
)

0 rows affected
RUN INSERT INTO `city` VALUES (1,'Kabul','AFG','Kabol',1780000)

1 rows affected
last insert id: 1
RUN INSERT INTO `city` VALUES (2,'Qandahar','AFG','Qandahar',237500)

1 rows affected
last insert id: 2
RUN INSERT INTO `city` VALUES (3,'Herat','AFG','Herat',186800)

1 rows affected
last insert id: 3
RUN INSERT INTO `city` VALUES (4,'Mazar-e-Sharif','AFG','Balkh',127800)

1 rows affected
last insert id: 4
RUN INSERT INTO `city` VALUES (5,'Amsterdam','NLD','Noord-Holland',731200)

1 rows affected
last insert id: 5
RUN INSERT INTO `city` VALUES (6,'Rotterdam','NLD','Zuid-Holland',593321)

1 rows affected
last insert id: 6
RUN INSERT INTO `city` VALUES (7,'Haag','NLD','Zuid-Holland',440900)

1 rows affected
last insert id: 7
RUN INSERT INTO `city` VALUES (8,'Utrecht','NLD','Utrecht',234323)

1 rows affected
last insert id: 8
RUN INSERT INTO `city` VALUES (9,'Eindhoven','NLD','Noord-Brabant',201843)

1 rows affected
last insert id: 9
RUN INSERT INTO `city` VALUES (10,'Tilburg','NLD','Noord-Brabant',193238)

1 rows affected
last insert id: 10
RUN INSERT INTO `city` VALUES (11,'Groningen','NLD','Groningen',172701)

1 rows affected
last insert id: 11
RUN INSERT INTO `city` VALUES (12,'Breda','NLD','Noord-Brabant',160398)

1 rows affected
last insert id: 12
RUN INSERT INTO `city` VALUES (13,'Apeldoorn','NLD','Gelderland',153491)

1 rows affected
last insert id: 13
RUN INSERT INTO `city` VALUES (14,'Nijmegen','NLD','Gelderland',152463)

1 rows affected
last insert id: 14
RUN INSERT INTO `city` VALUES (15,'Enschede','NLD','Overijssel',149544)

1 rows affected
last insert id: 15
RUN INSERT INTO `city` VALUES (16,'Haarlem','NLD','Noord-Holland',148772)

1 rows affected
last insert id: 16
send Mysqlx.Crud.Find {
  collection {
    name: "city"
    schema: "xtest"
  }
  data_model: DOCUMENT
}

Mysqlx.Error {
  severity: ERROR
  code: 5117
  msg: "Unknown column \'doc\' in collection"
  sql_state: "HY000"
}

Mysqlx.Ok {
  msg: "bye!"
}
ok
