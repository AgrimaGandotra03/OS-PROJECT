/*
 * allocator.c - implementation of the memory model and the four
 * placement strategies (First, Best, Worst and Next Fit).
 */
#include <stdio.h>
#include <stdlib.h>
#include "allocator.h"

static const char *NAMES[STRATEGY_COUNT] = {
    "First Fit", "Best Fit", "Worst Fit", "Next Fit"
};

const char *strategy_name(Strategy s)
{
    return (s >= 0 && s < STRATEGY_COUNT) ? NAMES[s] : "Unknown";
}

static Block *new_block(int start, int size, int is_free)
{
    Block *b = malloc(sizeof *b);
    if (!b) {
        fprintf(stderr, "Fatal: out of memory\n");
        exit(1);
    }
    b->start = start;
    b->size = size;
    b->free = is_free;
    b->pid = -1;
    b->req = 0;
    b->next = NULL;
    return b;
}

void mem_destroy(Memory *m)
{
    Block *b = m->head;
    while (b) {
        Block *n = b->next;
        free(b);
        b = n;
    }
    m->head = NULL;
}

int mem_init(Memory *m, const Config *cfg)
{
    m->cfg = *cfg;
    m->head = NULL;
    m->rover = 0;
    m->searched = 0;
    m->alloc_ok = 0;
    m->alloc_fail = 0;

    if (cfg->dynamic) {
        if (cfg->total <= 0)
            return 0;
        m->head = new_block(0, cfg->total, 1);
        return 1;
    }

    if (cfg->nfixed <= 0 || cfg->nfixed > MAX_FIXED_BLOCKS)
        return 0;

    Block *tail = NULL;
    int addr = 0;
    for (int i = 0; i < cfg->nfixed; i++) {
        if (cfg->fixed[i] <= 0) {
            mem_destroy(m);
            return 0;
        }
        Block *b = new_block(addr, cfg->fixed[i], 1);
        if (tail)
            tail->next = b;
        else
            m->head = b;
        tail = b;
        addr += cfg->fixed[i];
    }
    m->cfg.total = addr;
    return 1;
}

/*
 * Find a free block of at least `size` KB using strategy s.
 * Every block that is looked at increments m->searched, so the number can be
 * used to compare how much work each strategy does.
 */
static Block *find_block(Memory *m, Strategy s, int size)
{
    Block *b, *pick = NULL;

    switch (s) {
    case FIRST_FIT:
        /* first free block that is big enough */
        for (b = m->head; b; b = b->next) {
            m->searched++;
            if (b->free && b->size >= size)
                return b;
        }
        return NULL;

    case BEST_FIT:
        /* smallest free block that is big enough: scans the whole list */
        for (b = m->head; b; b = b->next) {
            m->searched++;
            if (b->free && b->size >= size && (!pick || b->size < pick->size))
                pick = b;
        }
        return pick;

    case WORST_FIT:
        /* largest free block: scans the whole list */
        for (b = m->head; b; b = b->next) {
            m->searched++;
            if (b->free && b->size >= size && (!pick || b->size > pick->size))
                pick = b;
        }
        return pick;

    case NEXT_FIT: {
        /* like First Fit, but resume from where the last allocation was made
         * and wrap around once */
        Block *start = m->head;
        for (b = m->head; b; b = b->next) {
            if (b->start + b->size > m->rover) {
                start = b;
                break;
            }
        }
        b = start;
        do {
            m->searched++;
            if (b->free && b->size >= size)
                return b;
            b = b->next ? b->next : m->head;
        } while (b != start);
        return NULL;
    }

    default:
        return NULL;
    }
}

int mem_allocate(Memory *m, Strategy s, int pid, int size)
{
    if (size <= 0)
        return ALLOC_BADSIZE;

    for (Block *p = m->head; p; p = p->next)
        if (!p->free && p->pid == pid)
            return ALLOC_DUP;

    Block *b = find_block(m, s, size);
    if (!b) {
        m->alloc_fail++;
        return ALLOC_NOFIT;
    }

    /* Dynamic mode: carve exactly `size` KB off and keep the rest free. */
    if (m->cfg.dynamic && b->size > size) {
        Block *rest = new_block(b->start + size, b->size - size, 1);
        rest->next = b->next;
        b->next = rest;
        b->size = size;
    }

    b->free = 0;
    b->pid = pid;
    b->req = size;
    m->rover = b->start;
    m->alloc_ok++;
    return ALLOC_OK;
}

/* Merge neighbouring free blocks into one (dynamic mode). */
static void coalesce(Memory *m)
{
    Block *b = m->head;
    while (b && b->next) {
        if (b->free && b->next->free) {
            Block *n = b->next;
            b->size += n->size;
            b->next = n->next;
            free(n);
        } else {
            b = b->next;
        }
    }
}

int mem_free(Memory *m, int pid)
{
    for (Block *b = m->head; b; b = b->next) {
        if (!b->free && b->pid == pid) {
            b->free = 1;
            b->pid = -1;
            b->req = 0;
            if (m->cfg.dynamic)
                coalesce(m);
            return 1;
        }
    }
    return 0;
}

void mem_compact(Memory *m)
{
    if (!m->cfg.dynamic)
        return;

    Block *b = m->head, *tail = NULL;
    int addr = 0;
    m->head = NULL;

    while (b) {
        Block *next = b->next;
        if (b->free) {
            free(b);
        } else {
            b->start = addr;
            addr += b->size;
            b->next = NULL;
            if (tail)
                tail->next = b;
            else
                m->head = b;
            tail = b;
        }
        b = next;
    }

    if (addr < m->cfg.total) {
        Block *f = new_block(addr, m->cfg.total - addr, 1);
        if (tail)
            tail->next = f;
        else
            m->head = f;
    }
    m->rover = 0;
}

Stats mem_stats(const Memory *m)
{
    Stats s = {0};
    s.total = m->cfg.total;

    for (const Block *b = m->head; b; b = b->next) {
        if (b->free) {
            s.free_total += b->size;
            s.holes++;
            if (b->size > s.largest_hole)
                s.largest_hole = b->size;
        } else {
            s.used += b->req;
            s.allocated_size += b->size;
            s.internal_frag += b->size - b->req;
        }
    }

    s.utilization = s.total ? 100.0 * s.used / s.total : 0.0;
    s.external_frag = s.free_total > 0
        ? 100.0 * (1.0 - (double)s.largest_hole / s.free_total)
        : 0.0;
    return s;
}

void mem_print(const Memory *m)
{
    printf("\n  %-8s %-8s %-8s %-7s %-9s %s\n",
           "Start", "End", "Size", "Status", "Process", "Wasted(internal)");
    printf("  ------------------------------------------------------------\n");
    for (const Block *b = m->head; b; b = b->next) {
        char who[16];
        if (b->free)
            snprintf(who, sizeof who, "-");
        else
            snprintf(who, sizeof who, "P%d", b->pid);
        printf("  %-8d %-8d %-8d %-7s %-9s %d\n",
               b->start, b->start + b->size - 1, b->size,
               b->free ? "FREE" : "USED", who,
               b->free ? 0 : b->size - b->req);
    }
}

void mem_print_bar(const Memory *m, int width)
{
    int total = m->cfg.total;
    putchar('|');
    for (const Block *b = m->head; b && total > 0; b = b->next) {
        int from = (int)((long)b->start * width / total);
        int to = (int)((long)(b->start + b->size) * width / total);
        char c = b->free ? '.' : (char)('0' + (b->pid % 10));
        for (int i = from; i < to; i++)
            putchar(c);
    }
    puts("|");
}
