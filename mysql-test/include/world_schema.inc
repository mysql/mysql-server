CREATE TABLE country (
  Code char(3) NOT NULL default '',
  Name char(52) NOT NULL default '',
  SurfaceArea float(10,2) NOT NULL default '0.00',
  Population int(11) NOT NULL default '0',
  Capital int(11) default NULL,
  PRIMARY KEY  (Code),
  UNIQUE INDEX (Name)
);
CREATE TABLE city (
  ID int(11) NOT NULL auto_increment,
  Name char(35) NOT NULL default '',
  Country char(3) NOT NULL default '',
  Population int(11) NOT NULL default '0',
  PRIMARY KEY  (ID),
  INDEX (Population),
  INDEX (Country) 
);
CREATE TABLE countrylanguage (
  Country char(3) NOT NULL default '',
  Language char(30) NOT NULL default '',
  Percentage float NOT NULL default '0.0',
  PRIMARY KEY  (Country, Language),
  INDEX (Percentage)
);
