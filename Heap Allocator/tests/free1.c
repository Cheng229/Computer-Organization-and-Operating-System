// a few allocations in multiples of 4 bytes followed by frees
#include <assert.h>
#include <stdlib.h>
#include "heapAlloc.h"

int main() {
   assert(initHeap(4096) == 0);
   void* ptr[4];

   ptr[0] = allocHeap(4);
   ptr[1] = allocHeap(8);
   assert(freeHeap(ptr[0]) == 0);
   assert(freeHeap(ptr[1]) == 0);
   //
    ptr[2] = allocHeap(16);
    //printf("%s\n","A" );
    ptr[3] = allocHeap(4);
    //printf("%s\n","B" );
    assert(freeHeap(ptr[2]) == 0);
  //  printf("%s\n","C" );
    assert(freeHeap(ptr[3]) == 0);
    //printf("%s\n","D" );
   exit(0);
}
