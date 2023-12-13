/*
 * pipeline.c
 * 
 * A dynamically allocated abstract syntax tree (aka pipeline)
 * to handle arbitrary expressions
 *
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "token.h"
#include "pipeline.h"

struct _ast_node {
    ASTNodeType type;
    const char* value;
    struct _ast_node* left;
    struct _ast_node* right;
};

/*
 * Convert an ASTNodeType into a printable character
 *
 * Parameters:
 *   ent    The ASTNodeType to convert
 * 
 * Returns: A single character representing the ent
 */

static char ASTNodeType_to_char(ASTNodeType ent)
{
    switch(ent) {
        case WORD:           return 'w';
        case QUOTED_WORD:    return 'W';
        case OP_LESSTHAN:    return '<';
        case OP_GREATERTHAN: return '>';
        case OP_PIPE:        return '|';
    }
    __builtin_unreachable();
}


// Documented in .h file
AST AST_word(ASTNodeType type, AST right, const char* value)
{
    assert(type == WORD || type == QUOTED_WORD);
    assert(value);

    AST ret = (AST) malloc(sizeof(struct _ast_node));
    assert(ret);

    ret->type = type;
    ret->value = strdup(value);
    ret->right = right;

    return ret;
}

// Documented in .h file
AST AST_redirect(ASTNodeType type, AST right)
{
    assert(type == OP_LESSTHAN || type == OP_GREATERTHAN);
    assert(right && right->type == WORD);

    AST ret = (AST) malloc(sizeof(struct _ast_node));
    assert(ret);

    ret->type = type;
    ret->right = right;

    return ret;
}

// Documented in .h file
AST AST_pipe(AST left, AST right)
{
    assert(left && right);

    AST ret = (AST) malloc(sizeof(struct _ast_node));
    assert(ret);

    ret->type = OP_PIPE;
    ret->left = left;
    ret->right = right;

    return ret;
}

void AST_append(AST* pipelinep, AST right)
{
    if (!right) return;

    if (*pipelinep == NULL) {
        *pipelinep = right;
        return;
    }

    AST temp = (*pipelinep)->right;
    if (!temp) {
        (*pipelinep)->right = right;
        return;
    }

    while (temp && temp->right) temp = temp->right;
    temp->right = right;
}


// Documented in .h file
ASTNodeType AST_type(AST pipeline)
{
    assert(pipeline);
    return pipeline->type;
}

// Documented in .h file
int AST_count(AST pipeline)
{
    if (!pipeline) return 0;
    int ret = 1;
    if (pipeline->type == OP_PIPE) ret += AST_count(pipeline->left);
    return ret + AST_count(pipeline->right);
}


// Documented in .h file
void AST_free(AST pipeline)
{
    if (!pipeline) return;

    ASTNodeType type = pipeline->type;
    AST_free(pipeline->right);
    if (type == OP_PIPE) AST_free(pipeline->left);
    if (type == WORD || type == QUOTED_WORD) free((void *) pipeline->value);
    free(pipeline);
}


/*
 * Exit program with a left, AST right and generated exit code
 *
 * Parameters:
 *  const char* The message to print for the last error
 *  const char* Note for more context
 */
static void exit_failure(const char* message, const char* context)
{
    char buffer[128];
    buffer[0] = 0;
    strcat(buffer, message);
    if (context && *context) {
        strcat(buffer, " (");
        strcat(buffer, context);
        strcat(buffer, ")");
    }
    perror(buffer);
    exit(EXIT_FAILURE);
}


// Documented in .h file
static int isword(ASTNodeType type)
{   return type == WORD || type == QUOTED_WORD; }


static int pipescount(AST pipeline)
{
    if (!pipeline) return 0;

    int ret = pipescount(pipeline->right);
    if (pipeline->type == OP_PIPE)
        ret = 1 + pipescount(pipeline->left);

    return ret;
}

static void setargs(AST pipeline, char*** argvs, int* argcs, int n)
{
    // process children right to left, due to somewhat left-associative
    // nature of the pipe operator
    int i = n - 1;
    while (pipeline && pipeline->type == OP_PIPE) {
        argcs[i] = 0;
        for (AST pl = pipeline->right; pl; pl = pl->right)
            argcs[i]++;

        int j = 0;
        argvs[i] = (char**) malloc((argcs[i] + 1) * sizeof(char*));
        for (AST pl = pipeline->right; pl; pl = pl->right) {
            ASTNodeType type = pl->type;
            if      (isword(type)) argvs[i][j++] = (char*) pl->value;
            else if (type == OP_LESSTHAN) argvs[i][j++] = "<";
            else if (type == OP_GREATERTHAN) argvs[i][j++] = ">";
        }
        argvs[i][j] = NULL;

        pipeline = pipeline->left;
        i--;
    }

    // leftmost child in the pipeline
    argcs[i] = 0;
    for (AST pl = pipeline; pl; pl = pl->right)
        argcs[i]++;

    int j = 0;
    argvs[i] = (char**) malloc((argcs[i] + 1) * sizeof(char*));
    for (AST pl = pipeline; pl; pl = pl->right) {
        ASTNodeType type = pl->type;
        if      (isword(type)) argvs[i][j++] = (char*) pl->value;
        else if (type == OP_LESSTHAN) argvs[i][j++] = "<";
        else if (type == OP_GREATERTHAN) argvs[i][j++] = ">";
    }
    argvs[i][j] = NULL;
}

