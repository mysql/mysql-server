DROP TABLE IF EXISTS customer;
CREATE TABLE IF NOT EXISTS customer
(
    C_CustomerKey int NOT NULL ,
    C_Name varchar(25) NOT NULL,
    C_Address varchar(25) NOT NULL,
    C_City varchar(10) NOT NULL,
    C_Nation varchar(15) NOT NULL,
    C_Region varchar(12) NOT NULL,
    C_Phone varchar(15) NOT NULL,
    C_MktSegment varchar(10) NOT NULL,
    KEY(C_CustomerKey),
    KEY(C_Name),
    KEY(C_City),
    KEY(C_Region),
    KEY(C_Phone),
    KEY(C_MktSegment)
) ;
 
DROP TABLE IF EXISTS part;
CREATE TABLE IF NOT EXISTS part
(
    P_PartKey int not null,
    P_Name varchar(25) NOT NULL,
    P_MFGR varchar(10) NOT NULL,
    P_Category varchar(10) NOT NULL,
    P_Brand varchar(15) NOT NULL,
    P_Colour varchar(15) NOT NULL,
    P_Type varchar(25) NOT NULL,
    P_Size tinyint NOT NULL,
    P_Container char(10) NOT NULL,
    key(P_PartKey),
    key(P_Name),
    key(P_MFGR),
    key(P_Category),
    key(P_Brand)
) ;
 
DROP TABLE IF EXISTS supplier;
CREATE TABLE supplier
(
    S_SuppKey int not null,
    S_Name char(25) NOT NULL,
    S_Address varchar(25) NOT NULL,
    S_City char(10) NOT NULL,
    S_Nation char(15) NOT NULL,
    S_Region char(12) NOT NULL,
    S_Phone char(15) NOT NULL,
    key(S_SuppKey),
    key(S_City),
    key(S_Name),
    key(S_Phone),
    key(S_Region)
) ;
 
DROP TABLE IF EXISTS dim_date;
CREATE TABLE IF NOT EXISTS dim_date
(
    D_DateKey int not null,
    D_Date char(18) NOT NULL,
    D_DayOfWeek char(9) NOT NULL,
    D_Month char(9) NOT NULL,
    D_Year smallint NOT NULL,
    D_YearMonthNum int NOT NULL,
    D_YearMonth char(7) NOT NULL,
    D_DayNumInWeek tinyint NOT NULL,
    D_DayNumInMonth tinyint NOT NULL,
    D_DayNumInYear smallint NOT NULL,
    D_MonthNumInYear tinyint NOT NULL,
    D_WeekNumInYear tinyint NOT NULL,
    D_SellingSeason char(12) NOT NULL,
    D_LastDayInWeekFl tinyint NOT NULL,
    D_LastDayInMonthFl tinyint NOT NULL,
    D_HolidayFl tinyint NOT NULL,
    D_WeekDayFl tinyint NOT NULL, 
    KEY(D_DateKey),
    KEY(D_Year), 
    KEY(D_YearMonth), 
    KEY(D_WeekNumInYear, D_Year)
) 
;
 
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
    LO_Quantity tinyint not null,
    LO_ExtendedPrice bigint,
    LO_OrdTotalPrice bigint,
    LO_Discount bigint not null,
    LO_Revenue bigint not null,
    LO_SupplyCost bigint,
    LO_Tax tinyint,
    LO_CommitDateKey int not null,
    LO_ShipMode varchar(10),
    KEY(LO_CustKey),
    KEY(LO_SuppKey),
    KEY(LO_OrderDateKey),
    KEY(LO_PartKey)
)
;
