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

int main()
{
    enum {a_phase,b_phase};
    // 关闭时什么都不记录
    phase_timer<false,2> disabled({"a","b"},4);
    auto disabled_lap=disabled.task(0);
    disabled_lap.lap(a_phase);
    check(disabled.total(a_phase)==0,"disabled timer records time");
    // 串行：阶段时间与在同样位置自己记录的时间一致。机器很忙时两者都会变长，但仍应一致
    phase_timer<true,2> serial({"a","b"},1);
    auto lap_timer=serial.task(0);
    double t0=now();
    busy(0.05);
    lap_timer.lap(a_phase);
    double t1=now();
    busy(0.15);
    lap_timer.lap(b_phase);
    double t2=now();
    check(std::fabs(serial.total(a_phase)-(t1-t0))<1e-3,"serial phase a",serial.total(a_phase)-(t1-t0));
    check(std::fabs(serial.total(b_phase)-(t2-t1))<1e-3,"serial phase b",serial.total(b_phase)-(t2-t1));
    // restart()之前的时间不计入
    phase_timer<true,2> restarted({"a","b"},1);
    auto restarted_lap=restarted.task(0);
    busy(0.05);
    double t3=now();
    restarted_lap.restart();
    busy(0.02);
    restarted_lap.lap(a_phase);
    check(std::fabs(restarted.total(a_phase)-(now()-t3))<1e-3,"time before restart() is counted",restarted.total(a_phase));
    // 多线程：每个线程用自己任务的计时器，结果等于各线程自己记录的时间之和
    const int thread_num=4;
    phase_timer<true,2> parallel({"a","b"},thread_num);
    std::thread threads[thread_num];
    double own_a[thread_num],own_b[thread_num];
    for (int t=0;t<thread_num;t++)
        threads[t]=std::thread([&,t]
        {
            auto thread_lap=parallel.task(t);
            double begin=now();
            busy(0.02);
            thread_lap.lap(a_phase);
            double middle=now();
            busy(0.04);
            thread_lap.lap(b_phase);
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
    check(std::fabs(parallel.total(a_phase)-sum_a)<1e-3*thread_num,"parallel phase a",parallel.total(a_phase)-sum_a);
    check(std::fabs(parallel.total(b_phase)-sum_b)<1e-3*thread_num,"parallel phase b",parallel.total(b_phase)-sum_b);
    parallel.print("Test");
    printf(failures==0?"All tests passed\n":"%d tests failed\n",failures);
    return failures!=0;
}
