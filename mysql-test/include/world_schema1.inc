CREATE TABLE Country (
  Code char(3) NOT NULL default '',
  Name char(52) NOT NULL default '',
  SurfaceArea float(10,2) NOT NULL default '0.00',
  Population int(11) NOT NULL default '0',
  Capital int(11) default NULL
);
CREATE TABLE City (
  ID int(11) NOT NULL,
  Name char(35) NOT NULL default '',
  Country char(3) NOT NULL default '',
  Population int(11) NOT NULL default '0'
);
CREATE TABLE CountryLanguage (
  Country char(3) NOT NULL default '',
  Language char(30) NOT NULL default '',
  Percentage float(3,1) NOT NULL default '0.0'
);
