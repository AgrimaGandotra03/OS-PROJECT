/*
 * main.c - menu-driven front end for the Dynamic Memory Allocation Simulator.
 *
 * Build:  gcc -Wall -Wextra -std=c99 -o memsim main.c allocator.c
 * Run:    ./memsim        (Windows: memsim.exe)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "allocator.h"

#define BAR_WIDTH 60

/* ------------------------------------------------------------------ */
/* Workloads: a list of "allocate pid size" / "free pid" events        */
/* ------------------------------------------------------------------ */

typedef struct {
    char type;      /* 'A' = allocate, 'F' = free */
    int pid;
    int size;
} Event;

typedef struct {
    Event *ev;
    int n, cap;
} Workload;

static void wl_add(Workload *w, char type, int pid, int size)
{
    if (w->n == w->cap) {
        w->cap = w->cap ? w->cap * 2 : 32;
        w->ev = realloc(w->ev, (size_t)w->cap * sizeof *w->ev);
        if (!w->ev) {
            fprintf(stderr, "Fatal: out of memory\n");
            exit(1);
        }
    }
    w->ev[w->n].type = type;
    w->ev[w->n].pid = pid;
    w->ev[w->n].size = size;
    w->n++;
}

static void wl_free(Workload *w)
{
    free(w->ev);
    w->ev = NULL;
    w->n = w->cap = 0;
}

/* ------------------------------------------------------------------ */
/* Input helpers                                                       */
/* ------------------------------------------------------------------ */

static void read_line(char *buf, size_t n)
{
    if (!fgets(buf, (int)n, stdin)) {
        printf("\nInput closed. Goodbye.\n");
        exit(0);
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *prompt, int min, int max)
{
    char buf[128];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        read_line(buf, sizeof buf);

        char *end;
        long v = strtol(buf, &end, 10);
        int no_digits = (end == buf);
        while (isspace((unsigned char)*end))
            end++;
        if (no_digits || *end != '\0' || v < min || v > max) {
            printf("  Please enter a whole number from %d to %d.\n", min, max);
            continue;
        }
        return (int)v;
    }
}

/* ------------------------------------------------------------------ */
/* Display helpers                                                     */
/* ------------------------------------------------------------------ */

static void describe_config(const Config *c, char *out, size_t n)
{
    if (c->dynamic)
        snprintf(out, n, "Dynamic partitions, %d KB", c->total);
    else
        snprintf(out, n, "Fixed partitions, %d blocks, %d KB", c->nfixed, c->total);
}

static void show_map(const Memory *m)
{
    mem_print(m);
    printf("\n  ");
    mem_print_bar(m, BAR_WIDTH);
    printf("  (digit = last digit of process ID, . = free)\n");
}

static void print_stats(const Memory *m)
{
    Stats s = mem_stats(m);
    printf("\n  Total memory         : %d KB\n", s.total);
    printf("  Memory in use        : %d KB (%.1f%%)\n", s.used, s.utilization);
    printf("  Free memory          : %d KB in %d hole(s)\n", s.free_total, s.holes);
    printf("  Largest free hole    : %d KB\n", s.largest_hole);
    printf("  External fragmentation: %.1f%%  (free memory not usable as one block)\n",
           s.external_frag);
    printf("  Internal fragmentation: %d KB  (wasted inside allocated blocks)\n",
           s.internal_frag);
    printf("  Allocations ok/failed: %d / %d\n", m->alloc_ok, m->alloc_fail);
    printf("  Blocks searched      : %ld\n", m->searched);
}

/* ------------------------------------------------------------------ */
/* Menu actions                                                        */
/* ------------------------------------------------------------------ */

static void configure(Config *cfg)
{
    printf("\n  1. Dynamic memory  (one big region, split and merged on demand)\n");
    printf("  2. Fixed partitions (you enter the block sizes)\n");
    printf("  3. Textbook example (fixed partitions: 100 500 200 300 600 KB)\n");
    int ch = read_int("  Choose: ", 1, 3);

    memset(cfg, 0, sizeof *cfg);

    if (ch == 1) {
        cfg->dynamic = 1;
        cfg->total = read_int("  Total memory size in KB (10-1000000): ", 10, 1000000);
    } else if (ch == 2) {
        char prompt[96];
        snprintf(prompt, sizeof prompt, "  Number of partitions (1-%d): ", MAX_FIXED_BLOCKS);
        cfg->nfixed = read_int(prompt, 1, MAX_FIXED_BLOCKS);
        for (int i = 0; i < cfg->nfixed; i++) {
            snprintf(prompt, sizeof prompt, "  Size of partition %d in KB: ", i + 1);
            cfg->fixed[i] = read_int(prompt, 1, 100000);
            cfg->total += cfg->fixed[i];
        }
    } else {
        static const int ex[] = {100, 500, 200, 300, 600};
        cfg->nfixed = 5;
        for (int i = 0; i < 5; i++) {
            cfg->fixed[i] = ex[i];
            cfg->total += ex[i];
        }
    }
}

