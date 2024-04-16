/* 
 * mm-implicit.c -  Simple allocator based on implicit free lists, 
 *                  first fit placement, and boundary tag coalescing. 
 *
 * Each block has header and footer of the form:
 * 
 *      31                     3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap  
 *  -----------------------------------------------------------------   
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name */
  "Rodrigo Nicastro",
  /* First member's full name */
  "Rodrigo Franklin de Mello Nicastro",
  /* First member's email address */
  "rofr4171@colorado.edu",
  /* Second member's full name (leave blank if none) */
  "",
  /* Second member's email address (leave blank if none) */
  ""
};

/////////////////////////////////////////////////////////////////////////////
// Constants and macros
//
// These correspond to the material in Figure 9.43 of the text
// The macros have been turned into C++ inline functions to
// make debugging code easier.
//
/////////////////////////////////////////////////////////////////////////////
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

struct listNode{
  struct listNode* next;
  struct listNode* prev;
};

static inline int MAX(int x, int y) {
  return x > y ? x : y;
}

//
// Pack a size and allocated bit into a word
// We mask of the "alloc" field to insure only
// the lower bit is used
//
static inline uint32_t PACK(uint32_t size, int alloc) {
  return ((size) | (alloc & 0x1));
}

//
// Read and write a word at address p
//
static inline uint32_t GET(void *p) { return  *(uint32_t *)p; }
static inline void PUT( void *p, uint32_t val)
{
  *((uint32_t *)p) = val;
}

//
// Read the size and allocated fields from address p
//
static inline uint32_t GET_SIZE( void *p )  { 
  return GET(p) & ~0x7;
}

static inline int GET_ALLOC( void *p  ) {
  return GET(p) & 0x1;
}

//
// Given block ptr bp, compute address of its header and footer
//
static inline void *HDRP(void *bp) {

  return ( (char *)bp) - WSIZE;
}
static inline void *FTRP(void *bp) {
  return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

//
// Given block ptr bp, compute address of next and previous blocks
//
static inline void *NEXT_BLKP(void *bp) {
  return  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}

static inline void* PREV_BLKP(void *bp){
  return  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}

/////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//

static char *heap_listp;  /* pointer to first block */

struct listNode root;

//
// function prototypes for internal helper routines
//
static void *extend_heap(uint32_t words);
static void place(void *bp, uint32_t asize);
static void *find_fit(uint32_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

static void insertNode(struct listNode *newNode);
static void deleteNode(struct listNode *node);

static void insertNode(struct listNode* newNode){
  struct listNode* ptr = &root;

  newNode->next = ptr->next;
  newNode->prev = ptr;

  ptr->next = newNode;
  newNode->next->prev = newNode;
}

static void deleteNode(struct listNode *node){
  node->prev->next = node->next;
  node->next->prev = node->prev;

  node->next = NULL;
  node->prev = NULL;
}

//
// mm_init - Initialize the memory manager 
//
int mm_init(void) 
{
  if ((heap_listp = (char *)mem_sbrk(4*WSIZE)) == NULL) return -1;
      
  PUT(heap_listp, 0);
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3*WSIZE), PACK(0, 1));
  heap_listp += (2*WSIZE);

  root.next = &root;
  root.prev = &root;
  
  if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;

  return 0;
}


//
// extend_heap - Extend heap with free block and return its block pointer
//
static void *extend_heap(uint32_t words) 
{
  char *bp;
  uint32_t size;
  size = (words %2) ? (words+1) * WSIZE : words * WSIZE;
 
  bp = (char *) mem_sbrk(size);
  
  if(bp == (char *) -1) return NULL;
  
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  struct listNode* ptr = (struct listNode*)coalesce(bp);
  insertNode((struct listNode*)ptr);

  return (void*)ptr;
}


//
// Practice problem 9.8
//
// find_fit - Find a fit for a block with asize bytes 
//
static void *find_fit(uint32_t asize)
{
  struct listNode* bp;

  unsigned int minsize = MAX(DSIZE, sizeof(struct listNode)) + OVERHEAD;

  int best = 0;

  struct listNode* bestbp = NULL;
  struct listNode* rootPtr = &root;

  for(bp = rootPtr->next; bp != rootPtr; bp = bp->next){
    int delta = GET_SIZE(HDRP(bp)) - asize;
    if(delta >= 0){
      if(delta <= minsize){
        return bp;
      }

      else if(delta == best && bp < bestbp){
        bestbp = bp;
      }

      else if(best == 0 || delta < best){
        best = delta;
        bestbp = bp;
      }
    }
  }
  return bestbp;
}

