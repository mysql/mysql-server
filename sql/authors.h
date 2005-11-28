/***************************************************************************
** Output from "SHOW AUTHORS"
** If you can update it, you get to be in it :)
** Dont be offended if your name is not in here, just add it!
** IMPORTANT: Names should be added in alphabetical order
***************************************************************************/

struct show_table_authors_st {
  const char *name;
  const char *location;
  const char *comment;
};

struct show_table_authors_st show_table_authors[]= {
  { "Brian \"Krow\" Aker", "Seattle, WA. USA",
    "Architecture, archive, federated, buncha of little stuff :)" },
  { "David Axmark", "Uppsala, Sweden", "Small stuff long time ago, Monty ripped it out!"},
  { "Omer BarNir", "Sunnyvale, CA. USA", "Testing (sometimes) and general QA stuff"},
  { "Nikolay Grishakin", "Austin, TX. USA", "Testing - Server"},
  { "Serge Kozlov", "Velikie Luki, Russia", "Testing - Cluster"},
  { "Matthias Leich", "Berlin, Germany", "Testing - Server"},
  { "Jonathan (Jeb) Miller", "Kyle, TX. USA", "Testing - Cluster, Replication"},
  { "Carsten Segieth (Pino)", "Fredersdorf, Germany", "Testing - Server"},
  { "Punita Srivastava", "Austin, TX. USA", "Testing - Merlin"},
  { "Alexey Stroganov (Ranger)", "Lugansk, Ukraine", "Testing - Benchmarks"},
  { "Oleksandr Byelkin", "Lugansk, Ukraine", "Query Cache (4.0), Subqueries (4.1), Views (5.0)"},
  { "Dmitri Lenev", "Moscow, Russia", "Time zones support (4.1), Triggers (5.0)"},
  { "Konstantin Osipov", "Moscow, Russia", "Prepared statements (4.1), Cursors (5.0)"},
  { "Petr Chardin", "Moscow, Russia", "Instance Manager (5.0)" },
  { "Sergey Vojtovich", "Izhevsk, Russia", "Plugins infrastructure (5.1)" },
  { "Alexander Nozdrin", "Moscow, Russia", "Bugfixing (Stored Procedures, 5.0)" },
  { "Per-Erik Martin", "Uppsala, Sweden", "Stored Procedures (5.0)" },
  { "Kent Boortz", "Orebro, Sweden", "Test platform, and general build stuff" },
  {NULL, NULL, NULL}
};
