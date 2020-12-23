#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/******************************************************************************/
/* DO NOT MODIFY THESE VARIABLES **********************************************/

//Globals set by command line args.
int b = 0; //number of block (b) bits
int s = 0; //number of set (s) bits
int E = 0; //number of lines per set

//Globals derived from command line args.
int B; //block size in bytes: B = 2^b
int S; //number of sets: S = 2^s

//Global counters to track cache statistics in access_data().
int hit_cnt = 0;
int miss_cnt = 0;
int evict_cnt = 0;

//Global to control trace output
int verbosity = 0; //print trace if set
/******************************************************************************/


//Type mem_addr_t: Use when dealing with addresses or address masks.
typedef unsigned long long int mem_addr_t;

//Type cache_line_t: Use when dealing with cache lines.
//TODO - COMPLETE THIS TYPE
typedef struct cache_line {
    char valid;
    mem_addr_t tag;
    int counter;//a counter represent the least recent of lines
    //Add a data member as needed by your implementation for LRU tracking.
} cache_line_t;

//Type cache_set_t: Use when dealing with cache sets
//Note: Each set is a pointer to a heap array of one or more cache lines.
typedef cache_line_t* cache_set_t;
//Type cache_t: Use when dealing with the cache.
//Note: A cache is a pointer to a heap array of one or more sets.
typedef cache_set_t* cache_t;

// Create the cache (i.e., pointer var) we're simulating.
cache_t cache;

/*
 * init_cache:
 * Allocates the data structure for a cache with S sets and E lines per set.
 * Initializes all valid bits and tags with 0s.
 */
void init_cache() {
  S = 1<<s;//get the number of sets, which is 2^s
  cache = malloc(S * sizeof(cache_set_t));//allocate memory for sets of cache
  if (cache == NULL){//chech if it is allocated successfully
    printf("%s\n", "Cannot allocate a cache" );
    exit(1);
  }
  for (int i = 0; i < S; i++){//allocate memory for line of set
    cache[i] = malloc(E * sizeof(cache_line_t));
    if (cache[i] == NULL){//chech if it is allocated successfully
      printf("%s\n", "Cannot allocate a set" );
      exit(1);
    }
    for (int j = 0; j < E; j++){//initialize those struct fields
      cache[i][j].valid = 0;
      cache[i][j].tag = -1;
      cache[i][j].counter = 0;
    }
  }
}


/*
 * free_cache:
 * Frees all heap allocated memory used by the cache.
 */
void free_cache() {
  for (int i = 0; i < S; i++){//loop to free all sets of cache
      free(cache[i]);
  }
  //free the cache
  free(cache);
  cache = NULL;
}


/*
 * access_data:
 * Simulates data access at given "addr" memory address in the cache.
 *
 * If already in cache, increment hit_cnt
 * If not in cache, cache it (set tag), increment miss_cnt
 * If a line is evicted, increment evict_cnt
 */
void access_data(mem_addr_t addr) {
  //compute the number of set of this addr in the cache
  int setIndex = (addr>>b) %(int)pow(2,s);
  int s_b_bit = s + b;//the number of s bits and b bits for future use
  int tagNum = addr/pow(2,s_b_bit);//obtain the number of tag
  int found = 0;//check if this addr is found in cache
  int full = 1;//check if the set is full
  for(int j = 0; j < E; j++){
    if (cache[setIndex][j].valid == 0){
      full = 0;//this set is not full
    }
  }
  //find the least counter and max counter
  int least = cache[setIndex][0].counter;//initialize least to first element
  int leastIndex = 0;//record the first index which is 0
  int max = cache[setIndex][0].counter;//initialize max counter to first element
  for (int j = 1; j < E; j++){
    if (cache[setIndex][j].counter < least){
      //find less counter
      least = cache[setIndex][j].counter;//record this less counter
      leastIndex = j;//record its index
    }
    if (cache[setIndex][j].counter > max){
      //find larger counter
      max = cache[setIndex][j].counter;//record the larger counter
    }
  }

  //match tag
  for (int j = 0; j < E; j++){
    if (cache[setIndex][j].tag == tagNum){//find the tag
      found = 1;//found this tag match
      //check valid bit
      if ((int)cache[setIndex][j].valid == 1){
        hit_cnt++;//hit
        cache[setIndex][j].counter = max + 1;//update the counter
      }
      else{//valid == 0
        miss_cnt++;//miss because of the 0 valid bit
        cache[setIndex][j].valid = 1;//update valid bit
        cache[setIndex][j].counter = max + 1;//update the counter
      }
    }
  }
  //no tag match for this set
  if (found == 0){
    miss_cnt++;//miss because of not found tag match
    //check if the set is full
    if (full == 0){//this set is not full
      for (int j = 0; j < E; j++){
        if (cache[setIndex][j].valid == 0){//set tag
          cache[setIndex][j].valid = 1;//update valid bit
          cache[setIndex][j].tag = tagNum;//set tag
          cache[setIndex][j].counter = max + 1;//update the counter
          break;//set a line tag randomly and break
        }
      }
    }
    else{//if the set is full
      evict_cnt++;//eviction because of the replacement
      cache[setIndex][leastIndex].tag = tagNum;//set tag
      cache[setIndex][leastIndex].counter = max + 1;//update the counter
    }
  }
}

/*
 * replay_trace:
 * Replays the given trace file against the cache.
 *
 * Reads the input trace file line by line.
 * Extracts the type of each memory access : L/S/M
 * TRANSLATE each "L" as a load i.e. 1 memory access
 * TRANSLATE each "S" as a store i.e. 1 memory access
 * TRANSLATE each "M" as a load followed by a store i.e. 2 memory accesses
 */
void replay_trace(char* trace_fn) {
    char buf[1000];
    mem_addr_t addr = 0;
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if (!trace_fp) {
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while (fgets(buf, 1000, trace_fp) != NULL) {
        if (buf[1] == 'S' || buf[1] == 'L' || buf[1] == 'M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);

            if (verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);

            //if the character is a load followed by a store
            if(buf[1] == 'M'){
              access_data(addr);
              access_data(addr);
            }
            //if the character is a load or a store
            if(buf[1] == 'S' || buf[1] == 'L' ){
              access_data(addr);
            }
            // GIVEN: 1. addr has the address to be accessed
            //        2. buf[1] has type of acccess(S/L/M)
            // call access_data function here depending on type of access

            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}


/*
 * print_usage:
 * Print information on how to use csim to standard output.
 */
void print_usage(char* argv[]) {
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of s bits for set index.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of b bits for block offsets.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}


/*
 * print_summary:
 * Prints a summary of the cache simulation statistics to a file.
 */
void print_summary(int hits, int misses, int evictions) {
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}


/*
 * main:
 * Main parses command line args, makes the cache, replays the memory accesses
 * free the cache and print the summary statistics.
 */
int main(int argc, char* argv[]) {
    char* trace_file = NULL;
    char c;

    // Parse the command line arguments: -h, -v, -s, -E, -b, -t
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (c) {
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'h':
                print_usage(argv);
                exit(0);
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                print_usage(argv);
                exit(1);
        }
    }

    //Make sure that all required command line args were specified.
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        print_usage(argv);
        exit(1);
    }

    //Initialize cache.
    init_cache();

    //Replay the memory access trace.
    replay_trace(trace_file);

    //Free memory allocated for cache.
    free_cache();

    //Print the statistics to a file.
    //DO NOT REMOVE: This function must be called for test_csim to work.
    print_summary(hit_cnt, miss_cnt, evict_cnt);
    return 0;
}
