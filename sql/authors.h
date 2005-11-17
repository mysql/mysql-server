/***************************************************************************
** Output from "SHOW AUTHORS"
** If you can update it, you get to be in it :)
** Dont be offended if your name is not in here, just add it!
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
  {NULL, NULL, NULL}
};
