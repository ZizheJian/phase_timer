# phase_timer

[English](README.md) | **中文**

`phase_timer`统计并行程序中每个阶段花了多少时间。程序被分成若干个任务，例如OpenMP循环里的每次迭代，每个任务各自累加自己的阶段时间，不需要加锁。最后打印所有任务在每个阶段上的耗时之和，以及每个阶段的占比。它只有一个C++17头文件，只依赖标准库，OpenMP、`std::thread`和串行程序都能用。

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
        auto task_timer=timer.start_task(task_id); // 任务开始，read阶段也从这里开始计时
        read(tasks[task_id]);
        task_timer.end_phase(read_phase);           // read阶段结束，把它经过的时间加到read_phase上
        compute(tasks[task_id]);
        task_timer.end_phase(compute_phase);
        write(tasks[task_id]);
        task_timer.end_phase(write_phase);
    }
    timer.print("Processing");
}
```

输出示例：

```
Processing phase times summed over tasks: read= 0.120000, compute= 1.800000, write= 0.080000 seconds
Processing phase shares: read= 6.00%, compute= 90.00%, write= 4.00%
```

某一段不想计入时，调用`task_timer.restart()`重新开始计时。`timer.total_seconds(phase)`返回某个阶段的时间之和。

第一个模板参数为`false`时，计时器不分配内存，所有调用都是空操作，所以计时代码可以留在需要测吞吐率的函数里。把两个版本都编译出来，运行时再选，例如`if (measure) process<true>(tasks); else process<false>(tasks);`。

## 数字的含义

每次`end_phase`把这个阶段从开始到现在经过的墙钟时间（用`std::chrono::steady_clock`读取）加到这个任务的对应阶段上，下一个阶段从同一时刻开始。输出的是所有任务的这些时间之和。用多个线程时，这个和会大于程序实际经过的时间。

墙钟时间也包括线程等待的时间：操作系统把线程所在的核让给别的程序时，这段等待会被算进线程当时所处的阶段。所以测量时要让每个线程独占一个核，并且这个核上没有别的进程：线程数不超过分配到的核数，例如Slurm里`--cpus-per-task`不小于线程数，并且不要在多人共用的登录节点上测。不需要申请整个节点。按线程统计的CPU时钟可以排除这种等待，但在Linux上每读一次约0.5 µs，而墙钟只要约30 ns，会严重放大只有几微秒的阶段。

不同的任务可以在不同线程中同时计时。同一个任务的计时器同一时刻只能由一个线程使用，因为它对累加器的更新没有同步。

每个任务的累加器各占一块64字节的缓存行，所以不同线程给相邻任务计时时不会互相拖慢。

## 测试

```bash
cd tests
g++ -std=c++17 -O2 -Wall -pthread -I ../c_codes test_phase_timer.cpp -o test_phase_timer
./test_phase_timer
```

测试用`std::thread`，并且不用OpenMP编译。测试检查：关闭时不记录任何时间，`restart()`之前的时间不计入，以及串行和多线程时记录的阶段时间与在同样位置独立测得的时间一致。
