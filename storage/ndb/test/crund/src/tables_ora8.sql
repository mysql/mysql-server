DROP TABLE B1;
DROP TABLE B0;
DROP TABLE A;

CREATE TABLE A (
        id          INT                 NOT NULL,
        cint        INT                 NULL,
        clong       DECIMAL(20)         NULL,
        cfloat      FLOAT               NULL,
        cdouble     DOUBLE PRECISION    NULL,
        cstring     VARCHAR(30)         NULL,
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE B0 (
        id          INT                 NOT NULL,
        cint        INT                 NULL,
        clong       DECIMAL(20)         NULL,
        cfloat      FLOAT               NULL,
        cdouble     DOUBLE PRECISION    NULL,
        cstring     VARCHAR(30)         NULL,
        a_id        INT                 NULL,
        CONSTRAINT PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT FK_B0_1 FOREIGN KEY (a_id) REFERENCES A (id)
);

CREATE TABLE B1 (
        id int NOT NULL,
        CONSTRAINT PK_B1_0 PRIMARY KEY (id)
);

CREATE INDEX I_B0_FK ON B0 (
	a_id
);

COMMIT WORK;

EXIT;
