--disable_query_log
set @end = now();
let $_elapsed = `select TIMEDIFF(@end, @start)`;
--disable_query_log
echo Elapsed time: $_elapsed;

