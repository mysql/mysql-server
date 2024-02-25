# Reset the performance_schema.setup_consumers table

UPDATE performance_schema.setup_consumers SET enabled = 'YES';
