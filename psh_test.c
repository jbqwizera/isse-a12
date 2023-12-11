/*
 * psh_test.c
 * 
 * Tests for tokenization and pipeline functions
 *
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "token.h"
#include "tokenize.h"
#include "pipeline.h"
#include "parse.h"


// Checks that value is true; if not, prints a failure message and
// returns 0 from this function
#define test_assert(value) {                                            \
    if (!(value)) {                                                     \
      printf("FAIL %s[%d]: %s\n", __FUNCTION__, __LINE__, #value);      \
      goto test_error;                                                  \
    }                                                                   \
  }


/*
 * Returns true if tok1 and tok2 compare equally, false otherwise
 */
static bool test_tok_eq(Token tok1, Token tok2)
{
    if (tok1.type != tok2.type)
        return false;

    if (tok1.type == TOK_WORD || tok1.type == TOK_QUOTED_WORD)
        return !strcmp(tok1.value, tok2.value);

    return true;
}


/*
 * Tests the TOK_tokenize_input function
 *
 * Returns: 1 if all tests pass, 0 otherwise
 */
int test_tok_tokenize_input()
{
    typedef struct {
        const char* input;
        const char* errmsg;
        const Token exp_tokens[20];
    } test_matrix_t;

    test_matrix_t tests[] =
    {
        // tests
        {"", "", {{TOK_END}}},
        // escaped n
        {"a\\nb", "", {TOK_new(TOK_WORD, "a\\nb"), {TOK_END}}},
        // escaped r
        {"a\\rb", "", {TOK_new(TOK_WORD, "a\\rb"), {TOK_END}}},
        // escaped t
        {"a\\tb", "", {TOK_new(TOK_WORD, "a\\tb"), {TOK_END}}},
        // escaped "
        {"a\\\"b", "", {TOK_new(TOK_WORD, "a\\\"b"), {TOK_END}}},
        // escaped '\\'
        {"a\\\\b", "", {TOK_new(TOK_WORD, "a\\\\b"), {TOK_END}}},
        // escaped [space]
        {"a\\ b", "", {TOK_new(TOK_WORD, "a\\ b"), {TOK_END}}},
        // escaped |, <, or >
        {"a\\|b", "", {TOK_new(TOK_WORD, "a\\|b"), {TOK_END}}},
        {"a\\<b", "", {TOK_new(TOK_WORD, "a\\<b"), {TOK_END}}},
        {"a\\>b", "", {TOK_new(TOK_WORD, "a\\>b"), {TOK_END}}},

        // multiple escaped characters
        {"one\\ file.txt\\n\\ttwo\\ file.txt\\n", "",
            {TOK_new(TOK_WORD, "one\\ file.txt\\n\\ttwo\\ file.txt\\n"),
            {TOK_END}}},

        // consecutive escape characters
        {"\\n\\r\\t\\\"\\\\\\ \\|\\<\\>", "",
            {TOK_new(TOK_WORD, "\\n\\r\\t\\\"\\\\\\ \\|\\<\\>"),
            {TOK_END}}},

        // escape within QUOTED_WORD
        {"\"Believe that you can\\n\\tand you're halfway there\"", "",
            {TOK_new(TOK_QUOTED_WORD,
                "\"Believe that you can\\n\\tand you're halfway there\""),
            {TOK_END}}},

        // WORD + QUOTED_WORD with escape chars
        {"\"To be, or not to be,\" that is the question!", "",
            {TOK_new(TOK_QUOTED_WORD, "\"To be, or not to be,\""),
            TOK_new(TOK_WORD, "that"), TOK_new(TOK_WORD, "is"),
            TOK_new(TOK_WORD, "the"), TOK_new(TOK_WORD, "question!"),
            {TOK_END}}},

        // escaped and unescaped WORD delimiters
        {"this\\ is\\ a\\ word more\\nword:\\ QUOTED_WORD\\t\"less is more\"",
            "", {TOK_new(TOK_WORD, "this\\ is\\ a\\ word"),
            TOK_new(TOK_WORD, "more\\nword:\\ QUOTED_WORD\\t"),
            TOK_new(TOK_QUOTED_WORD, "\"less is more\""), {TOK_END}}},

        // from writeup examples
        {"ls", "", {TOK_new(TOK_WORD, "ls"), {TOK_END}}},
        {"ls --color", "", {TOK_new(TOK_WORD, "ls"),
            TOK_new(TOK_WORD, "--color"), {TOK_END}}},
        {"cd Plaid\\ Shell\\ Playground", "", {TOK_new(TOK_WORD, "cd"),
            TOK_new(TOK_WORD, "Plaid\\ Shell\\ Playground"), {TOK_END}}},
        {"ls *.txt", "", {TOK_new(TOK_WORD, "ls"),
            TOK_new(TOK_WORD, "*.txt"), {TOK_END}}},
        {"echo $PATH", "", {TOK_new(TOK_WORD, "echo"),
            TOK_new(TOK_WORD, "$PATH"), {TOK_END}}},
        {"author", "", {TOK_new(TOK_WORD, "author"), {TOK_END}}},
        {"author | sed -e \"s/^/Written by /\"", "",
            {TOK_new(TOK_WORD, "author"), {TOK_PIPE},
            TOK_new(TOK_WORD, "sed"), TOK_new(TOK_WORD, "-e"),
            TOK_new(TOK_QUOTED_WORD, "\"s/^/Written by /\""), {TOK_END}}},
        {"grep Happy *.txt", "", {TOK_new(TOK_WORD, "grep"),
            TOK_new(TOK_WORD, "Happy"), TOK_new(TOK_WORD, "*.txt"),
            {TOK_END}}},
        {"cat \"best sitcoms.txt\" | grep Seinfield", "",
            {TOK_new(TOK_WORD, "cat"),
            TOK_new(TOK_QUOTED_WORD, "\"best sitcoms.txt\""), {TOK_PIPE},
            TOK_new(TOK_WORD, "grep"), TOK_new(TOK_WORD, "Seinfield"),
            {TOK_END}}},
        {"sed -ne \"s/The Simpsons/I Love Lucy/p\" < best\\ sitcoms.txt > output", "",
            {TOK_new(TOK_WORD, "sed"), TOK_new(TOK_WORD, "-ne"),
            TOK_new(TOK_QUOTED_WORD, "\"s/The Simpsons/I Love Lucy/p\""),
            {TOK_LESSTHAN},
            TOK_new(TOK_WORD, "best\\ sitcoms.txt"), {TOK_GREATERTHAN},
            TOK_new(TOK_WORD, "output"), {TOK_END}}},
        {"ls -l", "", {TOK_new(TOK_WORD, "ls"),
            TOK_new(TOK_WORD, "-l"), {TOK_END}}},
        {"cat \"best sitcoms.txt\"|grep Seinfield|wc -l", "",
            {TOK_new(TOK_WORD, "cat"),
            TOK_new(TOK_QUOTED_WORD, "\"best sitcoms.txt\""),
            {TOK_PIPE}, TOK_new(TOK_WORD, "grep"),
            TOK_new(TOK_WORD, "Seinfield"), {TOK_PIPE},
            TOK_new(TOK_WORD, "wc"), TOK_new(TOK_WORD, "-l"), {TOK_END}}},
        {"this is not a command", "", {TOK_new(TOK_WORD, "this"),
            TOK_new(TOK_WORD, "is"), TOK_new(TOK_WORD, "not"),
            TOK_new(TOK_WORD, "a"), TOK_new(TOK_WORD, "command"),
            {TOK_END}}},
        {"cd ~", "", {TOK_new(TOK_WORD, "cd"),
            TOK_new(TOK_WORD, "~"), {TOK_END}}},
        {"echo \"Operator could you help me place this call?\"", "",
            {TOK_new(TOK_WORD, "echo"),
            TOK_new(TOK_QUOTED_WORD,
                "\"Operator could you help me place this call?\""),
            {TOK_END}}},
        {"seq 10 | wc\"-l\"", "", {TOK_new(TOK_WORD, "seq"),
            TOK_new(TOK_WORD, "10"), {TOK_PIPE}, TOK_new(TOK_WORD, "wc"),
            TOK_new(TOK_QUOTED_WORD, "\"-l\""), {TOK_END}}},
        {"\x1b[A\x1b[A'", "",
            {TOK_new(TOK_WORD, "\x1b[A\x1b[A'"), {TOK_END}}},
        
        // tokenizer errors
        {"echo \\c", "Illegal escape character '?c", {{TOK_END}}},
        {"echo \"\\b\"", "Illegal escape character '?b", {{TOK_END}}},
        {"echo \"hi", "Unterminated quote", {{TOK_END}}},
    };


    const int num_tests = sizeof(tests) / sizeof(test_matrix_t);
    const int errmsg_sz = 128;
    char errmsg[errmsg_sz];

    CList list;

    for (int i = 0; i < num_tests; i++) {
        list = TOK_tokenize_input(tests[i].input, errmsg, errmsg_sz);
        test_assert(!strcmp(errmsg, tests[i].errmsg));
        for (int t = 0; tests[i].exp_tokens[t].type != TOK_END; t++) {
            test_assert(test_tok_eq(TOK_next(list), tests[i].exp_tokens[t]));
            free((void *) tests[i].exp_tokens[t].value);
            TOK_consume(list);
        }
        test_assert((!list || TOK_next(list).type == TOK_END));
        CL_free(list);
        list = NULL;
    }
    return 1;

test_error:
    CL_free(list);
    for (int i = 0; i < num_tests; i++)
        for (int t = 0; tests[i].exp_tokens[t].type != TOK_END; t++)
            free((void *) tests[i].exp_tokens[t].value);

    return 0;
}


