/*
 * clist.c
 * 
 * Linked list implementation for ISSE Assignment 5
 *
 * Author:
 *  Jean Baptiste Kwizera <jkwizera@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "clist.h"

#define DEBUG

struct _cl_node {
    CListElementType element;
    struct _cl_node* next;
};

struct _clist {
    struct _cl_node *head;
    int length;
};



/*
 * Create (malloc) a new _cl_node and populate it with the supplied values
 *
 * Parameters:
 *   element, next  the values for the node to be created
 * 
 * Returns: The newly-malloc'd node, or NULL in case of error
 */
static struct _cl_node*
_CL_new_node(CListElementType element, struct _cl_node *next)
{
    struct _cl_node* new = (struct _cl_node*) malloc(sizeof(struct _cl_node));

    assert(new);

    new->element = element;
    new->next = next;

    return new;
}



// Documented in .h file
CList CL_new()
{
    CList list = (CList) malloc(sizeof(struct _clist));
    assert(list);

    list->head = NULL;
    list->length = 0;

    return list;
}



// Documented in .h file
void CL_free(CList list)
{
    // Free each next node
    if (!list) return;

    while (list->head) {
        struct _cl_node *temp = list->head; // Save ptr to current head
        list->head = list->head->next; // Point head node to next
        temp->next = NULL; // Set to NULL, avoid loitering/mem. orphans nodes
        free((void *) temp->element.value);
        free(temp); // Free current head
    }
    free(list); // Free nodes wrapper
}



// Documented in .h file
int CL_length(CList list)
{
    assert(list);
#ifdef DEBUG
    // In production code, we simply return the stored value for
    // length. However, as a defensive programming method to prevent
    // bugs in our code, in DEBUG mode we walk the list and ensure the
    // number of elements on the list is equal to the stored length.

    int len = 0;
    for (struct _cl_node *node = list->head; node != NULL; node = node->next)
        len++;

    assert(len == list->length);
#endif // DEBUG

    return list->length;
}



// Documented in .h file
void CL_push(CList list, CListElementType element)
{
    assert(list);
    list->head = _CL_new_node(element, list->head);
    list->length++;
}



// Documented in .h file
CListElementType CL_pop(CList list)
{
    assert(list);
    struct _cl_node *popped_node = list->head;

    if (popped_node == NULL)
        return INVALID_RETURN;

    CListElementType ret = popped_node->element;

    // unlink previous head node, then free it
    list->head = popped_node->next;
    free((void *) popped_node->element.value);
    free(popped_node);
    // we cannot refer to popped node any longer

    list->length--;

    return ret;
}



// Documented in .h file
void CL_append(CList list, CListElementType element)
{
    assert(list);

    // Malloc new node to append and increment list length
    // If list was empty just set head to the new node and return
    struct _cl_node *temp = _CL_new_node(element, NULL);
    if (list->length++ == 0) {
        list->head = temp;
        return;
    }

    // Set next of tail to the new node
    struct _cl_node *node = list->head;
    while (node->next) node = node->next;
    node->next = temp;
}




// Documented in .h file
CListElementType CL_nth(CList list, int pos)
{
    assert(list);

    // Validate index position
    if (pos < -list->length || pos >= list->length)
        return INVALID_RETURN;

    // Normalize index position. Iterate the list and return the item at
    // given index position
    int i = (pos + list->length) % list->length;
    for (struct _cl_node *node = list->head; node != NULL; node = node->next)
        if (i-- == 0) return node->element;

    // Avoid control falling to the end of the function. This return will never
    // be reached as index is already validated
    return INVALID_RETURN;
}



// Documented in .h file
bool CL_insert(CList list, CListElementType element, int pos)
{
    assert(list);

    // Validate index
    if (pos < -list->length-1 || pos > list->length) return false;

    // Normalize index and handle special case of pos = 0 with push
    if (pos < 0) pos += list->length + 1;
    if (pos == 0) {
        CL_push(list, element);
        return true;
    }

    // Find the node to insert before
    struct _cl_node *node = list->head;
    while (--pos) node = node->next;

    // Create a new node with the given element and next node as the next of
    // the node to insert before. The node to insert before has the new node
    // as the next node
    node->next = _CL_new_node(element, node->next);
    list->length++;

    return true;
}


    
// Documented in .h file
CListElementType CL_remove(CList list, int pos)
{
    assert(list);

    // Validate index position
    if (pos < -list->length || pos > list->length-1)
        return INVALID_RETURN;

    // Normalize index and handle special case of pos = 0 with pop
    if (pos < 0)  pos += list->length;
    if (pos == 0) return CL_pop(list);

    // Find the node previous to the node to remove
    struct _cl_node *prev = list->head;
    while (--pos) prev = prev->next;

    struct _cl_node *temp = prev->next; // The node to remove
    CListElementType elem = temp->element;
    prev->next = prev->next->next; // Unlink the node to remove
    list->length--;
    temp->next = NULL; // So temp->next isn't just an orphan object in memory
    free((void *) temp->element.value);
    free(temp); // free the removed/unlinked node

    return elem;
}



// Documented in .h file
CList CL_copy(CList list)
{
    assert(list);

    // Re-create each node in current list for the independent CList instance
    CList res = CL_new();
    for (struct _cl_node *node = list->head; node != NULL; node = node->next)
        CL_append(res, node->element);
    return res;
}



// Documented in .h file
void CL_join(CList list1, CList list2)
{
    assert(list1);
    assert(list2);

    // Append list2 items to list1 as they are removed from list2
    while (CL_length(list2))
        CL_append(list1, CL_remove(list2, 0));
}


// Documented in .h file
void CL_reverse(CList list)
{
    assert(list);

    // Head of the nodes in reversed order
    struct _cl_node *reverse = NULL;
    while (list->head) {
        struct _cl_node *second = list->head->next; // Save next of head
        list->head->next = reverse; // Set next of head to the reversed nodes
        reverse = list->head; // The reversed nodes are now 1-item bigger
        list->head = second; // Move head forward
    }
    list->head = reverse;
}


// Documented in .h file
void CL_foreach(CList list, CL_foreach_callback callback, void *cb_data)
{
    assert(list);

    // Simply run callback for each set of arguments
    int i = 0;
    for (struct _cl_node *node = list->head; node != NULL; node = node->next)
        callback(i++, node->element, cb_data);
}
