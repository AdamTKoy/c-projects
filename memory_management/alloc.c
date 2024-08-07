/**
 * malloc
 * CS 341 - Spring 2024
 */

#define HEAP_CHECKER 0

#define heap_check()
#if HEAP_CHECKER
#undef heap_check
#define heap_check()                 \
    do                               \
    {                                \
        if (!heap_checker(__LINE__)) \
            exit(1);                 \
    } while (0)
#endif

#define SPLIT_LIMIT 1024
#define BLOCK_MULTIPLE 4
#define ADJ_MULT 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct metadata
{

    int free; // indicates whether block in use (0) or available (1)

    size_t request_size; // size of request, not incl. metadata
    size_t total_size;   // total bytes including padding for struct and block multiple

    struct metadata *prev; // pointer to preceding block's metadata
    struct metadata *next; // pointer to next block's metadata

    struct metadata *prev_free; // pointer to preceding block's metadata
    struct metadata *next_free; // pointer to next block's metadata

} metadata_t;

int heap_checker(int lineno);

// static variable for start of heap
static void *start_of_heap = NULL;
static metadata_t *head_struct = NULL;
static metadata_t *tail_struct = NULL;

static int free_blocks = 0;

// linked list to keep track of ONLY free blocks
static metadata_t *free_blocks_head = NULL;
static metadata_t *free_blocks_tail = NULL;

