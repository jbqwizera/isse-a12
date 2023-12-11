/*
 * pipeline.h
 * 
 * A dynamically allocated abstract syntax tree to handle
 * arbitrary expressions
 *
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */

#ifndef _PIPELINE_H_
#define _PIPELINE_H_


#include <stdio.h>

typedef struct _ast_node* AST;

typedef enum {
    WORD,
    QUOTED_WORD,
    OP_LESSTHAN,
    OP_GREATERTHAN,
    OP_PIPE
} ASTNodeType;


AST AST_word(ASTNodeType type, AST right, const char* word);

AST AST_redirect(ASTNodeType type, AST right);

AST AST_pipe(AST left, AST right);

void AST_append(AST* pipelinep, AST right);

int AST_count(AST pipeline);

ASTNodeType AST_type(AST pipeline);

void AST_free(AST pipeline);

size_t AST_pipeline2str(AST pipeline, char* buf, size_t buf_sz);

/*
 * Fork/exec child processes to execute the provided abstract
 * syntax tree. On error, write the error to the errmsg buffer exit program
 *
 * Parameters:
 *  AST     The abstract syntax tree to process
 *  char*   The error message buffer space
 *  size_t  The size of the error message buffer space
 */
void AST_execute(AST pipeline, char* errmsg, size_t errmsg_sz);

#endif /* _PIPELINE_H_ */