/*
 * Tests the AST_word, AST_redirect, AST_pipe, ET_pipeline2string,
 * AST_count, and AST_append functions.
 *
 * Returns: 1 if all tests pass, 0 otherwise
 */
int test_ast_pipeline()
{
    AST pipeline = NULL;
    char buffer[128];
    size_t len;
    int count;

    // ls
    pipeline = AST_word(WORD, 0, "ls");
    len = AST_pipeline2str(pipeline, buffer, sizeof(buffer));
    count = AST_count(pipeline);
    test_assert(count == 1);
    test_assert(!strcmp(buffer, "ls"));
    test_assert(strlen(buffer) == len);

    // ls "$HOME"
    AST_append(&pipeline, AST_word(QUOTED_WORD, 0, "\"$HOME\""));
    len = AST_pipeline2str(pipeline, buffer, sizeof(buffer));
    count = AST_count(pipeline);
    test_assert(count == 2);
    test_assert(!strcmp(buffer, "ls \"$HOME\""))
    test_assert(strlen(buffer) == len);

    // ls "$HOME" | grep ^t
    pipeline = AST_pipe(pipeline,
            AST_word(WORD, AST_word(WORD, 0, "^t"), "grep"));
    len = AST_pipeline2str(pipeline, buffer, sizeof(buffer));
    count = AST_count(pipeline);
    test_assert(count == 5);
    test_assert(!strcmp(buffer, "ls \"$HOME\" | grep ^t"));
    test_assert(strlen(buffer) == len);

    AST_free(pipeline);

    // echo "all happy families are alike" > karenina.txt
    pipeline = AST_word(WORD, 0, "echo");
    AST_append(&pipeline,
            AST_word(QUOTED_WORD, 0, "\"all happy families are alike\""));
    AST_append(&pipeline,
            AST_redirect(OP_GREATERTHAN,
                AST_word(WORD, 0, "karenina.txt")));

    len = AST_pipeline2str(pipeline, buffer, sizeof(buffer));
    count = AST_count(pipeline);
    test_assert(count == 4);
    test_assert(!strcmp(buffer, "echo \"all happy families are alike\" > karenina.txt"));
    test_assert(strlen(buffer) == len);

    AST_free(pipeline);

    return 1;

test_error:
    AST_free(pipeline);
    return 0;
}