int AST_execute(AST pipeline, char* errmsg, size_t errmsg_sz)
{
    int num_pipes = pipescount(pipeline);
    int num_children = num_pipes + 1;
    char*** argvs = (char***) malloc(num_children * sizeof(char**));
    int argcs[num_children];
    int pids[num_children];
    int fds[num_pipes][2];

    setargs(pipeline, argvs, argcs, num_children);

    for (int i = 0; i < num_pipes; i++)
        if (pipe(fds[i]) == -1)
            exit_failure("pipe", NULL);

    for (int i = 0; i < num_children; i++) {
        int argc = argcs[i];
        char** argv = argvs[i];

        pids[i] = fork();
        if (pids[i] ==-1) exit_failure("fork", NULL);
        if (pids[i] == 0) {
            // close all fds before exec
            for (int j = 0; j < num_pipes; j++) {
                // first child uses fds[0][1]           for writing
                // last  child uses fds[num_pipes-1][0] for reading
                // i-th  child uses fds[i-1][0]         for reading
                //             and  fds[i][1]           for writing
                // close everything else
                if (i == 0) {
                    close(fds[j][0]);
                    if (j != 0) close(fds[j][1]);
                }
                else if (i == num_children-1) {
                    close(fds[j][1]);
                    if (j != num_pipes-1) close(fds[j][0]);
                }
                else {
                    if (j != i-1) close(fds[j][0]);
                    if (j != i)   close(fds[j][1]);
                }
            }

            if (argc-2 >= 0) {
                char* value = argv[argc-2];
                if (!strcmp(value, "<")) {
                    // input redirection
                    int ifd = open(argv[argc-1], O_RDONLY);
                    if (ifd == -1) exit_failure("open", argv[argc-1]);
                    dup2(ifd, STDIN_FILENO);
                    close(ifd);
                    argv[argc-2] = NULL;
                }
                else if (!strcmp(value, ">")) {
                    // output redirection
                    int ofd = open(argv[argc-1], O_RDWR | O_CREAT | O_TRUNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                    if (ofd == -1) exit_failure("open", argv[argc-1]);
                    dup2(ofd, STDOUT_FILENO);
                    close(ofd);
                    argv[argc-2] = NULL;
                }
            }

            // piping: redirect stdin to previous pipe
            if (i > 0) {
                dup2(fds[i-1][0], STDIN_FILENO);
                close(fds[i-1][0]);
            }

            // piping: redirect stdout to next pipe
            if (i < num_children-1) {
                dup2(fds[i][1], STDOUT_FILENO);
                close(fds[i][1]);
            }

            // execute
            execvp(argv[0], argv);
            exit_failure("execvp", argv[0]);
        }
        free(argvs[i]);
    }
    free(argvs);


    // close all fds in parent
    for (int i = 0; i < num_pipes; i++)
    {   close(fds[i][0]); close(fds[i][1]); }

    // wait for all children to finish executing
    int exit_val = 0;
    for (int i = 0; i < num_children; i++) {
        int exit_status;
        int pid = wait(&exit_status);

        if (WEXITSTATUS(exit_status) != 0) {
            exit_val = WEXITSTATUS(exit_status);
            printf("Child %d exited with status %d\n",
                pid, WEXITSTATUS(exit_status));
        }
    }

    return exit_val;
}


/*
 * Returns the minimum of two unsigned long integers
 *
 * Parameters:
 *  unsigned long   The first integer
 *  unsigned long   The second integer
 *
 * Returns:
 *  unsigned long   The minimum of the two integers
 */
static unsigned long min(unsigned long a, unsigned long b)
{   return a < b? a: b; }


/*
 * Helper function for AST_pipeline2str.
 *
 * Parameters:
 *  AST         The input pipeline to print
 *  char*       The buffer to write the pipeline to
 *  size_t      The size of the buffer
 */
size_t size = 0;
static void AST_sprint(AST pipeline, char* buf, size_t buf_sz)
{
    // Don't attemp to print NULL pipeline or beyond buffer memory
    if (!pipeline || size >= buf_sz) return;

    ASTNodeType type = pipeline->type;
    if (type == OP_PIPE) {
        AST_sprint(pipeline->left, buf, buf_sz);
        size += min(buf_sz - size,
                snprintf(buf + size, buf_sz - size, "| "));
        AST_sprint(pipeline->right, buf, buf_sz);
    } else {
        if (type == WORD || type == QUOTED_WORD)
            size += min(buf_sz - size,
                    snprintf(buf + size, buf_sz - size, "%s ", pipeline->value));
        else
            size += min(buf_sz - size,
                    snprintf(buf + size, buf_sz - size, "%c ", ASTNodeType_to_char(type)));
        AST_sprint(pipeline->right, buf, buf_sz);
    }
}

// Documented in .h file
size_t AST_pipeline2str(AST pipeline, char* buf, size_t buf_sz)
{
    // write to buffer
    size = 0;
    AST_sprint(pipeline, buf, buf_sz);
    while (buf[size-1] == ' ') --size;

    // terminate string, if buffer is full mark truncation
    if (size < buf_sz) buf[size] = 0;
    else {
        buf[--size] = 0;
        buf[size-1] = '$';
    }

    return size;
}
