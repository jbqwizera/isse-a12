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


#define   __builtin_cd "true"
#define __builtin_auth "echo"
#define         AUTHOR "Jean Baptiste Kwizera"


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


/* Check if a pipeline node type is WORD or QUOTED word
 *
 * Parameters:
 *  ASTNodeType The type of a pipeline
 * 
 * Returns:
 *  int         0 if type is neither WORD nor QUOTED_WORD
 *              non-zero value otherwise.
 */
static int isword(ASTNodeType type)
{   return type == WORD || type == QUOTED_WORD; }


// Documented in .h file
AST AST_word(ASTNodeType type, AST right, const char* value)
{
    assert(type == WORD || type == QUOTED_WORD);
    assert(value);

    AST ret = (AST) malloc(sizeof(struct _ast_node));
    assert(ret);

    ret->type  = type;
    ret->value = strdup(value);
    ret->left  = NULL;
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

    ret->type  = type;
    ret->value = NULL;
    ret->left  = NULL;
    ret->right = right;

    return ret;
}


// Documented in .h file
AST AST_pipe(AST left, AST right)
{
    assert(left && right);

    AST ret = (AST) malloc(sizeof(struct _ast_node));
    assert(ret);

    ret->type  = OP_PIPE;
    ret->value = NULL;
    ret->left  = left;
    ret->right = right;

    return ret;
}


// Documented in .h file
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
int AST_countnodes(AST pipeline)
{
    if (!pipeline) return 0;
    int ret = 1;
    if (pipeline->type == OP_PIPE) ret += AST_countnodes(pipeline->left);
    return ret + AST_countnodes(pipeline->right);
}


// Documented in .h file
int AST_countpipes(AST pipeline)
{
    if (!pipeline || pipeline->type != OP_PIPE) return 0;
    return 1 + AST_countpipes(pipeline->left);
}


// Documented in .h file
int AST_countcommands(AST pipeline)
{   return 1 + AST_countpipes(pipeline); }


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


static void setargs(AST pipeline, char*** argvs, int* argcs, int n)
{
    // process children right to left, due to somewhat left-associative
    // nature of the pipe operator
    int i = n - 1;
    while (pipeline) {
        argcs[i] = 0;
        AST curr = pipeline;
        if (pipeline->type == OP_PIPE) curr = curr->right;

        for (AST iter = curr; iter; iter = iter->right)
            argcs[i]++;

        int j = 0;
        argvs[i] = (char**) malloc((argcs[i] + 1) * sizeof(char*));
        for (AST iter = curr; iter; iter = iter->right) {
            ASTNodeType type = iter->type;
            if      (isword(type)) argvs[i][j++] = (char*) iter->value;
            else if (type == OP_LESSTHAN) argvs[i][j++] = "<";
            else if (type == OP_GREATERTHAN) argvs[i][j++] = ">";
        }
        argvs[i][j] = NULL;

        if (pipeline->type != OP_PIPE) break;
        pipeline = pipeline->left;
        i--;
    }
}


// Documented in .h file
int AST_execute(AST pipeline)
{
    int num_pipes = AST_countpipes(pipeline);
    int num_children = num_pipes + 1;
    char*** argvs = (char***) malloc(num_children * sizeof(char**));
    int argcs[num_children];
    int pids[num_children];
    int fds[num_pipes][2];

    setargs(pipeline, argvs, argcs, num_children);

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(fds[i]) == -1) {
            perror("pipe");
            _exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_children; i++) {
        int argc = argcs[i];
        char** argv = argvs[i];

        // builtin commands - manipulating shell require no forking
        if (!strcmp(argv[0], "exit") || !strcmp(argv[0], "quit")) {
            free(argv);
            _exit(0);
        }
        else if (!strcmp(argv[0], "cd")) {
            char* dirpath = argv[1];
            if (dirpath == NULL) dirpath = getenv("HOME");
            if (chdir(dirpath)) perror("cd");
            free(argv);
            continue;
        }

        // fork-exec a child process for the command
        pids[i] = fork();
        if (pids[i] == -1)
        {   perror("fork"); _exit(EXIT_FAILURE); }

        if (pids[i] == 0) {
            // close all fds before exec
            // child[i] needs pipe[i-1][0] for reading
            // child[i] needs pipe[i+0][1] for writing
            // close all other fds before exec
            for (int j = 0; j < num_pipes; j++) {
                if (j != i-1) close(fds[j][0]);
                if (j != i+0) close(fds[j][1]);
            }

            // check for any i/o redirection
            int lessthan = -1;
            int greaterthan = -1;
            for (int j = 1; j < argc - 1; j++) {
                if (strcmp(argv[j], "<") == 0) lessthan = j;
                if (strcmp(argv[j], ">") == 0) greaterthan = j;
            }

            // input redirection
            if (lessthan >= 0) {
                char* infile = argv[lessthan+1];
                int ifd = open(infile, O_RDONLY);
                if (ifd == -1) {
                    perror("open");
                    printf("%s: Permission denied\n", infile);
                    _exit(EXIT_FAILURE);
                }

                dup2(ifd, STDIN_FILENO);
                close(ifd);

                // shift argv values (including the NULL terminator)
                // to the right of < by 2, overwritting the < and the filename
                int offset = 2;
                for (int j = lessthan; j+offset <= argc; j++)
                    argv[j] = argv[j+offset];
                argc -= offset;
                if (lessthan < greaterthan) greaterthan -= offset;
            }

            // output redirection
            if (greaterthan >= 0) {
                char* outfile = argv[greaterthan+1];
                int ofd = open(outfile, O_RDWR | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                if (ofd == -1) {
                    printf("%s: Permission denied\n", outfile);
                    _exit(EXIT_FAILURE);
                }

                dup2(ofd, STDOUT_FILENO);
                close(ofd);

                // shift argv values (including the NULL terminator)
                // to the right of > by 2, overwritting the < and the filename
                int offset = 2;
                for (int j = greaterthan; j+offset <= argc; j++)
                    argv[j] = argv[j+offset];
                argc -= offset;
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
            if (!strcmp(argv[0], "author")) {
                execlp(__builtin_auth, __builtin_auth, AUTHOR, NULL);
                perror(__builtin_auth);
                _exit(EXIT_FAILURE);
            }

            execvp(argv[0], argv);
            if (!strcmp(strerror(errno), "No such file or directory"))
                printf("%s: Command not found."
                    "*Child.*exited with status %d\n", argv[0], errno);
            else
                perror(argv[0]);
            _exit(EXIT_FAILURE);
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

        if (pid > 0 && WEXITSTATUS(exit_status) != 0) {
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
 *  AST     The input pipeline to print
 *  char*   The buffer to write the pipeline to
 *  size_t  The size of the buffer
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
