# Q8: As final I tested most popular destination in sense count of direct connected cities for different diapason of years.
SELECT DestCityName, COUNT( OriginCityName) FROM ontime WHERE Year BETWEEN 2006 and 2007 GROUP BY DestCityName ORDER BY 2 DESC LIMIT 10;
