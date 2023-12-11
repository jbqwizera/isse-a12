/*
 * parse.c
 * 
 * Code that implements a parser for arithmetic expressions
 *
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "tokenize.h"


// Documented in .h file
AST Parse(CList tokens, char *errmsg, size_t errmsg_sz)
{
    *errmsg = 0;
    int redirect_in  = 0;
    int redirect_out = 0;
    AST ret = NULL;
    TokenType tt;
    while ((tt = TOK_next_type(tokens)) != TOK_END) {
        Token token = TOK_next(tokens);
        char* value = token.value? strdup(token.value): NULL;
        TOK_consume(tokens);

        if (tt == TOK_WORD || tt == TOK_QUOTED_WORD) {
            AST_append(&ret, AST_word(tt, 0, value));
        }
        else if (tt == TOK_LESSTHAN || tt == TOK_GREATERTHAN) {
            redirect_in  += tt == TOK_LESSTHAN;
            redirect_out += tt == TOK_GREATERTHAN;
            char* which = "LESSTHAN";
            if (tt == TOK_GREATERTHAN) which = "GREATERTHAN";

            if (!ret)
                snprintf(errmsg, errmsg_sz, "%s must have a left expression", which);

            if (ret && (AST_type(ret) == OP_LESSTHAN || AST_type(ret) == OP_GREATERTHAN))
                snprintf(errmsg, errmsg_sz, "Invalid %s left expression", which);

            if (!*errmsg && redirect_in > 1)
                snprintf(errmsg, errmsg_sz, "Pipeline may have at most one %s", which);

            if (!*errmsg && redirect_out > 1)
                snprintf(errmsg, errmsg_sz, "Pipeline may have at most one %s", which);

            TokenType next_tt = TOK_next_type(tokens);
            if (!*errmsg && next_tt != TOK_WORD)
                snprintf(errmsg, errmsg_sz, "%s expected TOK_WORD next, but got %s",
                    which, TT_to_str(next_tt));

            if (*errmsg) {
                AST_free(ret);
                return NULL;
            }

            value = strdup(TOK_next(tokens).value);
            TOK_consume(tokens);
            AST_append(&ret, AST_redirect(tt, AST_word(next_tt, 0, value)));
        }
        else if (tt == TOK_PIPE) {
            if (!ret)
                snprintf(errmsg, errmsg_sz,
                    "PIPE must have a left expression");

            TokenType next_tt = TOK_next_type(tokens);
            if (!*errmsg && next_tt != TOK_WORD && next_tt != TOK_QUOTED_WORD) {
                if (next_tt != TOK_END)
                    snprintf(errmsg, errmsg_sz,
                        "Invalid PIPE right expression");
                else
                    snprintf(errmsg, errmsg_sz,
                        "PIPE must have a right expression");
            }

            if (*errmsg) {
                AST_free(ret);
                return NULL;
            }
            value = strdup(TOK_next(tokens).value);
            TOK_consume(tokens);
            ret = AST_pipe(ret, AST_word(next_tt, 0, value));
        }
        else {
            snprintf(errmsg, errmsg_sz, "Unexpected token %s", TT_to_str(tt));
            AST_free(ret);
            return NULL;
        }
        free(value);
    }
    return ret;
}
