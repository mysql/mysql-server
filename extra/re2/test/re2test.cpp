/*
 *  Test program for Google's RE2 regular expression library.
*/

#include <stdio.h>
#include <string>

#include "re2/re2.h"

/* sample strings */
const char *strings[] = {
    "abc",
    "ábc",
    "abc-123",
    "def",
    "def-456",
    NULL
};

class Unistring : std::string
{
public:
    Unistring(const char *cstring)
    {
        append(cstring);
    }
    
    
    const char *to_cmdline()
    {
        cmd_line = "CMD-";
        cmd_line.append(data(), length());
        return (cmd_line.c_str());
    }
    
private:
    std::string cmd_line;
};
    


int main(int argc, char *argv[])
{
    int i;
    
    if (argc > 1) {
        char *regex_text = argv[1];

        RE2 pattern(regex_text);

        printf("Matching strings:\n");
        for (i = 0; strings[i]; i++) {
            if (RE2::PartialMatch(strings[i], pattern))
            {
                printf("%s\n", strings[i]);
            }
        }
    }
    else 
    {
        /* print all strings */
        printf("All strings:\n");
        for (i = 0; strings[i]; i++) {
            printf("'%s'  %d  %s\n", strings[i], strlen(strings[i]), 
                   Unistring(strings[i]).to_cmdline());
        }
    }
    
}
