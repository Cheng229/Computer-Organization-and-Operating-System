#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "heapAlloc.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {
        int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    *
    * End Mark:
    *  The end of the available memory is indicated using a size_status of 1.
    *
    * Examples:
    *
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    *
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */

blockHeader *heapStart = NULL;
blockHeader *lastAlloc = NULL;

int allocSize = 0;//size of all allocated blocks
int const headerSize = 4;//size of header part
int totalMemSize = 0;// the size of total memory which is available
/*
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
  void* allocHeap(int size) {
      //check if it is the first allocHeap call
      if (lastAlloc == NULL){//this is the first allocHeap call
        lastAlloc = heapStart;//initialize lastAlloc
        totalMemSize = heapStart->size_status;//compute total memory size
        //set lastAlloc size_status to 0 for allocating this block
        lastAlloc->size_status = 0;
      }
      //check if the allocating size is too small(<=0) or big to fit
      if (size <= 0 || size > totalMemSize - 6){
        return NULL;
      }
      //check if the total allocated size is out of heap range
      if (allocSize + size > totalMemSize - 6){
        return NULL;
      }
      //compute the size of allocating block
      int paddingSize = 0;//size of padding part
      if (((size + 4) % 8)== 0){//no need for padding
        paddingSize = 0;
      }else{//need a padding to reach 8 multiple size
        paddingSize = 8 * (((size + 4) / 8) + 1) - headerSize - size;
      }
      //size is the payloadSize
      int payloadSize = size;
      //total block size(the block to be allocated)
      int blockSize = headerSize + paddingSize + payloadSize;
      //assign current block to last allocated address
      blockHeader *curBlock = lastAlloc;
      //check if it is the first allocaion
      if (curBlock->size_status == 0){
        //if it is the first allocation, change the size status of the
        //current block to denote the whole heap size
        curBlock->size_status = (totalMemSize - 2) | 2;
      }
      int continueLoop = 1;//whether to continue the while Loop
      while (continueLoop == 1){
        //enter loop to allocate new block
        if ((curBlock->size_status & 1) == 0){//the current block is freed
          //compute the space of this current block
          //clear p bit of current block to 0 to compute the size of curBlock
          int curSpace = curBlock->size_status & (curBlock->size_status ^ 2);
          //compute difference between curspace and blockSize
          int diff = curSpace - blockSize;
          if (diff == 0){//current block can be fitted, has enough space
            //increase 1 at a bit to denote this block is assigned
            curBlock->size_status =curBlock->size_status | 1;
            //search the next block header
            blockHeader* nextHeader =
             (blockHeader*)((char*)curBlock + (curBlock->size_status & (~3)));
            //check if the next block is the end of heap
            if (nextHeader->size_status != 1){
              //not the heap end, update the next block header p bit to 1
              nextHeader->size_status = nextHeader->size_status | 2;
            }
            //update the last allocated block to current block
            lastAlloc = curBlock;
            //increase the allocated size
            allocSize = allocSize + blockSize;
            return (blockHeader*)((char*)curBlock + 4);
          }else if (diff > 0){//more empty space, need to split
            //printf("%i %i\n",curBlock->size_status,curBlock->size_status & 3);
            //update the allocating block size
            curBlock->size_status = blockSize + (curBlock->size_status & 3);
            //increase 1 at a bit (to be allocated)
            curBlock->size_status =curBlock->size_status | 1;
            //search the next block header
            blockHeader* nextHeader =
             (blockHeader*)((char*)curBlock + (curBlock->size_status & (~3)));
            //assign next block with the size of diff we computed
            nextHeader->size_status = diff;
            //update the p bit of next block header to 1 (previous allocated)
            nextHeader->size_status = nextHeader->size_status | 2;
            //after split, next block is freed, add footer
            blockHeader* nextFooter = (blockHeader*)((char*)nextHeader +
                                      (nextHeader->size_status & (~3)) - 4);
            //set footer value
            nextFooter->size_status = nextHeader->size_status & (~3);
            //update lastAlloc address to current block
            lastAlloc = curBlock;
            allocSize = allocSize + blockSize;
            return (blockHeader*)((char*)curBlock + 4);
          }else {//not enough space
            //space is not enough, search next block
            blockHeader* nextHeader =
             (blockHeader*)((char*)curBlock + (curBlock->size_status & (~3)));
            if (nextHeader == lastAlloc){//no enough space in the heap
              return NULL;
            }else{//continue search
              if (nextHeader->size_status != 1){//next block is not heap end
                curBlock = nextHeader;
              }else{//next block is heap end
                curBlock = heapStart;
              }
            }
          }
        }else{//this block is allocated, move to next block header
          //search next block header
          blockHeader* nextHeader =
           (blockHeader*)((char*)curBlock + (curBlock->size_status & (~3)));
          //check if the heap is iterated once
          if (nextHeader == lastAlloc){
            return NULL;
          }else{//move to the next block
            if (nextHeader->size_status != 1){//next block is not heap end
              curBlock = nextHeader;
            }else{//next block is heap end
              curBlock = heapStart;
            }
          }
        }
      }
      return NULL;
  }

/*
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */
int freeHeap(void *ptr) {
    if (ptr == NULL){//invalid ptr parameter
      return -1;
    }
    if ((int)ptr % 8 != 0){//check if ptr is a multiple of 8
      return -1;
    }
    //check if the ptr is out of heap range
    if ((blockHeader*)ptr >= (blockHeader*)((char*)heapStart + totalMemSize - 2)
        || (blockHeader*)ptr < heapStart ){
      return -1;
    }
    //set a header for ptr block
    blockHeader* ptrHead = (blockHeader*)((char*)ptr - 4);
    //check if it is a freed block
    if ((ptrHead->size_status & 1) == 0){
      return -1;
    }
    //set a variable to check if ptr is at lastAlloc address
    int iflast = 0;
    if (ptrHead == lastAlloc){
      iflast = 1;
    }
    //Then This block is allocated
    //define a footer for this block
    blockHeader* ptrFoot =
     (blockHeader*)((char*)ptrHead + (ptrHead->size_status & ~3) - 4);
    //mark this block to be freed
    ptrHead ->size_status = ptrHead->size_status & ~1;
    ptrFoot->size_status = ptrHead->size_status & ~3;
    //update the allocated size
    allocSize = allocSize - (ptrHead->size_status & ~3);


    //if ptr has previous block
    //if previous block is free then coalesce previous block and current block
    //Then update previous block header and current footer
    if (ptrHead != heapStart){
      if ((ptrHead->size_status & 2) == 0){//previous block is freed
          //find the previous block header and footer
          blockHeader*preFooter = (blockHeader*)((char*) ptrHead - 4);
          blockHeader*preHeader =
           (blockHeader*)((char*)preFooter - preFooter->size_status + 4);
          //update this block's p bit to 1 (previous is allocated)
          preHeader->size_status =
           ((preHeader->size_status & ~3) + (ptrHead->size_status & ~3)) | 2;
          //move ptrheader to previous header for coalesce
          ptrHead = preHeader;
          ptrFoot->size_status = ptrHead->size_status & ~3;
          //if last allocated block is coalesced, move lastAlloc to previous
          if (iflast == 1){
            lastAlloc = ptrHead;
          }
      }
    }

    //if ptr has nextHeader
    //if next block is free then coalesce next block and current block
    //Then update current block header and next footer
    blockHeader*nextHeader =(blockHeader*)((char*)ptrFoot + 4);
    if (nextHeader->size_status != 1){
      nextHeader->size_status = nextHeader->size_status & ~2;//update p bit
      if ((nextHeader->size_status & 1) == 0){//next block is freed
        blockHeader*nextFooter =
        (blockHeader*)((char*) nextHeader + (nextHeader->size_status & ~3) - 4);
        //coalesce
        //move footer to next footer
        ptrFoot = nextFooter;
        //update the freed size
        ptrHead->size_status =
         ((ptrHead->size_status & ~3) + (nextHeader->size_status & ~3)) | 2;
        //update footer value
        nextFooter->size_status = ptrHead->size_status & ~3;
      }
    }
    return 0;
}

/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int initHeap(int sizeOfRegion) {

    static int allocated_once = 0; //prevent multiple initHeap calls

    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    int allocsize; // size of requested allocation including padding
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;

    if (0 != allocated_once) {
        fprintf(stderr,
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((char*)heapStart + allocsize - 4);
    footer->size_status = allocsize;

    return 0;
}

/*
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts)
 * t_End    : address of the last byte in the block
 * t_Size   : size of the block as stored in the block header
 */
void dumpMem() {

    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");

    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;

        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used)
            used_size += t_size;
        else
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status,
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;
}