// when taking an existing free block, is it *too* big and should be split?
void *split_extra(void *h_void, metadata_t *h, size_t adj_size);

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size)
{
    if (start_of_heap == NULL)
    {
        start_of_heap = sbrk(0);
    }

    size_t md_plus_request = (num * size) + sizeof(metadata_t);
    size_t adj_size = md_plus_request;

    if (md_plus_request % BLOCK_MULTIPLE != 0)
    { // ensures all blocks always multiples of BLOCK_MULTIPLE
        adj_size = ((md_plus_request / BLOCK_MULTIPLE) + 1) * BLOCK_MULTIPLE;
    }

    if (free_blocks > 0)
    {

        metadata_t *h = free_blocks_head;

        while (h)
        {
            if (h->total_size >= adj_size)
            {
                // use this available block
                free_blocks--;
                h->free = 0;
                h->request_size = num * size;

                void *h_void = (void *)h;

                void *user_mem = split_extra(h_void, h, adj_size);

                memset(user_mem, 0, num * size);
                return user_mem;
            }

            h = h->next_free;
        }
    }

    // No existing blocks matched
    void *calloc_mem = sbrk(adj_size);
    if (calloc_mem == (void *)-1)
    {
        return NULL;
    }

    // establish struct at front of block
    metadata_t *md = (metadata_t *)(calloc_mem);
    md->request_size = num * size;
    md->total_size = adj_size;
    md->free = 0;
    md->next = NULL;

    if (calloc_mem == start_of_heap)
    { // FIRST request
        md->prev = NULL;
        head_struct = md;
        tail_struct = md;
    }
    else
    {
        md->prev = tail_struct;
        tail_struct->next = md;
        tail_struct = md;
    }

    void *user_mem = calloc_mem + sizeof(metadata_t);
    memset(user_mem, 0, num * size);
    return user_mem;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size)
{
    if (start_of_heap == NULL)
    {
        start_of_heap = sbrk(0);
    }

    size_t md_plus_request = size + sizeof(metadata_t);
    size_t adj_size = md_plus_request;

    if (md_plus_request % BLOCK_MULTIPLE != 0)
    { // ensures block size always multiple of BLOCK_MULTIPLE
        adj_size = ((md_plus_request / BLOCK_MULTIPLE) + 1) * BLOCK_MULTIPLE;
    }

    if (free_blocks > 0)
    { // search through linked list of free blocks

        metadata_t *h = free_blocks_head;

        while (h)
        {
            if (h->total_size >= adj_size)
            { // take first available match

                free_blocks--;
                h->free = 0;
                h->request_size = size;

                void *h_void = (void *)h;
                return split_extra(h_void, h, adj_size);
            }

            h = h->next_free;
        }
    }

    // No existing blocks matched
    void *malloc_mem = sbrk(adj_size);
    if (malloc_mem == (void *)-1)
    {
        return NULL;
    }

    // heap_check();

    // establish struct at front of block
    metadata_t *md = (metadata_t *)(malloc_mem);
    md->request_size = size;
    md->total_size = adj_size;
    md->free = 0;
    md->next = NULL;
    md->next_free = NULL;
    md->prev_free = NULL;

    if (malloc_mem == start_of_heap)
    { // FIRST request
        md->prev = NULL;
        head_struct = md;
        tail_struct = md;
    }
    else
    {
        md->prev = tail_struct;
        tail_struct->next = md;
        tail_struct = md;
    }

    return malloc_mem + sizeof(metadata_t);
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr)
{
    // (1) shift ptr back to get start of metadata_t
    // (2) cast the void * to a metadata_t *
    // (3) check if 'prev' and/or 'next' pointers are also free, if so --> coalesce
    // (4) Update metadata struct appropriately (free = 1, total_size, prev, next)

    if (ptr)
    {

        void *meta = ptr - sizeof(metadata_t);

        metadata_t *block_to_free = (metadata_t *)(meta);

        metadata_t *previous_block = block_to_free->prev;
        metadata_t *next_block = block_to_free->next;

        //  coalescing

        // if previous block exists AND
        if (previous_block && previous_block->free)
        { // Left block free?

            if (next_block && next_block->free)
            { // Right block *also* free?

                previous_block->total_size += block_to_free->total_size + next_block->total_size;
                previous_block->next = next_block->next;

                previous_block->next_free = next_block->next_free;

                if (next_block->next_free)
                {
                    next_block->next_free->prev_free = previous_block;
                }
                else
                {
                    free_blocks_tail = previous_block;
                }

                if (next_block->next)
                {
                    next_block->next->prev = previous_block;
                }
                else
                {
                    tail_struct = previous_block;
                }

                free_blocks--; // had 2 free blocks, turned into 1 bigger free block (net -1 free blocks)
            }
            else
            { // ONLY left block free
                previous_block->total_size += block_to_free->total_size;
                previous_block->request_size = 0;

                // previous_block's prev_free and next_free stay the same
                // if it was the free head or free tail, that also stays the same

                previous_block->next = block_to_free->next;

                if (block_to_free->next)
                {
                    block_to_free->next->prev = previous_block;
                }
                else
                {
                    tail_struct = previous_block;
                }
            }
        }

        else if (next_block && next_block->free)
        { // coalecse ONLY w/ next block

            block_to_free->total_size += next_block->total_size;
            block_to_free->request_size = 0;
            block_to_free->next = next_block->next;

            // modify free block list
            block_to_free->next_free = next_block->next_free;

            if (next_block->next_free)
            {
                next_block->next_free->prev_free = block_to_free;
            }
            else
            {
                free_blocks_tail = block_to_free;
            }

            block_to_free->prev_free = next_block->prev_free;

            if (next_block->prev_free)
            {
                next_block->prev_free->next_free = block_to_free;
            }
            else
            {
                free_blocks_head = block_to_free;
            }

            // modifying ALL block list
            if (next_block->next)
            {
                next_block->next->prev = block_to_free;
            }
            else
            {
                tail_struct = block_to_free;
            }

            block_to_free->free = 1;
        }

        else
        { // no coalescing possible, manually add into linked list of free blocks

            if (free_blocks_head == NULL)
            { // adding to empty free block list
                free_blocks_head = block_to_free;
                free_blocks_tail = block_to_free;
            }

            else if (block_to_free < free_blocks_head)
            { // adding to front of free blocks list
                free_blocks_head->prev_free = block_to_free;
                block_to_free->next_free = free_blocks_head;
                free_blocks_head = block_to_free;
            }

            else if (block_to_free > free_blocks_tail)
            { // adding to end of free blocks list
                free_blocks_tail->next_free = block_to_free;
                block_to_free->prev_free = free_blocks_tail;
                free_blocks_tail = block_to_free;
            }

            else
            { // placing somewhere in the middle
                metadata_t *current = free_blocks_head;
                metadata_t *next = NULL;

                while (current)
                {
                    next = current->next_free;

                    if (next > block_to_free)
                    {
                        block_to_free->next_free = next;
                        next->prev_free = block_to_free;
                        block_to_free->prev_free = current;
                        current->next_free = block_to_free;
                        break;
                    }
                    current = current->next_free;
                }
            }

            block_to_free->free = 1;
            block_to_free->request_size = 0;
            free_blocks++;
        }
    }
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size)
{
    // NULL pointer --> allocate new
    if (!ptr)
    {
        return malloc(size);
    }

    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    size_t new_request_size = size + sizeof(metadata_t);

    void *shifted_ptr = ptr - sizeof(metadata_t);
    metadata_t *existing = (metadata_t *)(shifted_ptr);

    // first check if existing block can handle the increase
    if (new_request_size <= existing->total_size)
    {
        existing->request_size = size;
        return ptr;
    }

    metadata_t *next_block = existing->next;
    if ((next_block && next_block->free) && (next_block->total_size + existing->total_size >= new_request_size))
    {

        existing->request_size = size;
        existing->total_size += next_block->total_size;

        if (next_block->prev_free)
        {
            next_block->prev_free->next_free = next_block->next_free;
        }
        else
        {
            free_blocks_head = next_block->next_free;
        }

        if (next_block->next_free)
        {
            next_block->next_free->prev_free = next_block->prev_free;
        }
        else
        {
            free_blocks_tail = next_block->prev_free;
        }

        if (next_block->next)
        {
            next_block->next->prev = existing;
        }
        else
        {
            tail_struct = existing;
        }

        existing->next = existing->next->next;

        free_blocks--;

        return ptr;
    }

    // otherwise: completely new location

    // void *memcpy(void *dest, const void * src, size_t n)
    // The C library function void *memcpy(void *dest, const void *src, size_t n)
    // copies n characters from memory area src to memory area dest.

    // (1) Find a new spot
    // (2) Split if necessary
    // (3) Copy everything over
    // (4) Free old spot and coalesce if possible

    size_t adj_size = new_request_size;

    size_t num_chars_to_copy = size;
    if (existing->request_size < size)
    {
        num_chars_to_copy = existing->request_size;
    }

    if (free_blocks > 0)
    {

        metadata_t *h = free_blocks_head;

        while (h)
        {
            if (h->total_size >= adj_size)
            {
                // use this available block
                h->free = 0;
                h->request_size = size;
                free_blocks--;
                // prev, next, total_size should remain the same

                void *h_void = (void *)(h);

                void *copy_mem_here = split_extra(h_void, h, adj_size);
                const void *original_mem = ptr;
                memcpy(copy_mem_here, original_mem, num_chars_to_copy);

                free(ptr);

                return copy_mem_here;
            }

            h = h->next_free;
        }
    }

    // No existing blocks matched

    // (1) Sbrk a new spot
    // (2) Copy everything over
    // (3) Mark old block as 'free'

    void *malloc_mem = sbrk(adj_size);
    if (malloc_mem == (void *)-1)
    {
        return NULL;
    }

    // insert struct at front of block
    metadata_t *md = malloc_mem;
    md->request_size = size;
    md->total_size = adj_size;
    md->free = 0;
    md->next = NULL;
    md->prev = NULL;
    md->next_free = NULL;
    md->prev_free = NULL;

    if (malloc_mem == start_of_heap)
    { // FIRST request
        head_struct = md;
        tail_struct = md;
    }
    else
    {
        md->prev = tail_struct;
        tail_struct->next = md;
        tail_struct = md;
    }

    void *copy_mem_here = malloc_mem + sizeof(metadata_t);
    const void *original_mem = ptr;
    memcpy(copy_mem_here, original_mem, num_chars_to_copy);

    free(ptr);

    return copy_mem_here;
}

