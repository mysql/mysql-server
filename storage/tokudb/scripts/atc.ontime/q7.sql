SELECT t.Year, c1/c2 FROM (select Year,count(*)*1000 as c1 from ontime WHERE DepDelay>10 GROUP BY Year) t JOIN (select Year,count(*) as c2 from ontime GROUP BY Year) t2 ON (t.Year=t2.Year);
