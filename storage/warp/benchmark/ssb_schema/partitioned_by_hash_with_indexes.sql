DROP TABLE IF EXISTS customer;
CREATE TABLE IF NOT EXISTS customer
(
    C_CustomerKey int primary key,
    C_Name varchar(25),
    C_Address varchar(25),
    C_City varchar(10),
    C_Nation varchar(15),
    C_Region varchar(12),
    C_Phone varchar(15),
    C_MktSegment varchar(10),
    KEY(C_Name),
    KEY(C_City),
    KEY(C_Region),
    KEY(C_Phone),
    KEY(C_MktSegment)
);
 
DROP TABLE IF EXISTS part;
CREATE TABLE IF NOT EXISTS part
(
    P_PartKey int primary key,
    P_Name varchar(25),
    P_MFGR varchar(10),
    P_Category varchar(10),
    P_Brand varchar(15),
    P_Colour varchar(15),
    P_Type varchar(25),
    P_Size tinyint,
    P_Container char(10),
    key(P_Name),
    key(P_MFGR),
    key(P_Category),
    key(P_Brand)
);
 
DROP TABLE IF EXISTS supplier;
CREATE TABLE supplier
(
    S_SuppKey int primary key,
    S_Name char(25),
    S_Address varchar(25),
    S_City char(10),
    S_Nation char(15),
    S_Region char(12),
    S_Phone char(15),
    key(S_City),
    key(S_Name),
    key(S_Phone)
);
 
DROP TABLE IF EXISTS dim_date;
CREATE TABLE IF NOT EXISTS dim_date
(
    D_DateKey int primary key,
    D_Date char(18),
    D_DayOfWeek char(9),
    D_Month char(9),
    D_Year smallint,
    D_YearMonthNum int,
    D_YearMonth char(7),
    D_DayNumInWeek tinyint,
    D_DayNumInMonth tinyint,
    D_DayNumInYear smallint,
    D_MonthNumInYear tinyint,
    D_WeekNumInYear tinyint,
    D_SellingSeason char(12),
    D_LastDayInWeekFl tinyint,
    D_LastDayInMonthFl tinyint,
    D_HolidayFl tinyint,
    D_WeekDayFl tinyint,
    KEY(D_Year, D_DayNumInYear),
    KEY(D_YearMonth),
    KEY(D_WeekNumInYear, D_Year)
) ;
 
DROP TABLE IF EXISTS lineorder;
CREATE TABLE IF NOT EXISTS lineorder
(
    LO_OrderKey bigint not null,
    LO_LineNumber tinyint not null,
    LO_CustKey int not null,
    LO_PartKey int not null,
    LO_SuppKey int not null,
    LO_OrderDateKey int not null,
    LO_OrderPriority varchar(15),
    LO_ShipPriority char(1),
    LO_Quantity tinyint,
    LO_ExtendedPrice decimal,
    LO_OrdTotalPrice decimal,
    LO_Discount decimal,
    LO_Revenue decimal,
    LO_SupplyCost decimal,
    LO_Tax tinyint,
    LO_CommitDateKey int not null,
    LO_ShipMode varchar(10),
    key(LO_CustKey),
    key(LO_PartKey),
    key(LO_SuppKey),
    key(LO_OrderDateKey)
)
PARTITION BY HASH (LO_OrderDateKey) PARTITIONS 8
;
