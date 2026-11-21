// 编译运行见README。只用标准库和std::thread，不依赖OpenMP
#include <phase_timer.hpp>
#include <cmath>
#include <thread>

static int failures=0;
static void check(bool condition,const char *message,double value=0)
{
    if (!condition)
    {
        printf("FAILED: %s (%g)\n",message,value);
        failures++;
    }
}

static double now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// 做一段大约要seconds秒的计算
static double busy(double seconds)
{
    double end=now()+seconds,sum=0;
    while (now()<end)
        for (int i=0;i<1000;i++)
            sum+=std::sin(i);
    return sum;
}

// 表中第phase列的总和
template<typename Timer>
static double column_sum(const Timer &timer,size_t phase)
{
    double sum=0;
    for (auto &row:timer.rows)
        sum+=row.phase_seconds[phase];
    return sum;
}

int main()
{
    enum {a_phase,b_phase};
    // 关闭时什么都不记录
    phase_timer<false,2> disabled({"a","b"},4);
    disabled.start(0);
    disabled.end_phase(0,a_phase);
    check(column_sum(disabled,a_phase)==0 and disabled.rows.empty(),"disabled timer records time");
    // 串行：表中的时间与在同样位置自己记录的时间一致。机器很忙时两者都会变长，但仍应一致
    phase_timer<true,2> serial({"a","b"},1);
    serial.start(0);
    double t0=now();
    busy(0.05);
    serial.end_phase(0,a_phase);
    double t1=now();
    busy(0.15);
    serial.end_phase(0,b_phase);
    double t2=now();
    check(std::fabs(column_sum(serial,a_phase)-(t1-t0))<1e-3,"serial phase a",column_sum(serial,a_phase)-(t1-t0));
    check(std::fabs(column_sum(serial,b_phase)-(t2-t1))<1e-3,"serial phase b",column_sum(serial,b_phase)-(t2-t1));
    // 再次调用start()会丢掉之前经过的时间
    phase_timer<true,2> restarted({"a","b"},1);
    restarted.start(0);
    busy(0.05);
    double t3=now();
    restarted.start(0);
    busy(0.02);
    restarted.end_phase(0,a_phase);
    check(std::fabs(column_sum(restarted,a_phase)-(now()-t3))<1e-3,"time before the second start() is counted",column_sum(restarted,a_phase));
    // 多线程：每个线程写自己的一行，每列总和等于各线程自己记录的时间之和
    const int thread_num=4;
    phase_timer<true,2> parallel({"a","b"},thread_num);
    std::thread threads[thread_num];
    double own_a[thread_num],own_b[thread_num];
    for (int t=0;t<thread_num;t++)
        threads[t]=std::thread([&,t]
        {
            parallel.start(t);
            double begin=now();
            busy(0.02);
            parallel.end_phase(t,a_phase);
            double middle=now();
            busy(0.04);
            parallel.end_phase(t,b_phase);
            own_a[t]=middle-begin;
            own_b[t]=now()-middle;
        });
    for (auto &thread:threads)
        thread.join();
    double sum_a=0,sum_b=0;
    for (int t=0;t<thread_num;t++)
    {
        sum_a+=own_a[t];
        sum_b+=own_b[t];
    }
    check(std::fabs(column_sum(parallel,a_phase)-sum_a)<1e-3*thread_num,"parallel phase a",column_sum(parallel,a_phase)-sum_a);
    check(std::fabs(column_sum(parallel,b_phase)-sum_b)<1e-3*thread_num,"parallel phase b",column_sum(parallel,b_phase)-sum_b);
    // 每行单独占64字节的整数倍
    check(sizeof(phase_timer<true,7>::task_row)%64==0,"rows are not padded to cache lines");
    parallel.print("Test");
    printf(failures==0?"All tests passed\n":"%d tests failed\n",failures);
    return failures!=0;
}