static Strategy choose_strategy(Strategy cur)
{
    printf("\n");
    for (int s = 0; s < STRATEGY_COUNT; s++)
        printf("  %d. %s%s\n", s + 1, strategy_name((Strategy)s),
               (Strategy)s == cur ? "  (current)" : "");
    return (Strategy)(read_int("  Choose: ", 1, STRATEGY_COUNT) - 1);
}

static void do_allocate(Memory *m, Strategy s)
{
    int pid = read_int("  Process ID (1-9999): ", 1, 9999);
    int size = read_int("  Process size in KB: ", 1, 10000000);

    switch (mem_allocate(m, s, pid, size)) {
    case ALLOC_OK:
        printf("  P%d (%d KB) allocated using %s.\n", pid, size, strategy_name(s));
        show_map(m);
        break;
    case ALLOC_NOFIT:
        printf("  No free block can hold %d KB, so P%d could not be allocated.\n",
               size, pid);
        break;
    case ALLOC_DUP:
        printf("  Process ID %d is already in memory. Use a different ID.\n", pid);
        break;
    default:
        printf("  Invalid size.\n");
        break;
    }
}

static void do_free(Memory *m)
{
    int pid = read_int("  Process ID to deallocate: ", 1, 9999);
    if (mem_free(m, pid)) {
        printf("  P%d deallocated.\n", pid);
        show_map(m);
    } else {
        printf("  Process %d is not in memory.\n", pid);
    }
}

/* ------------------------------------------------------------------ */
/* Workload creation and comparison                                    */
/* ------------------------------------------------------------------ */

static void builtin_workload(const Config *c, Workload *w)
{
    if (!c->dynamic) {
        /* classic textbook sequence, meant for 100 500 200 300 600 */
        wl_add(w, 'A', 1, 212);
        wl_add(w, 'A', 2, 417);
        wl_add(w, 'A', 3, 112);
        wl_add(w, 'A', 4, 426);
        return;
    }

    /* Designed for 1000 KB; sizes scale with the memory size. */
    static const Event demo[] = {
        {'A', 1, 200}, {'A', 2, 150}, {'A', 3, 300}, {'A', 4, 100},
        {'F', 2, 0},   {'F', 4, 0},   {'A', 5, 120}, {'A', 6, 80},
        {'F', 1, 0},   {'A', 7, 250}, {'A', 8, 90},  {'F', 3, 0},
        {'A', 9, 400}, {'A', 10, 60}, {'A', 11, 130}
    };
    for (size_t i = 0; i < sizeof demo / sizeof demo[0]; i++) {
        int size = (int)((long)demo[i].size * c->total / 1000);
        if (demo[i].type == 'A' && size < 1)
            size = 1;
        wl_add(w, demo[i].type, demo[i].pid, size);
    }
}

static void random_workload(const Config *c, Workload *w)
{
    int n = read_int("  Number of events (5-500): ", 5, 500);
    int seed = read_int("  Random seed (0 = pick one from the clock): ", 0, 1000000);
    if (seed == 0)
        seed = (int)(time(NULL) % 1000000) + 1;
    printf("  Using seed %d (enter it again to repeat this workload).\n", seed);
    srand((unsigned)seed);

    int maxsz;
    if (c->dynamic) {
        maxsz = c->total / 4;
    } else {
        maxsz = 0;
        for (int i = 0; i < c->nfixed; i++)
            if (c->fixed[i] > maxsz)
                maxsz = c->fixed[i];
    }
    if (maxsz < 1)
        maxsz = 1;
    int minsz = maxsz / 10;
    if (minsz < 1)
        minsz = 1;

    int live[500], nlive = 0, next_pid = 1;
    for (int i = 0; i < n; i++) {
        if (nlive > 0 && rand() % 100 < 35) {
            int k = rand() % nlive;
            wl_add(w, 'F', live[k], 0);
            live[k] = live[--nlive];
        } else {
            int size = minsz + rand() % (maxsz - minsz + 1);
            wl_add(w, 'A', next_pid, size);
            live[nlive++] = next_pid++;
        }
    }
}

/* File format: one event per line, '#' starts a comment.
 *   A <pid> <size>     allocate
 *   F <pid>            free                                            */
static int load_workload(Workload *w)
{
    char path[256];
    printf("  Path of workload file: ");
    fflush(stdout);
    read_line(path, sizeof path);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  Cannot open '%s'.\n", path);
        return 0;
    }

    char line[256];
    int ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        char *p = line;
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '#' || *p == '\0')
            continue;

        int pid, size;
        if ((p[0] == 'A' || p[0] == 'a') &&
            sscanf(p + 1, "%d %d", &pid, &size) == 2 && pid > 0 && size > 0) {
            wl_add(w, 'A', pid, size);
        } else if ((p[0] == 'F' || p[0] == 'f') &&
                   sscanf(p + 1, "%d", &pid) == 1 && pid > 0) {
            wl_add(w, 'F', pid, 0);
        } else {
            printf("  Line %d is invalid (use 'A pid size' or 'F pid').\n", ln);
            fclose(f);
            w->n = 0;
            return 0;
        }
    }
    fclose(f);
    return 1;
}