int heap_checker(int lineno)
{
    void *top_of_heap = sbrk(0);
    // void* heap_curr = start_of_heap;

    /*
    int num_free_blocks = 0;
    while (heap_curr < top_of_heap) {
        metadata_t* block = (metadata_t*) heap_curr;
        if (block->free) num_free_blocks++;
        heap_curr = (void*)(block->next);
    }
    */

    // Check that a block's 'next' block also has a 'prev' back to them

    metadata_t *list_curr = head_struct;
    int num_free_nodes = 0;
    while (list_curr)
    {
        if (list_curr->free)
            num_free_nodes++;
        list_curr = list_curr->next;
    }

    /*
    if (num_free_blocks != num_free_nodes) {
        fprintf(stderr, "HEAP CHECK: number of free nodes in list does not match heap at alloc.c:%d\n", lineno);
        return 0;
    }
    */

    fprintf(stderr, "HEAP CHECK line %d: sbrk(0) is %p, number of free nodes is:%d\n", lineno, top_of_heap, num_free_nodes);

    return 1;
}

void *split_extra(void *h_void, metadata_t *h, size_t adj_size)
{

    // only split if newly created free block would be of size at least SPLIT_LIMIT
    if ((h->total_size > adj_size) && (h->total_size - adj_size > SPLIT_LIMIT))
    {

        void *new_block = h_void + adj_size;
        metadata_t *split_block = (metadata_t *)new_block;

        split_block->free = 1;
        split_block->request_size = 0;
        split_block->total_size = h->total_size - adj_size;

        split_block->next = h->next;

        if (h->next)
        {
            h->next->prev = split_block;
        }
        else
        {
            tail_struct = split_block;
        }

        split_block->prev = h;
        h->next = split_block;
        h->total_size = adj_size;

        split_block->next_free = h->next_free;

        if (h->next_free)
        {
            h->next_free->prev_free = split_block;
        }
        else
        {
            free_blocks_tail = split_block;
        }

        split_block->prev_free = h->prev_free;
        if (h->prev_free)
        {
            h->prev_free->next_free = split_block;
        }
        else
        {
            free_blocks_head = split_block;
        }

        h->prev_free = NULL;
        h->next_free = NULL;

        free_blocks++;

        return h_void + sizeof(metadata_t);
    }

    // o.w. not splitting
    if (h->prev_free)
    {
        h->prev_free->next_free = h->next_free;
    }
    else
    {
        free_blocks_head = h->next_free;
    }

    if (h->next_free)
    {
        h->next_free->prev_free = h->prev_free;
    }
    else
    {
        free_blocks_tail = h->prev_free;
    }

    h->prev_free = NULL;
    h->next_free = NULL;

    return h_void + sizeof(metadata_t);
}