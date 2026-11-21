# phase_timer

**English** | [中文](README_CN.md)

`phase_timer` measures how much time each phase of a parallel program takes. The work is split into tasks, for example the iterations of an OpenMP loop, and each task accumulates its own phase times, so no locking is needed. At the end it prints the time of every phase summed over all tasks, together with each phase's share. It is a single C++17 header that uses only the standard library, so it works with OpenMP, `std::thread`, or serial code.

## Usage

Add the directory as a CMake subdirectory and link the target:

```cmake
add_subdirectory(third_party/phase_timer/c_codes ${CMAKE_BINARY_DIR}/phase_timer_build)
target_link_libraries(my_target PRIVATE phase_timer)
```

```cpp
#include <phase_timer.hpp>

template<bool measure_phase_time>
void process(std::vector<Task> &tasks)
{
    enum {read_phase,compute_phase,write_phase};
    phase_timer<measure_phase_time,3> timer({"read","compute","write"},tasks.size());
    #pragma omp parallel for
    for (size_t task_id=0;task_id<tasks.size();task_id++)
    {
        auto lap_timer=timer.task(task_id); // starts timing
        read(tasks[task_id]);
        lap_timer.lap(read_phase);           // adds the time since the last lap to read_phase
        compute(tasks[task_id]);
        lap_timer.lap(compute_phase);
        write(tasks[task_id]);
        lap_timer.lap(write_phase);
    }
    timer.print("Processing");
}
```

This prints, for example:

```
Processing phase times summed over tasks: read= 0.120000, compute= 1.800000, write= 0.080000 seconds
Processing phase shares: read= 6.00%, compute= 90.00%, write= 4.00%
```

Call `lap_timer.restart()` to skip a section that should not be counted. `timer.total(phase)` returns the summed time of one phase.

When the first template argument is `false`, the timer allocates nothing and every call is a no-op, so the timing code can stay in a function whose throughput is measured. Compile both versions and choose one at run time, for example with `if (measure) process<true>(tasks); else process<false>(tasks);`.

## What the numbers mean

Each lap adds the wall-clock time since the previous lap, read from `std::chrono::steady_clock`, to the phase of that task. The printed values are these times summed over all tasks. With several threads, their total is larger than the elapsed time of the program.

Wall-clock time also includes the time a thread spends waiting while the operating system runs something else on its core, and that wait is added to whichever phase the thread happens to be in. Run the measurement with each thread on its own core that no other process uses: use no more threads than allocated cores, for example `--cpus-per-task` in Slurm at least as large as the thread count, and avoid shared login nodes. The whole node is not needed. A per-thread CPU clock would exclude such waits, but on Linux each read costs about 0.5 µs instead of about 30 ns, which distorts phases that last only a few microseconds.

Different tasks can be timed from different threads at the same time. The timer of one task must be used by only one thread at a time, because its laps are not synchronized.

Each task's times occupy their own 64-byte cache line, so threads that time neighboring tasks do not slow each other down.

## Tests

```bash
cd tests
g++ -std=c++17 -O2 -Wall -pthread -I ../c_codes test_phase_timer.cpp -o test_phase_timer
./test_phase_timer
```

The tests use `std::thread` and are compiled without OpenMP. They check that a disabled timer records nothing, that `restart()` discards earlier time, and that the recorded phase times match times measured independently at the same points in serial and multithreaded runs.
