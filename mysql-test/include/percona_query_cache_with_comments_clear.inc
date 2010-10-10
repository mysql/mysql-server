# Reset query cache variables.
flush query cache; # This crashed in some versions
flush query cache; # This crashed in some versions
reset query cache;
flush status;
