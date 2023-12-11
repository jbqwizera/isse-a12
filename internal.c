#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define  AUTHOR "Jean Baptiste Kwizera"


int main(int argc, char* argv[])
{
    if (argc < 2) exit(1);

    size_t buffer_sz = 256;
    char buffer[buffer_sz];
    if      (!strcmp(argv[1], "quit"))   exit(0);
    else if (!strcmp(argv[1], "exit"))   exit(0);
    else if (!strcmp(argv[1], "author")) printf("%s\n", AUTHOR);
    else if (!strcmp(argv[1], "pwd")) {
        getcwd(buffer, buffer_sz);
        printf("%s\n", buffer);
    }
    else if (!strcmp(argv[1], "cd")) {
        argc > 2? chdir(argv[2]): chdir(getenv("HOME"));
    }
}
