#
# Bug #27566937 SERVER STARTUP CRASH AFTER SHUTDOWN IF TABLES
# CREATED USING JUNK CHARACTER-DEBUG
#
/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
SET NAMES utf8mb4 ;
DROP DATABASE IF EXISTS `mytest`;
CREATE DATABASE `mytest`;
USE `mytest`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
SET character_set_client = utf8mb4 ;
CREATE TABLE `???]?yt???c??.?t??*?nb???%fk2???1??\?` (c1 int
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;
CREATE TABLE `mytest` (c1 int);
# restart
USE mytest;
SELECT COUNT(*) FROM mytest;
COUNT(*)
0
SELECT COUNT(*) FROM `???]?yt???c??.?t??*?nb???%fk2???1??\?`;
COUNT(*)
0
DROP DATABASE IF EXISTS `mytest`;
