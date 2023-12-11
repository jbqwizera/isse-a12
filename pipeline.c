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

static int isinternal(const char* name)
{
    return !strcmp(name, "outo") ||
        !strcmp(name, "exit") ||
        !strcmp(name, "author") ||
        !strcmp(name, "pwd") ||
        !strcmp(name, "cd");
}

static void internal(const char* name, const char* dir)
{
    size_t buffer_sz = 256;
    char buffer[buffer_sz];

    if      (!strcmp(name, "exit")) exit(0);
    else if (!strcmp(name, "quit")) exit(0);
    else if (!strcmp(name, "author")) printf("Jean Baptiste Kwizera\n");
    else if (!strcmp(name, "pwd")) {
        getcwd(buffer, buffer_sz);
        printf("%s\n", buffer);
    }
    else if (!strcmp(name, "cd")) {
        dir? chdir(dir): chdir(getenv("HOME"));
    }
}

void AST_execute(AST pipeline, char* errmsg, size_t errmsg_sz)
{
    if (!pipeline) return;

    pid_t pid = fork();
    if (pid ==-1) exit_failure("fork", NULL);
    if (pid == 0) {
        // store the command + arguments in a NULL-terminated buffer
        char* argv[128];
        int argc = 0;
        AST flow = pipeline;
        for (; flow && isword(flow->type); flow = flow->right)
            argv[argc++] = (char*) flow->value;
        argv[argc] = NULL;
        if (!flow) {
            if (isinternal(argv[0])) {
                internal(argv[0], argv[1]);
                execvp("true", argv);
                exit_failure("true", argv[0]);
            }
            else {
                execvp(argv[0], argv);
                exit_failure("execvp", argv[0]);
            }
        }

        // assume redirection
        // next up in the pipeline must be a file
        assert(flow->right);
        assert(flow->right->type == WORD);
        char* filename = (char*) flow->right->value;

        // redirect file to stdin
        if (flow->type == OP_LESSTHAN) {
            int ifd = open(filename, O_RDONLY);
            if (ifd == -1) exit_failure("open", filename);
            dup2(ifd, STDIN_FILENO);
            close(ifd);
        }
            
        // redirect stdout to file
        if (flow->type == OP_GREATERTHAN) {
            int ofd = open(filename, O_RDWR | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
            if (ofd == -1) exit_failure("open", filename);
            dup2(ofd, STDOUT_FILENO);
            close(ofd);
        }

        // move the pipeline flow
        flow = flow->right;
        flow = flow->right;
        
        // execute
        if (isinternal(argv[0])) {
            internal(argv[0], argv[1]);
            execvp("true", argv);
            exit_failure("true", argv[0]);
        }
        else {
            execvp(argv[0], argv);
            exit_failure("execvp", argv[0]);
        }
    }

    int exitstatus;
    waitpid(pid, &exitstatus, 0);
    if (exitstatus) {
        snprintf(errmsg, errmsg_sz,
            "child %d exited with status %d\n", pid, exitstatus);
        printf("Exiting program...\n");
    }
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
