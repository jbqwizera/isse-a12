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
#include <glob.h>

#include "parse.h"
#include "tokenize.h"


/*
 * Expands a string value of a WORD token using system glob
 * and appends the results to the pipeline
 *
 * Parameters:
 *  pipelinep   The pointer to the pipeline to append to
 *  value       The value to expand
 *
 * Returns:     Glob error on unsuccessful glob call
 */
static int glob_append(AST* pipelinep, const char* value)
{
    glob_t pglob;
    int options = GLOB_TILDE_CHECK | GLOB_NOCHECK;
    int globexit = glob(value, options, NULL, &pglob);
    if (globexit) {
        globfree(&pglob);
        return globexit;
    }

    for (int i = 0; i < pglob.gl_pathc; i++)
        AST_append(pipelinep, AST_word(WORD, 0, pglob.gl_pathv[i]));

    globfree(&pglob);

    return 0;
}


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

        if (tt == TOK_QUOTED_WORD) AST_append(&ret, AST_word(tt, 0, value));
        else if (tt == TOK_WORD) {
            int globexit = glob_append(&ret, value);
            if (globexit) {
                snprintf(errmsg, errmsg_sz, "Glob encountered an error");
                AST_free(ret);
                free(value);
                return NULL;
            }
        }
        else if (tt == TOK_LESSTHAN || tt == TOK_GREATERTHAN) {
            redirect_in  += tt == TOK_LESSTHAN;
            redirect_out += tt == TOK_GREATERTHAN;

            if (!ret)
                snprintf(errmsg, errmsg_sz, "No command specified");

            if (ret && (AST_type(ret) == OP_LESSTHAN || AST_type(ret) == OP_GREATERTHAN))
                snprintf(errmsg, errmsg_sz, "No command specified");

            if (!*errmsg && (redirect_in > 1 || redirect_out > 1))
                snprintf(errmsg, errmsg_sz, "Multiple redirection");

            TokenType next_tt = TOK_next_type(tokens);
            if (!*errmsg && next_tt != TOK_WORD)
                snprintf(errmsg, errmsg_sz, "Expect filename after redirection");

            if (*errmsg) {
                AST_free(ret);
                free(value);
                return NULL;
            }

            value = strdup(TOK_next(tokens).value);
            TOK_consume(tokens);
            AST tempfile = NULL;
            int globexit = glob_append(&tempfile, value);
            if (globexit) {
                snprintf(errmsg, errmsg_sz, "Glob encountered an error");
                AST_free(tempfile);
                AST_free(ret);
                free(value);
                return NULL;
            }
            AST_append(&ret, AST_redirect(tt, tempfile));
        }
        else if (tt == TOK_PIPE) {
            if (!ret)
                snprintf(errmsg, errmsg_sz, "No command specified");

            TokenType next_tt = TOK_next_type(tokens);
            if (!*errmsg && next_tt != TOK_WORD && next_tt != TOK_QUOTED_WORD)
                snprintf(errmsg, errmsg_sz, "No command specified");

            if (*errmsg) {
                AST_free(ret);
                free(value);
                return NULL;
            }
            value = strdup(TOK_next(tokens).value);
            TOK_consume(tokens);
            AST tempcmd = AST_word(next_tt, 0, value);
            if (next_tt == TOK_WORD) {
                AST_free(tempcmd);
                tempcmd = NULL;
                int globexit = glob_append(&tempcmd, value);
                if (globexit) {
                    snprintf(errmsg, errmsg_sz, "Glob encountered an error");
                    AST_free(tempcmd);
                    AST_free(ret);
                    free(value);
                    return NULL;
                }
            }
            ret = AST_pipe(ret, tempcmd);
        }
        else {
            snprintf(errmsg, errmsg_sz, "Unexpected token %s", TT_to_str(tt));
            AST_free(ret);
            free(value);
            return NULL;
        }
        free(value);
    }
    return ret;
}
