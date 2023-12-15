/*
 * parse.h
 * 
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */


#ifndef _PARSE_H_
#define _PARSE_H_

#include "clist.h"
#include "pipeline.h"

/*
 * Parses a list of tokens into a pipeline AST, which is the abstract
 * syntax tree for the plaidsh grammar.
 *
 * Parameters:
 *   tokens     List of tokens remaining to be parsed
 *   errmsg     Return space for an error message, filled in in case of error
 *   errmsg_sz  The size of errmsg
 * 
 * Returns: The parsed pipeline AST on success. If a parsing error is
 *   encountered, copies an error message into errmsg and returns
 *   NULL.
 */
AST Parse(CList tokens, char* errmsg, size_t errmsg_sz);

#endif /* _PARSE_H_ */
