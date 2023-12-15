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


/* Allocate memory and initialize a pipeline node of type WORD or QUOTED_WORD
 *
 * Parameters:
 *  ASTNodeType The type of the pipeline node to create, either of WORD or
 *              QUOTED_WORD
 *  AST         The pipeline next link
 *  const char* The word value of the node
 *
 * Returns:
 *  AST         The malloc'ed and initialized pipeline node
 */
AST AST_word(ASTNodeType type, AST right, const char* word);


/* Allocate memory and initialize a pipeline node of type
 * OP_LESSTHAN or OP_GREATERTHAN
 *
 * Parameters:
 *  ASTNodeType The type of the pipeline node to create, either of OP_LESSTHAN
 *              or OP_GREATERTHAN
 *  AST         The pipeline next link
 *
 * Returns:
 *  AST         The malloc'ed and initialized pipeline node 
 */ 
AST AST_redirect(ASTNodeType type, AST right);


/* Allocate memory and initialize a pipeline node of type OP_PIPE
 *
 * Parameters:
 *  AST     The left pipeline of the pipeline to be created
 *  AST     The right command of the pipeline to be created
 *
 * Returns:
 *  AST     The malloc'ed and initialized pipeline node
 */
AST AST_pipe(AST left, AST right);


/* Append a pipeline to the right end of another pipeline
 *
 * Parameters:
 *  AST*    The pointer to the pipeline to append to
 *  AST     The pipeline to append
 */
void AST_append(AST* pipelinep, AST right);


/* Count the number of nodes in the pipeline
 *
 * Parameters:
 *  AST     The pipeline to count nodes for
 *
 * Returns:
 *  int     The number of nodes in the pipeline
 */
int AST_countnodes(AST pipeline);


/*
 * Count the number of pipes in the pipeline
 *
 * Parameters:
 *  AST     The pipeline to count pipes for
 *
 * Returns:
 *  int     The number of pipes in the pipeline
 */
int AST_countpipes(AST pipeline);


/* Count the number of commands in the pipeline. Commands are units in the
 * pipeline delimited by pipes
 *
 * Parameters:
 *  AST     The pipeline to count commands for
 *
 * Returns:
 *  int     The number of commands in the pipeline
 */
int AST_countcommands(AST pipeline);


/* The type of the pipeline node
 *
 * Parameters:
 *  AST     The pipeline to find type for
 *
 * Returns:
 *  ASTNodeType The type of the pipeline node
 */
ASTNodeType AST_type(AST pipeline);


/* Return to heap the malloc'ed memory of the nodes in the pipeline
 *
 * Parameters:
 *  AST     The pipeline to free memory for
 */
void AST_free(AST pipeline);


/*
 * Convert an AST pipeline into a printable ASCII string stored in buf
 *
 * Parameters:
 *  AST     The pipeline
 *  char*   The buffer
 *  size_t  The size of the buffer, in bytes
 * 
 * If it takes more characters to represent the pipeline than are allowed in
 * buf, this function places as many characters as will fit into buf,
 * followed by a '$' character to indicate the result was truncated.
 * Buf will always be terminated with a \0 and in no case will this
 * function write characters beyond the end of buf.
 * 
 * Returns:
 *  size_t  The number of characters written to buf, not counting
 *          the \0 terminator
 */
size_t AST_pipeline2str(AST pipeline, char* buf, size_t buf_sz);


/*
 * Fork/exec child processes for each of the commands in the pipeline
 * to execute the provided abstract syntax tree.
 *
 * Parameters:
 *  AST     The abstract syntax tree to process
 *
 * Returns:
 *  int     The exit status of execution. Non-zero if any child process
 *          exited with a non-zero wait status, otherwise 0.
 */
int AST_execute(AST pipeline);

#endif /* _PIPELINE_H_ */
