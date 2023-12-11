/*
 * tokenize.c
 * 
 * Functions to tokenize and manipulate lists of tokens
 *
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "clist.h"
#include "tokenize.h"
#include "token.h"

// Documented in .h file
const char* TT_to_str(TokenType tt)
{
    switch(tt) {
        case TOK_WORD:
            return "WORD";
        case TOK_QUOTED_WORD:
            return "QUOTED_WORD";
        case TOK_LESSTHAN:
            return "LESSTHAN";
        case TOK_GREATERTHAN:
            return "GREATERTHAN";
        case TOK_PIPE:
            return "PIPE";
        case TOK_END:
            return "(end)";
    }
    __builtin_unreachable();
}

static bool isescape(char ch)
{
    return ch == 'n' || ch == 'r' || ch == 't' || ch == ' ' ||
           ch == '"' || ch == '>' || ch == '<' || ch == '|' ||
           ch == '\\';
}

/*
static int globber_append(CList tokens, const char* word)
{
    glob_t pglob;
    int options = GLOB_TILDE_CHECK | GLOB_NOCHECK;
    int globexit = glob(word, options, NULL, &pglob);
    if (globexit) {
        globfree(&pglob);
        return globexit;
    }

    for (int i = 0; i < pglob.gl_pathc; i++) {
        char* path = pglob.gl_pathv[i];
        CL_append(tokens, TOK_nnew(TOK_WORD, path, strlen(path)));
    }
    globfree(&pglob);

    return 0;
}
*/

// Documented in .h file
CList TOK_tokenize_input(const char* input, char* errmsg, size_t errmsg_sz)
{
    *errmsg = 0;
    CList tokens = CL_new();
    const char* start = NULL;
    size_t len;

    while (*input) {
        char ch = *input;
        if (ch == '"') {
            // end of WORD: unescaped double quote
            if (start) {
                len = input - start;
                CL_append(tokens, TOK_nnew(TOK_WORD, start, len));
            }
            start = input++;
            while (*input && *input != '"') {
                if ((ch = *input++) == '\\' && !isescape(*input)) {
                    snprintf(errmsg, errmsg_sz,
                        "Illegal escape character '?%c'", *input);
                    CL_free(tokens);
                    return NULL;
                }
            }

            if (!*input) {
                snprintf(errmsg, errmsg_sz, "Unterminated quote");
                CL_free(tokens);
                return NULL;
            }
            len = input - start + 1;
            CL_append(tokens, TOK_nnew(TOK_QUOTED_WORD, start, len));
            start = NULL;

        } else if (ch == '<' || ch == '>' || ch == '|' || isspace(ch)) {
            // end of WORD: unescaped token or whitespace

            if (start) {
                len = input - start;
                CL_append(tokens, TOK_nnew(TOK_WORD, start, len));
            }
            if (ch == '<') CL_append(tokens, (Token){TOK_LESSTHAN});
            if (ch == '>') CL_append(tokens, (Token){TOK_GREATERTHAN});
            if (ch == '|') CL_append(tokens, (Token){TOK_PIPE});

            start = NULL;

        } else {
            if (!start) start = input;
            if (ch == '\\' && !isescape(*++input)) {
                snprintf(errmsg, errmsg_sz,
                    "Illegal escape character '?%c'", *input);
                CL_free(tokens);
                return NULL;
            }
        }

        if (*input) ++input;

        if (!*input && start) {
            // end of WORD: end of the input line
            len = input - start + 1;
            CL_append(tokens, TOK_nnew(TOK_WORD, start, len));
            start = NULL;
        }
    }

    return tokens;
}


// Documented in .h file
TokenType TOK_next_type(CList tokens)
{
    if (CL_length(tokens))
        return TOK_next(tokens).type;
    return TOK_END;
}


// Documented in .h file
Token TOK_next(CList tokens)
{   return CL_nth(tokens, 0); }


// Documented in .h file
void TOK_consume(CList tokens)
{   CL_remove(tokens, 0); }

// TOK_print helper function to pass as a CL_foreach callback
static void TOK_print_token(int pos, Token token, void* cb_data)
{
    TokenType tt = token.type; 
    if (tt == TOK_WORD || tt == TOK_QUOTED_WORD)
        printf("%s\n", token.value);
    else
        printf("%s\n", TT_to_str(token.type));
} 

// Documented in .h file
void TOK_print(CList tokens)
{   CL_foreach(tokens, TOK_print_token, NULL); }


// Documented in .h file
Token TOK_nnew(TokenType tt, const char* value, size_t len)
{
    Token token;
    token.type = tt;
    if (tt == TOK_WORD || tt == TOK_QUOTED_WORD) {
        assert(value);
        if (!len) len = strlen(value);
        token.value = strndup(value, len);
    }
    return token;
}

// Documented in .h file
Token TOK_new(TokenType tt, const char* value)
{   return TOK_nnew(tt, value, strlen(value)); }
