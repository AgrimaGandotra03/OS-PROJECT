/*
 * allocator.h - Dynamic Memory Allocation Simulator
 *
 * Simulates how an operating system hands out chunks of memory to processes
 * using First Fit, Best Fit, Worst Fit and Next Fit.
 *
 * Two memory models are supported:
 *   - Dynamic (variable) partitions: one big region that is split on
 *     allocation and merged (coalesced) when processes leave.
 *   - Fixed partitions: a pre-defined list of blocks; a process takes a whole
 *     block, so unused space inside it is internal fragmentation.
 */
#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#define MAX_FIXED_BLOCKS 64

typedef enum {
    FIRST_FIT = 0,
    BEST_FIT,
    WORST_FIT,
    NEXT_FIT,
    STRATEGY_COUNT
} Strategy;

/* Return codes of mem_allocate() */
enum {
    ALLOC_OK = 1,
    ALLOC_NOFIT = 0,
    ALLOC_DUP = -1,     /* a process with this ID is already in memory */
    ALLOC_BADSIZE = -2  /* size must be positive */
};

/* One block of memory: either free or owned by a process. */
typedef struct Block {
    int start;          /* first address (KB) */
    int size;           /* size of the block (KB) */
    int free;           /* 1 = free, 0 = allocated */
    int pid;            /* owning process, -1 when free */
    int req;            /* size the process actually asked for (KB) */
    struct Block *next;
} Block;

/* How the memory is laid out at start-up. */
typedef struct {
    int dynamic;                    /* 1 = dynamic partitions, 0 = fixed */
    int total;                      /* total memory in KB */
    int nfixed;                     /* number of fixed partitions */
    int fixed[MAX_FIXED_BLOCKS];    /* sizes of the fixed partitions */
} Config;

typedef struct {
    Config cfg;
    Block *head;        /* linked list of blocks, sorted by address */
    int rover;          /* address where Next Fit resumes its search */
    long searched;      /* total number of blocks examined so far */
    int alloc_ok;       /* successful allocations */
    int alloc_fail;     /* allocations that found no block */
} Memory;

typedef struct {
    int total;
    int used;               /* KB actually requested by running processes */
    int allocated_size;     /* KB of blocks handed out (>= used) */
    int free_total;         /* KB in free blocks */
    int holes;              /* number of free blocks */
    int largest_hole;       /* KB of the biggest free block */
    int internal_frag;      /* KB wasted inside allocated blocks */
    double utilization;     /* used / total, in percent */
    double external_frag;   /* 1 - largest_hole / free_total, in percent */
} Stats;

const char *strategy_name(Strategy s);

/* Set up memory from cfg. Returns 1 on success, 0 on invalid config. */
int  mem_init(Memory *m, const Config *cfg);
void mem_destroy(Memory *m);

/* Place process pid of the given size using strategy s. */
int  mem_allocate(Memory *m, Strategy s, int pid, int size);

/* Release the memory of process pid. Returns 1 if found, 0 otherwise. */
int  mem_free(Memory *m, int pid);

/* Slide all allocated blocks to the start (dynamic mode only). */
void mem_compact(Memory *m);

Stats mem_stats(const Memory *m);

void mem_print(const Memory *m);                 /* table of blocks */
void mem_print_bar(const Memory *m, int width);  /* one-line picture */

#endif /* ALLOCATOR_H */
