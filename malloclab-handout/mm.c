/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define INDEX_SIZE 14
#define MAX_SIZE 4096
#define WSIZE 4
#define DSIZE 8

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned long*) (p))
#define PUT(p, val) (*(unsigned long*) (p) = val)

#define ADDRESSP(op) ((address)(op) + 3 * WSIZE)

#define HDRP(bp) ((address)(bp) - 3 * WSIZE)
#define FTRP(bp) ((address)(bp) + GET_SIZE(bp) - 4 * WSIZE)
#define PREV_BLKP(bp) ((address)(bp) - 2 * WSIZE)
#define NEXT_BLKP(bp) ((address)(bp) - WSIZE)
#define LEFT_BLKHDRP(bp) (HDRP(bp) - GET_SIZE(bp - WSIZE))
#define RIGHT_BLKHDRP(bp) (HDRP(bp) + GET_SIZE(bp))

#define GET_SIZE(p) (*(unsigned long*)(HDRP(p)) & ~0x7)
#define GET_ALLOC(p) (*(unsigned long*)(HDRP(p)) & 0x1)
#define GET_BLK_SIZE(size) (ALIGN(size + 4 * WSIZE))


typedef char *address;
static address* array_head;
static address heap_head;


int mm_check(void) {
    // check all used memory
    printf("check memory\n");
    for (address start = heap_head;
        start < (address)mem_heap_lo();
        start = ADDRESSP(RIGHT_BLKHDRP(start))
        ) {
        printf("%p\t%lu\t%lu\n", start, GET_ALLOC(start), GET_SIZE(start));
    }


    for (unsigned int start_index = 0;
        start_index < INDEX_SIZE;
        ++start_index) {
        for (address head = array_head[start_index]; head != NULL; head = *(address *)(NEXT_BLKP(head))){
            printf("[%u]\t%p\t%lu\t%lu\n", start_index, head, GET_ALLOC(head), GET_SIZE(head));
        }
    }
    return 0;
}


/*
return the proper index for given size
*/
unsigned int get_index(size_t size) {
    unsigned int count = 0;
    while ((size >>= 1)) {
        ++count;
    }
    return count;
}

/*
help function for create new block.
*/
address create_blk(void *p, size_t blk_size, unsigned int allocated, address prev, address next) {
    address address_p = ADDRESSP(p);
    // set head
    PUT(HDRP(address_p), PACK(blk_size, allocated));
    // set prev
    PUT(PREV_BLKP(address_p), (unsigned long)prev);
    // set next
    PUT(NEXT_BLKP(address_p), (unsigned long)next);
    // set foot
    PUT(FTRP(address_p), PACK(blk_size, allocated));

    if (!allocated) {
        // insert into free list
        unsigned int free_index = get_index(blk_size);

        if (array_head[free_index]) {
            PUT(NEXT_BLKP(address_p), (unsigned long)array_head[free_index]);
            PUT(PREV_BLKP(array_head[free_index]), (unsigned long)address_p);
        }
        array_head[free_index] = address_p;
    }
    return address_p;
}

/*
remove the address from free list
*/
int remove_free_list(address p) {
    if (GET_ALLOC(p)) {
        return -1;
    }

    address prev = *(address*)PREV_BLKP(p);
    address next = *(address*)NEXT_BLKP(p);
    unsigned int free_index = get_index(GET_SIZE(p));
    if (!prev && !next) {
        // free head
        array_head[free_index] = NULL;
    } else if (!prev && next) {
        array_head[free_index] = next;
    } else if (prev && !next) {
        PUT(NEXT_BLKP(prev), 0);
    } else {
        PUT(NEXT_BLKP(prev), (unsigned long)next);
        PUT(PREV_BLKP(next), (unsigned long)prev);
    }
    return 0;
}

