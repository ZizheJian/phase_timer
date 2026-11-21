#pragma once

#include <array>
#include <chrono>
#include <cstdio>
#include <vector>

// phase_timer统计并行程序中每个阶段一共花了多少时间。
//
// 它维护一张表：每个任务一行，每个阶段一列，格子里是这个任务在这个阶段上累计的秒数。
// 任务可以是OpenMP循环的一次迭代、一个partition等。不同任务写不同的行，所以多个线程同时计时也不需要加锁。
//
// 用法：
//   phase_timer<true,3> timer({"read","compute","write"},task_num);  // 3个阶段，task_num个任务
//   timer.start(task_id);               // 任务开始，第一个阶段从现在开始计时
//   read(...);
//   timer.end_phase(task_id,0);         // 第0个阶段结束，把它的耗时加到表中，下一个阶段从现在开始
//   compute(...);
//   timer.end_phase(task_id,1);
//   write(...);
//   timer.end_phase(task_id,2);
//   timer.print("Processing");          // 打印每一列（每个阶段）的总和与占比
//
// 中间有不想计入的代码时，在它之后再调用一次start(task_id)。
// 第一个模板参数为false时不分配内存，所有函数什么都不做，计时代码可以一直留在需要测吞吐率的函数里。
// 量的是墙钟时间，线程被操作系统挤掉时的等待也会算进去，所以测量时要让每个线程独占一个核。
template<bool enabled,size_t phase_num>
class phase_timer
{
    public:
        // 表的一行，对应一个任务。alignas(64)让每行单独占一块64字节的缓存，不同线程写不同的行时不会互相拖慢
        struct alignas(64) task_row
        {
            std::array<double,phase_num> phase_seconds{}; // 这个任务在每个阶段上累计的秒数
            double phase_start_time=0; // 这个任务当前阶段开始的时刻
        };

        std::array<const char*,phase_num> phase_names; // 各阶段的名字，打印时用
        std::vector<task_row> rows; // 整张表，第i行对应第i个任务

        phase_timer(std::array<const char*,phase_num> phase_names,size_t task_num)
        {
            this->phase_names=phase_names;
            if constexpr(enabled)
                rows.resize(task_num);
        }

        // 第task_id个任务的下一个阶段从现在开始计时
        void start(size_t task_id)
        {
            if constexpr(enabled)
                rows[task_id].phase_start_time=now();
        }

        // 第task_id个任务的第phase个阶段到此结束：把它的耗时加到表中，下一个阶段从现在开始计时
        void end_phase(size_t task_id,size_t phase)
        {
            if constexpr(enabled)
            {
                double current_time=now();
                rows[task_id].phase_seconds[phase]+=current_time-rows[task_id].phase_start_time;
                rows[task_id].phase_start_time=current_time;
            }
        }

        // 打印两行，第一行是每个阶段的总秒数，第二行是各阶段占的比例，例如：
        //   Processing phase times summed over tasks: read= 0.1, compute= 0.3 seconds
        //   Processing phase shares: read= 25.00%, compute= 75.00%
        void print(const char *title) const
        {
            if constexpr(enabled)
            {
                // 每一列的总和，以及所有阶段加起来的总秒数
                std::array<double,phase_num> phase_total_seconds{};
                double all_phase_seconds=0;
                for (auto &row:rows)
                    for (size_t phase=0;phase<phase_num;phase++)
                    {
                        phase_total_seconds[phase]+=row.phase_seconds[phase];
                        all_phase_seconds+=row.phase_seconds[phase];
                    }
                printf("%s phase times summed over tasks:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %f",phase==0?"":",",phase_names[phase],phase_total_seconds[phase]);
                printf(" seconds\n");
                if (all_phase_seconds<=0)
                    return;
                printf("%s phase shares:",title);
                for (size_t phase=0;phase<phase_num;phase++)
                    printf("%s %s= %.2f%%",phase==0?"":",",phase_names[phase],100*phase_total_seconds[phase]/all_phase_seconds);
                printf("\n");
            }
        }

    private:
        // 当前时刻，单位为秒。steady_clock只会向前走，不受修改系统时间的影响
        static double now()
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
};
