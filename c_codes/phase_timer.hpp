#pragma once

#include <array>
#include <chrono>
#include <cstdio>
#include <vector>

// phase_timer统计一段并行程序中各阶段的耗时。
//
// 用法概要：
//   1. 把程序分成若干个任务，例如OpenMP循环里的每次迭代，或者每个partition。每个任务有自己的累加器，不同任务互不干扰，不需要加锁。
//   2. 每个任务开始时调用start_task(任务编号)，得到这个任务的计时器task_timer，同时开始计时。
//   3. 每做完一个阶段，调用task_timer的end_phase(阶段编号)，把这个阶段从开始到现在经过的时间加到这个阶段上，下一个阶段从现在开始计时。
//   4. 全部做完后，print()把所有任务在每个阶段上的时间加起来打印，并给出各阶段占的比例。
//
// 注意事项：
//   - 量的是墙钟时间（真实经过的时间）。线程被操作系统挤掉、等待别的程序时，这段等待也会算进去，所以测量时要让每个线程独占一个核。
//   - 同一个任务的计时器同一时刻只能由一个线程使用。
//
// 模板参数：
//   enabled    是否真的计时。为false时不分配内存，所有函数都是空操作，编译器会把它们全部优化掉，所以计时代码可以一直留在需要测吞吐率的函数里。
//   phase_num  阶段的数量。
template<bool enabled,size_t phase_num>
class phase_timer
{
    private:
        // 一个任务在各阶段上累计的秒数：seconds[k]是这个任务在第k个阶段上累计的秒数，初始全为0。
        // alignas(64)让每个task_phase_seconds从64字节的边界开始，占64字节的整数倍。
        // 原因：CPU缓存每次读写一整块64字节（一个缓存行）。如果两个任务的累加器落在同一块里，
        // 两个线程分别写自己的累加器时，也会让对方缓存里的这一块失效，互相拖慢（称为伪共享）。对齐后每个任务各占一块，就不会互相影响
        struct alignas(64) task_phase_seconds
        {
            std::array<double,phase_num> seconds{};
        };

        // 各阶段的名字，例如{"read","compute","write"}，只在print()打印时用作标签
        std::array<const char*,phase_num> phase_names;

        // 所有任务的累加器，第i个元素属于第i个任务：seconds_by_task[i].seconds[k]是第i个任务在第k个阶段上累计的秒数。
        // enabled为false时保持为空，不分配内存
        std::vector<task_phase_seconds> seconds_by_task;

    public:
        // 一个任务的计时器。它记着当前阶段开始的时刻，每个阶段结束时算出经过的时间，加到这个任务的累加器上
        class task_timer
        {
            private:
                // 指向这个任务的累加器，也就是seconds_by_task[i].seconds的开头。enabled为false时是nullptr，不会被使用
                double *phase_seconds;
                // 当前阶段开始的时刻，单位为秒
                double phase_start_time=0;

            public:
                // 创建时立即开始计时
                explicit task_timer(double *phase_seconds):phase_seconds(phase_seconds){restart();}

                // 从现在重新开始计时，之前经过的时间不计入任何阶段。用于跳过不想统计的代码
                void restart()
                {
                    if constexpr(enabled)
                        phase_start_time=now();
                }

                // 第phase个阶段到此结束：把它从开始到现在经过的时间加到这个阶段上，下一个阶段从现在开始计时
                void end_phase(size_t phase)
                {
                    if constexpr(enabled)
                    {
                        double current_time=now();
                        phase_seconds[phase]+=current_time-phase_start_time;
                        phase_start_time=current_time;
                    }
                }

            private:
                // 当前时刻，单位为秒。steady_clock是只会向前走的时钟，不受系统改时间的影响，适合计时
                static double now()
                {
                    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                }
        };

        // phase_names是各阶段的名字，task_num是任务的数量。计时时为每个任务分配一个全0的累加器
        phase_timer(std::array<const char*,phase_num> phase_names,size_t task_num):phase_names(phase_names)
        {
            if constexpr(enabled)
                seconds_by_task.assign(task_num,{});
        }

        // 第task_id个任务开始：返回这个任务的计时器，同时开始计时。不同任务的计时器可以在不同线程中同时使用
        task_timer start_task(size_t task_id)
        {
            if constexpr(enabled)
                return task_timer(seconds_by_task[task_id].seconds.data());
            return task_timer(nullptr);
        }

        // 所有任务在第phase个阶段上累计的秒数之和。enabled为false时返回0
        double total_seconds(size_t phase) const
        {
            double sum=0;
            for (auto &task:seconds_by_task)
                sum+=task.seconds[phase];
            return sum;
        }

        // 打印两行结果，title是每行开头的说明，例如：
        //   Compression phase times summed over tasks: read= 0.1, write= 0.2 seconds
        //   Compression phase shares: read= 33.33%, write= 66.67%
        // 第一行是每个阶段在所有任务上的时间之和，第二行是各阶段占全部计时时间的比例。enabled为false时什么都不打印
        void print(const char *title) const
        {
            if constexpr(enabled)
            {
                // 先算出每个阶段的总秒数，以及所有阶段加起来的总秒数
                std::array<double,phase_num> phase_total_seconds;
                double all_phase_seconds=0;
                for (size_t phase=0;phase<phase_num;phase++)
                    all_phase_seconds+=phase_total_seconds[phase]=total_seconds(phase);
                printf("%s phase times summed over tasks:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %f",phase==0?"":",",phase_names[phase],phase_total_seconds[phase]);
                printf(" seconds\n");
                // 没有记录到任何时间时，比例没有意义，只打印第一行
                if (all_phase_seconds<=0)
                    return;
                printf("%s phase shares:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %.2f%%",phase==0?"":",",phase_names[phase],100*phase_total_seconds[phase]/all_phase_seconds);
                printf("\n");
            }
        }
};
