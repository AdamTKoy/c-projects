/**
 * vector
 * CS 341 - Spring 2024
 */
#include "vector.h"
#include <assert.h>

// I added:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct vector
{
    /* The function callback for the user to define the way they want to copy
     * elements */
    copy_constructor_type copy_constructor;

    /* The function callback for the user to define the way they want to destroy
     * elements */
    destructor_type destructor;

    /* The function callback for the user to define the way they a default
     * element to be constructed */
    default_constructor_type default_constructor;

    /* Void pointer to the beginning of an array of void pointers to arbitrary
     * data. */
    void **array;

    /**
     * The number of elements in the vector.
     * This is the number of actual objects held in the vector,
     * which is not necessarily equal to its capacity.
     */
    size_t size;

    /**
     * The size of the storage space currently allocated for the vector,
     * expressed in terms of elements.
     */
    size_t capacity;
};

/**
 * IMPLEMENTATION DETAILS
 *
 * The following is documented only in the .c file of vector,
 * since it is implementation specfic and does not concern the user:
 *
 * This vector is defined by the struct above.
 * The struct is complete as is and does not need any modifications.
 *
 * The only conditions of automatic reallocation is that
 * they should happen logarithmically compared to the growth of the size of the
 * vector inorder to achieve amortized constant time complexity for appending to
 * the vector.
 *
 * For our implementation automatic reallocation happens when -and only when-
 * adding to the vector makes its new  size surpass its current vector capacity
 * OR when the user calls on vector_reserve().
 * When this happens the new capacity will be whatever power of the
 * 'GROWTH_FACTOR' greater than or equal to the target capacity.
 * In the case when the new size exceeds the current capacity the target
 * capacity is the new size.
 * In the case when the user calls vector_reserve(n) the target capacity is 'n'
 * itself.
 * We have provided get_new_capacity() to help make this less ambigious.
 */

static size_t get_new_capacity(size_t target)
{
    /**
     * This function works according to 'automatic reallocation'.
     * Start at 1 and keep multiplying by the GROWTH_FACTOR untl
     * you have exceeded or met your target capacity.
     */
    size_t new_capacity = 1;
    while (new_capacity < target)
    {
        new_capacity *= GROWTH_FACTOR;
    }
    return new_capacity;
}

vector *vector_create(copy_constructor_type copy_constructor,
                      destructor_type destructor,
                      default_constructor_type default_constructor)
{
    // your code here

    // allocate space for a 'vector' struct
    vector *new_v = malloc(sizeof(vector));
    if (!new_v)
    {
        printf("Unable to allocate memory (for struct) in vector_create.\n");
        exit(EXIT_FAILURE);
    }
    // if parameters are NULL --> shallow
    if (default_constructor == NULL)
    {
        new_v->copy_constructor = shallow_copy_constructor;
        new_v->destructor = shallow_destructor;
        new_v->default_constructor = shallow_default_constructor;
    }

    else
    {
        new_v->copy_constructor = copy_constructor;
        new_v->destructor = destructor;
        new_v->default_constructor = default_constructor;
    }

    // either way, initial capacity is INITIAL_CAPACITY
    // and initial size (*occupied* elements) is 0
    new_v->capacity = INITIAL_CAPACITY;
    new_v->size = 0;

    // allocate array of INITIAL_CAPACITY arbitrary pointers
    void **new_array = malloc(INITIAL_CAPACITY * sizeof(void *));
    if (!new_array)
    {
        printf("Unable to allocate memory (for array) in vector_create.\n");
        exit(EXIT_FAILURE);
    }
    new_v->array = new_array;
    return new_v;
}

void vector_destroy(vector *this)
{
    assert(this);

    size_t i;
    for (i = 0; i < this->size; i++)
    {
        if (this->array[i])
        {
            this->destructor(this->array[i]);
            this->array[i] = NULL;
        }
    }

    free(this->array);
    free(this);
}

void **vector_begin(vector *this)
{
    return this->array + 0;
}

void **vector_end(vector *this)
{
    return this->array + this->size;
}

size_t vector_size(vector *this)
{
    assert(this);

    return this->size;
}

