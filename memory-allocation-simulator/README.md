# Dynamic Memory Allocation Simulator (C)

A console program that simulates how an operating system hands out memory to
processes using **First Fit**, **Best Fit**, **Worst Fit** (and a bonus
**Next Fit**), and compares the strategies side by side.

**Concepts covered:** dynamic memory management, allocation strategies,
splitting, coalescing, compaction, internal and external fragmentation.

## Files

| File | Purpose |
|---|---|
| `allocator.h` | Data structures and function declarations |
| `allocator.c` | The memory model and the four placement strategies |
| `main.c` | Menu, input handling, workloads, comparison table |
| `Makefile` | Build with `make` (Linux / macOS / MSYS2) |
| `build.bat` | One-click build for Windows |
| `sample_workload.txt` | Example workload file for the compare option |

## How to build and run

You need a C compiler (`gcc` or `clang`). Nothing else.

### Windows
1. Install a compiler if you don't have one. Easiest options:
   - **MinGW-w64 / MSYS2** (gives you `gcc`), or
   - **Code::Blocks with MinGW**, or
   - **VS Code + MinGW** (C/C++ extension).
2. Unzip the folder, open **Command Prompt** or **PowerShell** inside it.
3. Build and run:
   ```
   build.bat
   memsim.exe
   ```
   or directly:
   ```
   gcc -Wall -Wextra -std=c99 -O2 -o memsim.exe main.c allocator.c
   memsim.exe
   ```

### Linux / macOS
```
cd memory-allocation-simulator
make
./memsim
```
or without make:
```
gcc -Wall -Wextra -std=c99 -O2 -o memsim main.c allocator.c
./memsim
```
(macOS: if `gcc` is not found, run `xcode-select --install`, or use `clang`.)

### Code::Blocks / Dev-C++ / any IDE
Create a new C console project and add `main.c`, `allocator.c` and
`allocator.h` to it, then build and run.

## Using the program

```
1. Configure memory                 pick the memory model
2. Choose allocation strategy       First / Best / Worst / Next Fit
3. Allocate a process               enter a process ID and size in KB
4. Deallocate a process
5. Show memory map                  table + one-line picture
6. Show statistics
7. Compact memory                   dynamic mode only
8. Compare all strategies           runs one workload through all four
9. Reset memory
0. Exit
```

### Two memory models (option 1)
- **Dynamic partitions**: one big region. A process gets exactly the size it
  asks for; the leftover stays free. Freed neighbours merge (coalescing).
  There is no internal fragmentation, only external.
- **Fixed partitions**: a list of block sizes you choose. A process takes a
  whole block, so unused space inside it is internal fragmentation.
  Option 3 loads the classic textbook layout `100 500 200 300 600`.

### Reading the memory map
```
|111111111111222222222222222222222222........................|
```
Each digit is the last digit of a process ID, `.` is free memory.

### Comparing the strategies (option 8)
Pick a workload, and the same sequence of events is replayed on identical
memory once per strategy:
1. **Built-in demo**: a fixed sequence (scaled to your memory size), good for demos.
2. **Random**: you give the number of events and a seed. The same seed gives
   the same workload, so results are repeatable.
3. **File**: your own events, see `sample_workload.txt`:
   ```
   A 1 200     # allocate process 1, 200 KB
   F 1         # free process 1
   ```

Columns of the comparison table:

| Column | Meaning |
|---|---|
| Placed / Failed | processes that were / were not given memory |
| Util% | memory actually requested by running processes / total memory |
| ExtFrag% | `1 - largest free hole / total free memory`; high means free space is scattered |
| IntFrag | KB wasted inside allocated blocks (fixed partitions only) |
| Holes / Largest | number of free blocks / size of the biggest one |
| Searched | blocks examined in total; a measure of allocation time |

## How each strategy works

| Strategy | Rule | Typical behaviour |
|---|---|---|
| First Fit | first free block that is big enough | fast, simple |
| Best Fit | smallest free block that is big enough | leaves tiny unusable holes, scans whole list |
| Worst Fit | largest free block | leaves big leftover holes, scans whole list |
| Next Fit | First Fit, but resumes from the last allocation | fast, spreads allocations out |

## Quick sanity check (textbook example)

Option 1 -> 3 (blocks 100 500 200 300 600), then option 8 -> 1. The processes
are 212, 417, 112 and 426 KB. Expected result:

- **Best Fit** places all four.
- **First Fit** and **Worst Fit** fail on the 426 KB process.

## Implementation notes (useful for a viva or report)

- Memory is a **singly linked list of `Block`s** sorted by address.
- Dynamic allocation **splits** a free block; `mem_free` **coalesces** adjacent
  free blocks in one pass.
- `mem_compact` slides allocated blocks to the start and leaves one big hole.
- Next Fit remembers the address of the last allocation (`rover`) rather than a
  pointer, so it stays valid when blocks are split, merged or compacted.
- Every block examined increments a counter, giving a fair "search cost"
  comparison between strategies.
- The code is plain C99 with no external libraries, and was tested with
  AddressSanitizer and UndefinedBehaviorSanitizer (no errors, no leaks).

## Ideas to extend it
- Buddy system allocation
- Save and load memory state
- Time-based simulation with process arrival and burst times
- Export the comparison table to CSV
