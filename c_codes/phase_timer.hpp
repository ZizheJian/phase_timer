#pragma once

#include <array>
#include <cstdio>
#include <vector>
#include <omp.h>

// 统计一段并行程序中各阶段的CPU时间。程序被分成若干个并行任务（例如OpenMP循环里的一次迭代），每个任务有自己的累加器，不需要加锁。
// 每个任务取一个lap_timer：restart()开始计时，每完成一个阶段调用lap(阶段编号)，把距上一次计时的时间加到这个阶段上。
// 最后print()打印所有任务在各阶段上的CPU时间之和及占比。
// enabled为false时不分配内存，所有调用都是空操作，可以留在需要测吞吐率的代码里
template<bool enabled,size_t phase_num>
class phase_timer
{
    public:
        class lap_timer
        {
            public:
                explicit lap_timer(double *times):times(times){restart();}
                void restart()
                {
                    if constexpr(enabled)
                        last_time=omp_get_wtime();
                }
                void lap(size_t phase)
                {
                    if constexpr(enabled)
                    {
                        double now=omp_get_wtime();
                        times[phase]+=now-last_time;
                        last_time=now;
                    }
                }
            private:
                double *times;
                double last_time=0;
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
                return lap_timer(task_times[task_id].data());
            return lap_timer(nullptr);
        }

        // 所有任务在某个阶段上的CPU时间之和
        double total(size_t phase) const
        {
            double sum=0;
            for (auto &times:task_times)
                sum+=times[phase];
            return sum;
        }

        // 打印每个阶段的累计CPU时间和占比，例如"Compression accumulated CPU times: read= 0.1, write= 0.2 seconds"
        void print(const char *title) const
        {
            if constexpr(enabled)
            {
                std::array<double,phase_num> totals;
                double measured_time=0;
                for (size_t phase=0;phase<phase_num;phase++)
                    measured_time+=totals[phase]=total(phase);
                printf("%s accumulated CPU times:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %f",phase==0?"":",",phase_names[phase],totals[phase]);
                printf(" seconds\n");
                if (measured_time<=0)
                    return;
                printf("%s stage shares:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %.2f%%",phase==0?"":",",phase_names[phase],100*totals[phase]/measured_time);
                printf("\n");
            }
        }

    private:
        std::array<const char*,phase_num> phase_names;
        std::vector<std::array<double,phase_num>> task_times;
};
