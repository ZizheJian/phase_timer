# phase_timer

**English** | [中文](README_CN.md)

`phase_timer` measures how much CPU time each phase of a parallel program takes. The work is split into tasks, for example the iterations of an OpenMP loop, and each task accumulates its own phase times, so no locking is needed. At the end it prints the CPU time of every phase summed over all tasks, together with each phase's share. It is a single C++17 header and uses `omp_get_wtime()` for timing.

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
Processing accumulated CPU times: read= 0.120000, compute= 1.800000, write= 0.080000 seconds
Processing stage shares: read= 6.00%, compute= 90.00%, write= 4.00%
```

Call `lap_timer.restart()` to skip a section that should not be counted. `timer.total(phase)` returns the summed time of one phase.

When the first template argument is `false`, the timer allocates nothing and every call is a no-op, so the timing code can stay in a function whose throughput is measured. Compile both versions and choose one at run time, for example with `if (measure) process<true>(tasks); else process<false>(tasks);`.

The reported times are CPU times summed over tasks, not wall-clock time. With several threads, their sum is larger than the elapsed time.
