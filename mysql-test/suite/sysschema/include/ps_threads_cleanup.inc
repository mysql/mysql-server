# Reset the performance_schema.setup_consumers table

UPDATE performance_schema.threads SET instrumented = 'YES';
