#pragma once

#include <array>
#include <chrono>
#include <cstdio>
#include <vector>

// 统计一段并行程序中各阶段的耗时。程序被分成若干个任务（例如OpenMP循环里的一次迭代），每个任务有自己的累加器，不需要加锁。
// 每个任务取一个lap_timer：restart()开始计时，每完成一个阶段调用lap(阶段编号)，把距上一次计时经过的墙钟时间加到这个阶段上。
// 最后print()打印所有任务在各阶段上的耗时之和及占比。
// 量的是墙钟时间，线程被操作系统挤掉时的等待也会算进去，所以要让每个线程独占一个核。同一个任务的计时器同一时刻只能由一个线程使用。
// enabled为false时不分配内存，所有调用都是空操作，可以留在需要测吞吐率的代码里
template<bool enabled,size_t phase_num>
class phase_timer
{
    private:
        // 每个任务的累加器单独占一块64字节对齐的内存。CPU缓存按64字节一块读写，不对齐时不同线程写相邻任务会互相拖慢
        struct alignas(64) task_time
        {
            std::array<double,phase_num> times{};
        };
        std::array<const char*,phase_num> phase_names;
        std::vector<task_time> task_times;

    public:
        class lap_timer
        {
            private:
                double *times;
                double last_time=0;

            public:
                explicit lap_timer(double *times):times(times){restart();}

                void restart()
                {
                    if constexpr(enabled)
                        last_time=now();
                }

                void lap(size_t phase)
                {
                    if constexpr(enabled)
                    {
                        double current=now();
                        times[phase]+=current-last_time;
                        last_time=current;
                    }
                }

            private:
                static double now()
                {
                    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                }
        };

        phase_timer(std::array<const char*,phase_num> phase_names,size_t task_num):phase_names(phase_names)
        {
            if constexpr(enabled)
                task_times.assign(task_num,{});
        }

        // 第task_id个任务的计时器，不同任务可以在不同线程中同时使用
        lap_timer task(size_t task_id)
        {
            if constexpr(enabled)
                return lap_timer(task_times[task_id].times.data());
            return lap_timer(nullptr);
        }

        // 所有任务在某个阶段上的耗时之和
        double total(size_t phase) const
        {
            double sum=0;
            for (auto &task:task_times)
                sum+=task.times[phase];
            return sum;
        }

        // 打印每个阶段的耗时之和和占比，例如"Compression phase times summed over tasks: read= 0.1, write= 0.2 seconds"
        void print(const char *title) const
        {
            if constexpr(enabled)
            {
                std::array<double,phase_num> totals;
                double measured_time=0;
                for (size_t phase=0;phase<phase_num;phase++)
                    measured_time+=totals[phase]=total(phase);
                printf("%s phase times summed over tasks:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %f",phase==0?"":",",phase_names[phase],totals[phase]);
                printf(" seconds\n");
                if (measured_time<=0)
                    return;
                printf("%s phase shares:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %.2f%%",phase==0?"":",",phase_names[phase],100*totals[phase]/measured_time);
                printf("\n");
            }
        }
};
