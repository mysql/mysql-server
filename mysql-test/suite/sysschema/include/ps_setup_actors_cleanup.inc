# Reset the performance_schema.setup_actors table

TRUNCATE TABLE performance_schema.setup_actors;

INSERT INTO performance_schema.setup_actors VALUES ('%', '%', '%', 'YES', 'YES');