int test_parse_once(const Token token_arr[], const char* exp_str, int exp_count)
{
    CList tokens = NULL;
    AST pipeline = NULL;
    const int buffer_sz = 256;
    char buffer[buffer_sz];

    tokens = CL_new();

    for (int i = 0; token_arr[i].type != TOK_END; i++)
        CL_append(tokens, token_arr[i]);

    pipeline = Parse(tokens, buffer, buffer_sz);
    test_assert(exp_count == AST_count(pipeline));

    AST_pipeline2str(pipeline, buffer, buffer_sz);
    test_assert(!strcmp(exp_str, buffer));

    CL_free(tokens);
    AST_free(pipeline);

    return 1;

test_error:
    CL_free(tokens);
    AST_free(pipeline);

    return 0;
}


int test_parse()
{
    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "ls"), {TOK_END}}, "ls", 1));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "ls"),
        TOK_new(TOK_WORD, "--color"), {TOK_END}}, "ls --color", 2));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "cd"),
            TOK_new(TOK_WORD, "Plaid\\ Shell\\ Playground"),
            {TOK_END}}, "cd Plaid\\ Shell\\ Playground", 2));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "ls"),
            TOK_new(TOK_WORD, "*.txt"), {TOK_END}}, "ls *.txt", 2));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "echo"),
            TOK_new(TOK_WORD, "$PATH"), {TOK_END}}, "echo $PATH", 2));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "author"), {TOK_PIPE},
            TOK_new(TOK_WORD, "sed"), TOK_new(TOK_WORD, "-e"),
            TOK_new(TOK_QUOTED_WORD, "\"s/^/Written by /\""), {TOK_END}},
            "author | sed -e \"s/^/Written by /\"", 5));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "grep"), TOK_new(TOK_WORD, "Happy"),
            TOK_new(TOK_WORD, "*.txt"), {TOK_END}},"grep Happy *.txt", 3));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "cat"),
            TOK_new(TOK_QUOTED_WORD, "\"best sitcoms.txt\""), {TOK_PIPE},
            TOK_new(TOK_WORD, "grep"), TOK_new(TOK_WORD, "Seinfield"),
            {TOK_END}}, "cat \"best sitcoms.txt\" | grep Seinfield", 5));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "sed"), TOK_new(TOK_WORD, "-ne"),
            TOK_new(TOK_QUOTED_WORD, "\"s/The Simpsons/I Love Lucy/p\""),
            {TOK_LESSTHAN}, TOK_new(TOK_WORD, "best\\ sitcoms.txt"),
            {TOK_GREATERTHAN}, TOK_new(TOK_WORD, "output"), {TOK_END}},
            "sed -ne \"s/The Simpsons/I Love Lucy/p\" < best\\ sitcoms.txt > output", 7));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "ls"), TOK_new(TOK_WORD, "-l"), {TOK_END}},
        "ls -l", 2));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "cat"),
            TOK_new(TOK_QUOTED_WORD, "\"best sitcoms.txt\""),
            {TOK_PIPE}, TOK_new(TOK_WORD, "grep"),
            TOK_new(TOK_WORD, "Seinfield"), {TOK_PIPE},
            TOK_new(TOK_WORD, "wc"), TOK_new(TOK_WORD, "-l"), {TOK_END}},
            "cat \"best sitcoms.txt\" | grep Seinfield | wc -l", 8));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "this"),
            TOK_new(TOK_WORD, "is"), TOK_new(TOK_WORD, "not"),
            TOK_new(TOK_WORD, "a"), TOK_new(TOK_WORD, "command"),
            {TOK_END}}, "this is not a command", 5));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "cd"), TOK_new(TOK_WORD, "~"),
            {TOK_END}}, "cd ~", 2));


    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "echo"),
            TOK_new(TOK_QUOTED_WORD,
                "\"Operator could you help me place this call?\""), {TOK_END}},
            "echo \"Operator could you help me place this call?\"", 2));

    test_assert(test_parse_once(
        (Token []){TOK_new(TOK_WORD, "seq"), TOK_new(TOK_WORD, "10"),
            {TOK_PIPE}, TOK_new(TOK_WORD, "wc"),
            TOK_new(TOK_QUOTED_WORD, "\"-l\""), {TOK_END}},
            "seq 10 | wc \"-l\"", 5));

    return 1;

