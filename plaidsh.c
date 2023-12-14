#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "clist.h"
#include "tokenize.h"
#include "parse.h"


#define KNRM    "\x1B[0m"
#define KBLD    "\x1B[1m"
#define KRED    "\x1B[31m"
#define PROMPT  "#? "

int main(int argc, char* argv[])
{
    CList tokens = NULL;
    size_t buffer_sz = 128;
    char buffer[buffer_sz];
    char* input = NULL;
    bool time_to_quit = false;
    AST pipeline = NULL;

    printf("Welcome to Plaid Shell!\n");

    while (!time_to_quit) {
        input = readline(KRED KBLD PROMPT KNRM);

        if (input == NULL || strcasecmp(input, "quit") == 0) {
            time_to_quit = true;
            goto loop_end;
        }

        if (*input == 0)
            goto loop_end;

        add_history(input);

        tokens = TOK_tokenize_input(input, buffer, buffer_sz);

        if (tokens == NULL) {
            fprintf(stderr, "%s\n", buffer);
            goto loop_end;
        }

        if (CL_length(tokens) == 0)
            goto loop_end;

        // uncomment for more debug info
        // TOK_print(tokens);

        pipeline = Parse(tokens, buffer, buffer_sz);

        if (pipeline == NULL) {
            fprintf(stderr, "%s\n", buffer);
            goto loop_end;
        }

        AST_pipeline2str(pipeline, buffer, buffer_sz);

        AST_execute(pipeline, buffer, buffer_sz);

loop_end:
        free(input);
        input = NULL;
        CL_free(tokens);
        tokens = NULL;
        AST_free(pipeline);
        pipeline = NULL;
    }

    return 0;
}
