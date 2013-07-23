SELECT DestCityName, COUNT( DISTINCT OriginCityName) FROM ontime use index(year_5) WHERE Year BETWEEN 2006 and 2007 GROUP BY DestCityName ORDER BY 2 DESC LIMIT 10;