test_error:
    return 0;
}


int test_parse_errors()
{
    size_t errmsg_sz = 128;
    char errmsg[errmsg_sz];
    CList tokens = NULL;

    tokens = TOK_tokenize_input("|", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 1);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "PIPE must have a left expression"));
    CL_free(tokens);

    tokens = TOK_tokenize_input("cat |", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 2);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "PIPE must have a right expression"));
    CL_free(tokens);

    tokens = TOK_tokenize_input(">", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 1);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "GREATERTHAN must have a left expression"));
    CL_free(tokens);

    tokens = TOK_tokenize_input("ls <", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 2);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "LESSTHAN expected TOK_WORD next, but got (end)"));
    CL_free(tokens);

    tokens = TOK_tokenize_input("ls < \"file.txt\"", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 3);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "LESSTHAN expected TOK_WORD next, but got QUOTED_WORD"));
    CL_free(tokens);

    tokens = TOK_tokenize_input("ls | < \"file.txt\"", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 4);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "Invalid PIPE right expression"));
    CL_free(tokens);

    tokens = TOK_tokenize_input("ls > file > file", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 5);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "Pipeline may have at most one GREATERTHAN"));
    CL_free(tokens);

    tokens = TOK_tokenize_input("ls < file < file", errmsg, errmsg_sz);
    test_assert(CL_length(tokens) == 5);
    test_assert(!Parse(tokens, errmsg, errmsg_sz));
    test_assert(!strcasecmp(errmsg, "Pipeline may have at most one LESSTHAN"));
    CL_free(tokens);

    return 1;

test_error:
    CL_free(tokens);
    return 0;
}

int main(int argc, char* argv[])
{
    int passed = 0;
    int num_tests = 0;

    num_tests++; passed += test_tok_tokenize_input();
    num_tests++; passed += test_ast_pipeline();
    num_tests++; passed += test_parse();
    num_tests++; passed += test_parse_errors();

    printf("Passed %d/%d test cases\n", passed, num_tests);
    fflush(stdout);

    return 0;
}