/*
coalescing four cases and return the new block address
*/
address coalescing(address p) {
    address left_p = ADDRESSP(LEFT_BLKHDRP(p));
    address right_p = ADDRESSP(RIGHT_BLKHDRP(p));
    unsigned long prev_alloc = GET_ALLOC(left_p);
    unsigned long next_alloc = GET_ALLOC(right_p);
    if (!prev_alloc && next_alloc) {
        // coalescing previous block
        remove_free_list(left_p);
        create_blk(HDRP(left_p), GET_SIZE(left_p) + GET_SIZE(p), 0, NULL, NULL);
        p = left_p;
    } else if (prev_alloc && !next_alloc) {
        // coalescing next block
        remove_free_list(right_p);
        create_blk(HDRP(p), GET_SIZE(right_p) + GET_SIZE(p), 0, NULL, NULL);
    } else if (!prev_alloc && !next_alloc) {
        // coalescing all block
        remove_free_list(left_p);
        remove_free_list(right_p);
        create_blk(HDRP(left_p), GET_SIZE(left_p) + GET_SIZE(p) + GET_SIZE(right_p), 0, NULL, NULL);
        p = left_p;
    } else {
        create_blk(HDRP(p), GET_SIZE(p), 0, NULL, NULL);
    }
    return p;
}


address extend_heap(size_t words) {
    address bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE: words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // make bp point to head of block
    bp = create_blk(bp - WSIZE, size, 1, NULL, NULL);

    address epilogue = (address)mem_heap_hi() - 3;
    PUT(epilogue, PACK(0, 1));
    // mm_check();
    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    array_head = mem_sbrk((INDEX_SIZE + 6) * sizeof(address));
    if (!array_head) {
        return -1;
    }
    memset(array_head, 0, (INDEX_SIZE + 1) * sizeof(address));

    // set prologue and epilogue
    heap_head = (address)(array_head + INDEX_SIZE + 4);

    // prologue head
    PUT(array_head + INDEX_SIZE + 1, PACK(16, 1));
    // prologue prev
    PUT(array_head + INDEX_SIZE + 2, 0);
    // prologue next
    PUT(array_head + INDEX_SIZE + 3, 0);
    // prologue foot
    PUT(array_head + INDEX_SIZE + 4, PACK(16, 1));
    // epilogue head
    PUT(array_head + INDEX_SIZE + 5, PACK(0, 1));
    return 0;
}



/*
help function for split new blocks from old block.
old block is a free block.
*/
address split(address p, size_t split_size) {
    size_t ori_size = GET_SIZE(p);
    size_t left_size = ori_size - split_size;
    if (left_size <= 4 * WSIZE) {
        // the space left is less than empty block, we should not split it
        create_blk(HDRP(p), GET_SIZE(p), 1, NULL, NULL);
        return p;
    }

    // create split block
    create_blk(HDRP(p), split_size, 1, NULL, NULL);

    address split_p = p + split_size;
    create_blk(HDRP(split_p), left_size, 0, NULL, NULL);
    return p;
};

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t blk_size = GET_BLK_SIZE(size);
    // iter all block head
    // unsigned int start_index = get_index(blk_size);
    // printf("size: %u, allocate size: %u, which in index: %u\n", size, blk_size, start_index);
    for (unsigned int start_index = get_index(GET_BLK_SIZE(size));
        start_index < INDEX_SIZE;
        ++start_index) {

        // first fit: to find proper block for request size
        for (address head = array_head[start_index]; head != NULL; head = *(address*)NEXT_BLKP(head)){
            if (GET_SIZE(head) >= blk_size) {
                // remove it from free linked list
                if (*(address *)PREV_BLKP(head)) {
                    PUT(NEXT_BLKP(*(address*)PREV_BLKP(head)), *(unsigned long*)NEXT_BLKP(head));
                    if (*(address *)NEXT_BLKP(head)) {
                        PUT(PREV_BLKP(*(address*)NEXT_BLKP(head)), *(unsigned long*)PREV_BLKP(head));
                    }
                } else {
                    array_head[start_index] = *(address *)NEXT_BLKP(head);
                }
                // split it and allocate
                return (void *)split(head, blk_size);
            }
        }
    }

    // we have to request new extra space
    address p = extend_heap(MAX(blk_size, MAX_SIZE) / WSIZE);
    return (void *)split(p, blk_size);
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // printf("free ptr: %p\n", ptr);
    if (ptr) {
        // coalescing block
        ptr = coalescing(ptr);
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














