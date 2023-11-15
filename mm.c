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
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<10)

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc)  ((size) | (alloc))

#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
 
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p)(GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


// Declaration
static void *heap_listp;
static void *last_bp;

int mm_init(void);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *next_fit(size_t asize);
static void place(void *bp, size_t asize);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    // heap_listp는 void였기 때문에 last_bp에 맞게 char형으로 변환
    last_bp = (char *)heap_listp; 
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        last_bp = bp;
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    last_bp = bp;
    return bp;
}

// next_fit: 최근에 할당된 블록 다음부터 검색을 시작하여 적합한 블록을 찾는 함수
static void *next_fit(size_t asize) 
{
    // 최근에 할당된 블록의 위치를 가리키는 포인터
    void *bp = last_bp; 

    // bp를 다음 블록부터 힙의 끝까지 순회
    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp)) {
        // 현재 블록이 할당되지 않았고, 그 크기가 요청된 크기 이상인지 확인
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            // last_bp를 현재 블록으로 업데이트
            last_bp = bp;
            return bp;
        }
    }

    // 힙의 시작 위치(heap_listp)로 설정
    bp = heap_listp;
    // bp가 최근에 할당된 블록(last_bp) 이전을 가리킬 때까지 반복
    while (bp < last_bp) {
        //  bp를 다음 블록으로 이동
        bp = NEXT_BLKP(bp);
        // 현재 블록이 할당되지 않았고, 그 크기가 요청된 크기 이상인지 확인
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            // last_bp를 현재 블록으로 업데이트
            last_bp = bp;
            return bp;
        }
    }
    // 요청된 크기의 빈 블록을 찾지 못했으면 NULL
    return NULL;
}


// 주어진 블록(bp)에 주어진 크기(asize)의 블록을 할당하는 함수
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    //요청된 크기를 할당한 후에도 새로운 블록을 생성할 수 있는지를 체크
    if ((csize - asize) >= (2*DSIZE)) {
        // 요청된 크기의 블록을 할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // 남은 공간에 새로운 블록을 생성
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    // 요청된 크기를 할당한 후에 충분한 공간이 남지 않는다면, 현재 블록 전체를 요청된 크기로 할당
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if ((bp = next_fit(asize)) != NULL) {
        place(bp, asize);
        last_bp = bp;
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    last_bp = bp;
    return bp;
}

/*
 * mm_realloc - 메모리 블록의 크기를 재조정하는 데 사용되며, 필요에 따라 메모리 블록의 병합이나 새로운 블록의 할당을 수행
 */
void *mm_realloc(void *bp, size_t size)
{
    size_t old_size = GET_SIZE(HDRP(bp));
    size_t new_size = size + (2 * WSIZE);
    
    // 요청된 새로운 크기가 기존 블록의 크기보다 작거나 같으면, 블록의 크기 변경할 필요 X
    if (new_size <= old_size) {
        return bp;
    }
    else {
        // 인접한 다음 블록(NEXT_BLKP(bp))이 할당되어 있는지(next_alloc) 확인
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        // 현재 블록과 인접한 다음 블록의 크기 합(current_size)가 new_size보다 크거나 같은지 확인
        size_t current_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));

        // 인접한 다음 블록이 할당되지 않았고, 두 블록의 크기 합이 new_size보다 크거나 같다면
        if (!next_alloc && current_size >= new_size) {
            // 새로운 블록의 헤더와 푸터를 업데이트
            PUT(HDRP(bp), PACK(current_size, 1));
            PUT(FTRP(bp), PACK(current_size, 1));
            return bp;
        }
        // new_size크기의 새로운 블록을 할당, 새로운 블록에 데이터를 저장(place),
        // 기존 블록의 데이터를 새로운 블록으로 복사(memcpy), 기존 블록을 해제(mm_free), 새로운 블록의 포인터 반환
        else {
            void *new_bp = mm_malloc(new_size);
            place(new_bp, new_size);
            memcpy(new_bp, bp, new_size);
            mm_free(bp);
            return new_bp;
        }
    }
}