void vector_resize(vector *this, size_t n)
{
    assert(this);

    if (n != this->size)
    {
        size_t i;

        if (n <= this->capacity)
        {
            if (n < this->size)
            {
                for (i = n; i < this->size; i++)
                {
                    this->destructor(this->array[i]);
                    this->array[i] = NULL;
                }
            }
            else
            { // (n > this->size)
                for (i = this->size; i < n; i++)
                {
                    this->array[i] = this->default_constructor();
                }
            }
        }

        else
        { // (n > this->capacity)
            size_t new_cap = get_new_capacity(n);
            this->array = realloc(this->array, new_cap * sizeof(void *));
            if (!this->array)
            {
                printf("Unable to reallocate memory in vector_resize.\n");
                exit(EXIT_FAILURE);
            }

            for (i = this->size; i < n; i++)
            {
                this->array[i] = this->default_constructor();
            }

            this->capacity = new_cap;
        }

        this->size = n;
    }
}

size_t vector_capacity(vector *this)
{
    assert(this);

    return this->capacity;
}

bool vector_empty(vector *this)
{
    assert(this);

    return (this->size == 0);
}

void vector_reserve(vector *this, size_t n)
{
    assert(this);

    if (n > this->capacity)
    {
        size_t new_cap = get_new_capacity(n);
        this->array = realloc(this->array, new_cap * sizeof(void *));
        if (!this->array)
        {
            printf("Unable to reallocate memory in vector_reserve.\n");
            exit(EXIT_FAILURE);
        }

        this->capacity = new_cap;
    }
}

void **vector_at(vector *this, size_t position)
{
    assert(this);
    assert(position >= 0);
    assert(position < this->size);

    // return reference to element at 'position'
    // a 'reference' to an element is a pointer to a pointer to it
    void **found = this->array + position;
    return found;
}

void vector_set(vector *this, size_t position, void *element)
{
    assert(this);
    assert(position >= 0);
    assert(position < this->size);

    this->destructor(this->array[position]);
    this->array[position] = NULL;
    this->array[position] = this->copy_constructor(element);
}

void *vector_get(vector *this, size_t position)
{
    assert(this);
    assert(!vector_empty(this));

    // gets reference to element @ position
    return *(this->array + position);
}

void **vector_front(vector *this)
{
    assert(this);
    assert(!vector_empty(this));

    return this->array + 0;
}

// last *existing* element (index within 'size')
void **vector_back(vector *this)
{
    assert(this);
    assert(!vector_empty(this));

    return this->array + (this->size - 1);
}

void vector_push_back(vector *this, void *element)
{
    assert(this);

    // if vector full, resize
    if ((this->size + 1) > this->capacity)
    {
        size_t new_cap = get_new_capacity(this->size + 1);
        this->array = realloc(this->array, new_cap * sizeof(void *));
        if (!this->array)
        {
            printf("Unable to reallocate memory in vector_push_back.\n");
            exit(EXIT_FAILURE);
        }

        this->capacity = new_cap;
    }

    // add element
    this->array[this->size] = this->copy_constructor(element);
    this->size = this->size + 1;
}

void vector_pop_back(vector *this)
{
    assert(this);
    assert(this->size > 0);

    this->destructor(this->array[this->size - 1]);
    this->array[this->size - 1] = NULL;
    this->size = this->size - 1;
}

void vector_insert(vector *this, size_t position, void *element)
{
    assert(this);
    assert(position >= 0);
    assert(position <= this->size);

    if (this->size + 1 > this->capacity)
    {
        size_t new_cap = get_new_capacity(this->size + 1);
        this->array = realloc(this->array, new_cap * sizeof(void *));
        if (!this->array)
        {
            printf("Unable to reallocate memory in vector_insert.\n");
            exit(EXIT_FAILURE);
        }

        this->capacity = new_cap;
    }

    size_t i;

    void *new_elem = this->copy_constructor(element);

    for (i = this->size; i > position; i--)
    {
        this->array[i] = this->array[i - 1];
    }

    this->array[position] = new_elem;

    this->size = this->size + 1;
}

void vector_erase(vector *this, size_t position)
{
    assert(this);
    assert(position < vector_size(this));
    assert(position >= 0);

    size_t i;

    void *temp = this->array[position];

    // shift items after index 'position' left and delete last
    for (i = position; i < (this->size - 1); i++)
    {
        this->array[i] = this->array[i + 1];
    }

    this->destructor(temp);
    temp = NULL;

    this->size = this->size - 1;
}

void vector_clear(vector *this)
{
    size_t i;
    for (i = 0; i < this->size; i++)
    {
        if (this->array[i])
        {
            this->destructor(this->array[i]);
            this->array[i] = NULL;
        }
    }
    this->size = 0;
}
