#include "prior_ll_lib.h"

typedef struct prior_ll_node  node_t;
typedef struct prior_ll_queue queue_t;

static int prior_ll_is_empty(queue_t *p_queue);

/**
 * Evaluates whether a given priority linked list is empty
 * @param p_queue priority linked list to evaluate
 * @return 1 if p_queue is empty, 0 if it isn't
 */
static int
prior_ll_is_empty(queue_t *p_queue)
{
    // validate
    if (!p_queue)
    {
        fprintf(
            stderr,
            "Error: cannot evaluate invalid priority linked list as empty.\n");
        return -1;
    }
    if (0 == p_queue->size)
    {
        return 1;
    }
    return 0;
}

/**
 * Print every item in the priority linked list using a passed function pointer
 * @param p_queue priority linked list to evaluate
 * @param p_print pointer to function to use to evaluate each list item
 */
void
prior_ll_traverse(queue_t *p_queue, void (*p_print)(void *))
{
    node_t *entry_pointer = NULL;
    // validate
    if (!p_queue)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Cannot traverse invalid priority linked list.\n");
        return;
#endif
    }
    entry_pointer = p_queue->p_front;
    // iterate and pass
    while (entry_pointer != NULL)
    {
        p_print(entry_pointer->p_data);
        entry_pointer = entry_pointer->p_next;
    }
}

/**
 * Create a new node in the priority linked list
 * Allocates node object in memory\n
 * Warning: Allocated node must be freed with prior_ll_dequeue,
 * prior_ll_destroy, or manually
 * @param p_data pointer to data to place in node
 * @param p_priority pointer to priority to give new node
 * @return pointer to allocated node object; NULL if unsuccessful
 */
node_t *
prior_ll_create_node(void *p_data, void *p_priority)
{
    node_t *node_pointer = NULL;
    if (!p_data || !p_priority)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Invalid data or priority for priority linked list node "
                "creation.\n");
#endif
        return NULL;
    }
    errno        = 0;
    node_pointer = malloc(sizeof(struct prior_ll_node));
    if (!node_pointer || errno)
    {
#ifdef DEBUG
        perror("Priority Linked List Node Allocation Error");
#endif
        return NULL;
    }
    // allocation successful, set fields
    node_pointer->p_data     = p_data;
    node_pointer->p_priority = p_priority;
    node_pointer->p_next     = NULL;
    // return
    return node_pointer;
}

/**
 * Create priority linked list by allocating object in memory.\n
 * Warning: Allocated node must be freed with prior_ll_destroy or manually
 * @return pointer to priority linked list; NULL if unsuccessful.
 */
queue_t *
prior_ll_create_queue(void)
{
    queue_t *queue_pointer = NULL;
    // attempt to allocate
    errno         = 0;
    queue_pointer = malloc(sizeof(queue_t));
    if (!queue_pointer || errno)
    {
#ifdef DEBUG
        perror("Priority Linked List Allocation Error");
#endif
        return NULL;
    }
    // set fields
    queue_pointer->size    = 0;
    queue_pointer->p_front = NULL;
    queue_pointer->p_rear  = NULL;
    // return
    return queue_pointer;
}

/**
 * Destroy priority linked list object by freeing subordinate nodes and linked
 * list object in memory.
 * @param pp_queue pointer to pointer to priority linked list object to destroy
 */
void
prior_ll_destroy(queue_t **pp_queue)
{
    node_t *next_node = NULL;
    // evaluate param
    if (!*pp_queue)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Invalid priority linked list for destroy operation.\n");
        return;
#endif
    }
    // if not empty, iterate and free

    while ((*pp_queue)->p_front != NULL)
    {
        next_node = (*pp_queue)->p_front->p_next;
        free((*pp_queue)->p_front);
        (*pp_queue)->p_front = next_node;
    }
    // free structure
    free(*pp_queue);
    *pp_queue = NULL;
}

/**
 * Add item to priority linked list using a priority and sorting function
 * to determine placement within the linked list.
 * @param p_queue pointer to priority linked list to operate on
 * @param p_data pointer to data to place in new node in p_queue
 * @param p_compare pointer to function to be used to sort p_queue
 * @param p_priority pointer to priority data to assign to new node in p_queue
 * @return 0 if successful, -1 on failure
 */
int
prior_ll_enqueue(queue_t *p_queue,
                 void    *p_data,
                 int (*p_compare)(void *, void *),
                 void *p_priority)
{
    node_t *new_node  = NULL;
    node_t *prev_node = NULL;
    node_t *next_node = NULL;
    // validate p_queue
    if (!p_queue)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Invalid priority link list passed for enqueue "
                "operation.\n");
#endif
        return -1;
    }
    // validate p_data
    if (!p_data)
    {
        fprintf(stderr, "Error: Invalid value passed for enqueue operation.\n");
        return -1;
    }
    // allocate new node
    new_node = prior_ll_create_node(p_data, p_priority);
    if (!new_node)
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Could not allocate new node for enqueue operation. See "
                "output for details.\n");
#endif
        return -1;
    }
    // if empty, place at head
    if (prior_ll_is_empty(p_queue))
    {
        p_queue->p_front = new_node;
        p_queue->p_rear  = new_node;
        // increase size
        p_queue->size++;
        return 0;
    }
    // iterate over linked list and run comparison function to determine where
    // node should go
    next_node = p_queue->p_front;
    while (next_node != NULL)
    {
        // if comparison nets equal or better, put it behind prev node
        // (thus before next_node)
        if (p_compare(p_priority, next_node->p_priority) == 1)
        {
            // if prev_node is NULL, set p_front to this new node
            if (!prev_node)
            {
                p_queue->p_front = new_node;
            }
            else
            {
                prev_node->p_next = new_node;
            }
            // update next on new_node
            new_node->p_next = next_node;
            // EXIT loop when done placing node
            break;
        }
        // set up nodes for next
        prev_node = next_node;
        next_node = next_node->p_next;
    }
    // if next_node is NULL, loop reached end of list so just append to rear
    if (!next_node)
    {
        p_queue->p_rear->p_next = new_node;
        // set rear
        p_queue->p_rear = new_node;
    }
    // increase size
    p_queue->size++;
    return 0;
}

/**
 * Retrieves a pointer to the object with the highest priority in the linked
 * list\n
 * @param p_queue pointer to priority linked list to operate on
 * @return pointer to a memory allocation containing the highest priority value
 * in p_queue; NULL if unsuccessful
 */
void *
prior_ll_dequeue(queue_t *p_queue)
{
    node_t *first_node;
    void   *value_pointer;

    // validate
    if (!p_queue || prior_ll_is_empty(p_queue))
    {
#ifdef DEBUG
        fprintf(stderr,
                "Error: Invalid priority linked list for dequeue operation.\n");
#endif
        return NULL;
    }
    // grab top (highest priority at any given time)
    first_node = p_queue->p_front;
    if (!first_node)
    {
        return NULL;
    }
    value_pointer = first_node->p_data;
    // remove from linked list by moving p_front up one
    p_queue->p_front = p_queue->p_front->p_next;
    // free node memory
    free(first_node);
    // reduce size
    p_queue->size--;
    return value_pointer;
}
