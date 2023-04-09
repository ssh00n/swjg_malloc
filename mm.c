#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* Basic constants and macros: */

#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros*/
#define WSIZE  4   /* Word and header/footer size (bytes) */
#define DSIZE  8   /* Double word size (bytes)*/
#define CHUNKSIZE  (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

// /* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

// /* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks. */
//#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define NEXT_BLKP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)))
//#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE((void *)(bp) - DSIZE))

/* Global variables: */
static char *heap_listp=0; /* Pointer to first block */  

/* Function prototypes for heap consistency checker routines: */
static void checkblock(void *bp);
// static void checkheap(bool verbose);
static void printblock(void *bp);

/*myMacros */
/*Pointer to get NEXT and PREVIOUS pointer of free_list*/
#define NEXT_PTR(p)  (*(char **)(p + WSIZE))
#define PREV_PTR(p)  (*(char **)(p))


/* myVariables */
// Pointer pointing to starting of explicit free list
static char* freeListPtr=0;

static char* heap_listp = NULL;

int mm_init(void); // initializes the heaplist and initialized the freeListPtr

void *mm_malloc(size_t size); // allocates the requested size that is require by the payload. Adding header, 
            // footer and alignment according to DSIZE, Find fit is called to find a suitable
            // block and if it returns successfully, allocate the block and remove it from free list.
            // Else, extend the heap size by calling extend_heap

void mm_free(void *bp); // frees the allocated block by setting the allocated bit to zero, editing the
                        // HEADER and FOOTER and coalescing the free blocks.

void *mm_realloc(void *ptr, size_t size); // reallocates the current block according the new size 
                                          // requirements.

static void *coalesce(void *bp); // performs boundary tag coalescing checking all possible cases. returns the address of the coalesced block.

static void *extend_heap(size_t words); // This is used to extend the existing heap if find_fit() fails. It 
                            // extends the heapsize by (extendsize/ WSIZE) words.

static void *find_fit(size_t asize); // finds a fit for a block with "asize" bytes. Returns that blocks's address or NULL if no suitable block was found.

static void place(void *bp, size_t asize); // calculates the current size of the block. If the diff between csize and requested size > 16 bytes
              // -> then it splits the block into two. The address of the original block is removed from
              // the free list. The first part of the old block is made allocated while the second part is 
              // made free and its address is stored in the free list. If diff < 16:
              // just removes the address of the block from the free list and makes the block allocated

void free_list_add(); // this adds the free block to the free_list according to the LIFO policy.
void free_list_delete(); // this removes the free block from the free_list


void* root;

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return 0;
    
    PUT(heap_listp, 0);
    PUT(heap_listp+WSIZE, PACK(DSIZE, 1));                  // prologue header 
    PUT(heap_listp+WSIZE*2, PACK(DSIZE, 1));                // prologue footer
    PUT(heap_listp+WSIZE*3, PACK(0, 1));                    // epilogue header
    
}