// 
// mm_free - Free a block 
//
void mm_free(void *bp)
{
  uint32_t size = GET_SIZE(HDRP(bp));
    
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));

  struct listNode* ptr = (struct listNode*)coalesce(bp);
  insertNode((struct listNode*)ptr);

}

//
// coalesce - boundary tag coalescing. Return ptr to coalesced block
//
static void *coalesce(void *bp) 
{
    uint32_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    uint32_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    uint32_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc){
        return bp;
    }

    else if (prev_alloc && !next_alloc){
      deleteNode((struct listNode*)NEXT_BLKP(bp));

      size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
      PUT(HDRP(bp), PACK(size, 0));
      PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc){
      deleteNode((struct listNode*)PREV_BLKP(bp));

      size += GET_SIZE(HDRP(PREV_BLKP(bp)));
      PUT(FTRP(bp), PACK(size, 0));
      PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
      bp = PREV_BLKP(bp);
    }

    else{
      deleteNode((struct listNode*)NEXT_BLKP(bp));
      deleteNode((struct listNode*)PREV_BLKP(bp));

      size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
      PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
      PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
      bp = PREV_BLKP(bp);
    }

  return bp;
}

//
// mm_malloc - Allocate a block with at least size bytes of payload 
//
void *mm_malloc(uint32_t size) 
{
  void *bp;
  uint32_t asize;
  uint32_t extendsize;

  size = MAX(size, sizeof(struct listNode));

  if(size <= 0) return NULL;

  if(size <= DSIZE) asize = 2*DSIZE;

  else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
  if ((bp = find_fit(asize)) != NULL) {
    deleteNode((struct listNode*)bp);
    place(bp, asize);        
    return bp;
  }

  extendsize = MAX(asize,CHUNKSIZE);  
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;

  deleteNode((struct listNode*)bp);
  place(bp, asize);

  return bp;
} 

//
//
// Practice problem 9.9
//
// place - Place block of asize bytes at start of free block bp 
//         and split if remainder would be at least minimum block size
//
static void place(void *bp, uint32_t asize)
{
  unsigned int minSize = MAX(DSIZE, (sizeof(struct listNode) + OVERHEAD));

  uint32_t csize = GET_SIZE(HDRP(bp));

  int remainingSize = csize - asize;
  
  if ((remainingSize) >= minSize) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(remainingSize, 0));
    PUT(FTRP(bp), PACK(remainingSize, 0));
  
    insertNode((struct listNode*)bp);
  }

  else{
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

//
// mm_realloc -- implemented for you
//
void *mm_realloc(void *ptr, uint32_t size)
{
  unsigned int minSize = MAX(size, MAX(DSIZE, sizeof(struct listNode))) + OVERHEAD;

  size_t currBlockSize = GET_SIZE(HDRP(ptr));

  if(currBlockSize == minSize) return ptr;

  else if(currBlockSize > minSize){
    place(ptr, minSize);
    return ptr;
  } 

  else{
    void *newp;
    uint32_t copySize;

    newp = mm_malloc(size);
    if (newp == NULL) {
      printf("ERROR: mm_malloc failed in mm_realloc\n");
      exit(1);
    }
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize) {
      copySize = size;
    }
    memcpy(newp, ptr, copySize);
    mm_free(ptr);
    return newp;
  }
}

//
// mm_checkheap - Check the heap for consistency 
//
void mm_checkheap(int verbose) 
{
  //
  // This provided implementation assumes you're using the structure
  // of the sample solution in the text. If not, omit this code
  // and provide your own mm_checkheap
  //
  void *bp = heap_listp;
  
  if (verbose) {
    printf("Heap (%p):\n", heap_listp);
  }

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
	printf("Bad prologue header\n");
  }
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose)  {
      printblock(bp);
    }
    checkblock(bp);
  }
     
  if (verbose) {
    printblock(bp);
  }

  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
    printf("Bad epilogue header\n");
  }
}

static void printblock(void *bp) 
{
  uint32_t hsize, halloc, fsize, falloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));  
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));  
    
  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%d:%c] footer: [%d:%c]\n",
	 bp, 
	 (int) hsize, (halloc ? 'a' : 'f'), 
	 (int) fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
  if ((uintptr_t)bp % 8) {
    printf("Error: %p is not doubleword aligned\n", bp);
  }
  if (GET(HDRP(bp)) != GET(FTRP(bp))) {
    printf("Error: header does not match footer\n");
  }
}