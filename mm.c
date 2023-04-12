#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "team06",
    /* First member's full name */
    "Seunghun Shin",
    /* First member's email address */
    "seunghunshin1284@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE  4   /* Word and header/footer size (bytes) */
#define DSIZE  8   /* Double word size (bytes)*/
#define CHUNKSIZE  (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc)  ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

#define GET_ADDRESS(p) (*(char *)(p))
#define PUT_ADDRESS(p, val)  (*(char *)(p) = (char*)(val))

#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE((void *)(bp) - DSIZE))

/*Pointer to get NEXT and PREV pointer of free_list*/
#define PREV_PTR(p)  (*(char **)(p + WSIZE))
#define NEXT_PTR(p)  (*(char **)(p))

#define SEG_MAX_SIZE 10

char* heap_listp = 0;

// Segregated free list
static char* seg_list[SEG_MAX_SIZE];  

static void *coalesce(void *bp);
static void *extend_heap(size_t words);

void free_list_insert(void *ptr); // find index based on the size of block -> insert
void free_list_delete(void *ptr); // delete the free block

int mm_init(void){
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                              // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));     // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));     // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));         // Epilogue header
    heap_listp += 2*WSIZE;
    
    for (int i=0; i<SEG_MAX_SIZE; i++)
        seg_list[i] = heap_listp;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));             // Free block header
    PUT(FTRP(bp), PACK(size, 0));             // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));     // New epilogue header
    
    return coalesce(bp);
}

int find_free_idx(size_t size){
    for (int idx = 0; idx<=8; idx++) {
        if (size <= (1 << (idx+4)))         // idx : 0 = 2**4 ~ idx : 9 = 2**12
            return idx;                     
    }
    return SEG_MAX_SIZE - 1;        // index of 2**12 ~ inf
}

static void* find_fit(size_t asize) {
    int idx = find_free_idx(asize);
    void* bp;

    while(idx < SEG_MAX_SIZE){
        for(bp = seg_list[idx]; GET_ALLOC(bp) != 1; bp = NEXT_PTR(bp)){
            if(GET_SIZE(HDRP(bp)) >= asize)
                return bp;
            }
        idx++;
    }
    return NULL;
}

void free_list_insert(void* ptr){
    size_t size = GET_SIZE(HDRP(ptr));
    int idx = find_free_idx(size);

    NEXT_PTR(ptr) = seg_list[idx];
    if(seg_list[idx] != heap_listp)
        PREV_PTR(seg_list[idx]) = ptr;
    seg_list[idx] = ptr;

}

void free_list_delete(void* ptr){
    size_t size = GET_SIZE(HDRP(ptr));
    int idx = find_free_idx(size);

    // Delete - Case 1 : Free block linked with root
    if (seg_list[idx] == ptr){      
        seg_list[idx] = NEXT_PTR(ptr);
    }

    // Delete - Case 2 : Free block not linked with root
    else{   
        NEXT_PTR(PREV_PTR(ptr)) = NEXT_PTR(ptr);
        if (NEXT_PTR(ptr) != heap_listp){
            PREV_PTR(NEXT_PTR(ptr)) = PREV_PTR(ptr);
        }
    }

}

void place(void* bp, size_t asize){
    size_t oldsize = GET_SIZE(HDRP(bp));
    
    free_list_delete(bp);       // Find proper block -> delete free block in seg list

    if ((oldsize - asize) >= 2*DSIZE){          
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(oldsize-asize, 0));
        PUT(FTRP(bp), PACK(oldsize-asize, 0));

        coalesce(bp);
    }
    else{
        PUT(HDRP(bp), PACK(oldsize, 1));
        PUT(FTRP(bp), PACK(oldsize, 1));
    }

}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {                 // case 1
        free_list_insert(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc){            // case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        free_list_delete((NEXT_BLKP(bp)));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc){            // case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        free_list_delete((PREV_BLKP(bp)));

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else{                                           // case 4
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));
        free_list_delete((PREV_BLKP(bp)));
        free_list_delete((NEXT_BLKP(bp)));

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    free_list_insert(bp);
    return bp;
}

void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    if (size == 0)
        return NULL;

    /* Adjust block size to include header + payload + footer */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. extend and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;

}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    coalesce(bp);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;

}