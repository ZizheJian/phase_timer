# phase_timer

[English](README.md) | **中文**

`phase_timer`统计并行程序中每个阶段花了多少CPU时间。程序被分成若干个任务，例如OpenMP循环里的每次迭代，每个任务各自累加自己的阶段时间，不需要加锁。最后打印所有任务在每个阶段上的CPU时间之和，以及每个阶段的占比。它只有一个C++17头文件，用`omp_get_wtime()`计时。

## 用法

在CMake里把目录加为子目录，再链接目标：

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
        auto lap_timer=timer.task(task_id); // 开始计时
        read(tasks[task_id]);
        lap_timer.lap(read_phase);           // 把距上一次计时的时间加到read_phase上
        compute(tasks[task_id]);
        lap_timer.lap(compute_phase);
        write(tasks[task_id]);
        lap_timer.lap(write_phase);
    }
    timer.print("Processing");
}
```

输出示例：

```
Processing accumulated CPU times: read= 0.120000, compute= 1.800000, write= 0.080000 seconds
Processing stage shares: read= 6.00%, compute= 90.00%, write= 4.00%
```

某一段不想计入时，调用`lap_timer.restart()`重新开始计时。`timer.total(phase)`返回某个阶段的时间之和。

第一个模板参数为`false`时，计时器不分配内存，所有调用都是空操作，所以计时代码可以留在需要测吞吐率的函数里。把两个版本都编译出来，运行时再选，例如`if (measure) process<true>(tasks); else process<false>(tasks);`。

输出的是各任务CPU时间之和，不是墙钟时间。用多个线程时，这个和会大于实际经过的时间。