static void apply_workload(Memory *m, Strategy s, const Workload *w)
{
    for (int i = 0; i < w->n; i++) {
        if (w->ev[i].type == 'A')
            mem_allocate(m, s, w->ev[i].pid, w->ev[i].size);
        else
            mem_free(m, w->ev[i].pid);
    }
}

static void compare_all(const Config *cfg)
{
    Workload w = {0};

    printf("\n  Workload source:\n");
    printf("  1. Built-in demo\n");
    printf("  2. Random workload\n");
    printf("  3. Load from a file\n");
    int ch = read_int("  Choose: ", 1, 3);

    if (ch == 1)
        builtin_workload(cfg, &w);
    else if (ch == 2)
        random_workload(cfg, &w);
    else if (!load_workload(&w)) {
        wl_free(&w);
        return;
    }
    if (w.n == 0) {
        printf("  The workload is empty.\n");
        wl_free(&w);
        return;
    }

    char desc[96];
    describe_config(cfg, desc, sizeof desc);
    printf("\n  Running %d events on identical memory (%s)\n\n", w.n, desc);
    printf("  %-10s %6s %6s %7s %9s %8s %6s %8s %9s\n",
           "Strategy", "Placed", "Failed", "Util%", "ExtFrag%",
           "IntFrag", "Holes", "Largest", "Searched");
    printf("  ------------------------------------------------------------------------------\n");

    Memory mems[STRATEGY_COUNT];
    int best_placed = -1;
    for (int s = 0; s < STRATEGY_COUNT; s++) {
        mem_init(&mems[s], cfg);
        apply_workload(&mems[s], (Strategy)s, &w);
        Stats st = mem_stats(&mems[s]);
        printf("  %-10s %6d %6d %6.1f%% %8.1f%% %8d %6d %8d %9ld\n",
               strategy_name((Strategy)s), mems[s].alloc_ok, mems[s].alloc_fail,
               st.utilization, st.external_frag, st.internal_frag,
               st.holes, st.largest_hole, mems[s].searched);
        if (mems[s].alloc_ok > best_placed)
            best_placed = mems[s].alloc_ok;
    }

    printf("\n  Most processes placed (%d): ", best_placed);
    int first = 1;
    for (int s = 0; s < STRATEGY_COUNT; s++) {
        if (mems[s].alloc_ok == best_placed) {
            printf("%s%s", first ? "" : ", ", strategy_name((Strategy)s));
            first = 0;
        }
    }
    printf("\n");

    printf("\n  Memory at the end of the run (digit = process ID, . = free):\n");
    for (int s = 0; s < STRATEGY_COUNT; s++) {
        printf("  %-10s ", strategy_name((Strategy)s));
        mem_print_bar(&mems[s], BAR_WIDTH);
        mem_destroy(&mems[s]);
    }
    wl_free(&w);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    Config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.dynamic = 1;
    cfg.total = 1000;

    Memory mem;
    mem_init(&mem, &cfg);
    Strategy cur = FIRST_FIT;

    printf("==============================================\n");
    printf("   Dynamic Memory Allocation Simulator (C)\n");
    printf("==============================================\n");

    for (;;) {
        char desc[96];
        describe_config(&mem.cfg, desc, sizeof desc);
        printf("\n---------------- MENU ----------------\n");
        printf(" Memory   : %s\n", desc);
        printf(" Strategy : %s\n", strategy_name(cur));
        printf("--------------------------------------\n");
        printf("  1. Configure memory\n");
        printf("  2. Choose allocation strategy\n");
        printf("  3. Allocate a process\n");
        printf("  4. Deallocate a process\n");
        printf("  5. Show memory map\n");
        printf("  6. Show statistics\n");
        printf("  7. Compact memory (dynamic mode only)\n");
        printf("  8. Compare all strategies on a workload\n");
        printf("  9. Reset memory\n");
        printf("  0. Exit\n");

        switch (read_int("Enter choice: ", 0, 9)) {
        case 1:
            configure(&cfg);
            mem_destroy(&mem);
            mem_init(&mem, &cfg);
            show_map(&mem);
            break;
        case 2:
            cur = choose_strategy(cur);
            break;
        case 3:
            do_allocate(&mem, cur);
            break;
        case 4:
            do_free(&mem);
            break;
        case 5:
            show_map(&mem);
            break;
        case 6:
            print_stats(&mem);
            break;
        case 7:
            if (!mem.cfg.dynamic) {
                printf("  Compaction only applies to dynamic partitions.\n");
            } else {
                mem_compact(&mem);
                printf("  Memory compacted: all free space is now one hole at the end.\n");
                show_map(&mem);
            }
            break;
        case 8:
            compare_all(&mem.cfg);
            break;
        case 9:
            mem_destroy(&mem);
            mem_init(&mem, &cfg);
            printf("  Memory reset.\n");
            break;
        case 0:
            mem_destroy(&mem);
            printf("Goodbye!\n");
            return 0;
        }
    }
}
