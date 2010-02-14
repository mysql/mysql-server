CONNECT 'jdbc:derby:crunddb;create=true';

-- DROP TABLE IF EXISTS not supported yet by derby
DROP TABLE b1;
DROP TABLE b0;
DROP TABLE a;

CREATE TABLE a (
        id          INT                 NOT NULL,
        cint        INT,
        clong       DECIMAL(20),
        cfloat      FLOAT,
        cdouble     DOUBLE PRECISION,
        CONSTRAINT  PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE b0 (
        id          INT                 NOT NULL,
        cint        INT,
        clong       DECIMAL(20),
        cfloat      FLOAT,
        cdouble     DOUBLE PRECISION,
        cvarbinary_def  BLOB(202),
        cvarchar_def    VARCHAR(202),
        cblob_def       BLOB(202),
        ctext_def       VARCHAR(202),
        a_id        INT,
        CONSTRAINT  PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT  FK_B0_1 FOREIGN KEY (a_id) REFERENCES a (id)
);

CREATE TABLE b1 (
        id int NOT NULL,
        CONSTRAINT PK_B1_0 PRIMARY KEY (id)
);

-- derby already creates an index on B0.a_id

-- seems that these datatypes are not supported (parse error with ij):
--        cvarbinary_def  VARBINARY(202),
--        ctext_def       TEXT(202),

EXIT;
