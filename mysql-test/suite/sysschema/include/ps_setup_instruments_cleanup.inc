# Reset the performance_schema.setup_instruments table

UPDATE performance_schema.setup_instruments SET enabled = 'YES', timed = 'YES';
