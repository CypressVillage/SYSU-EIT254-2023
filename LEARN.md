## 一些笔记和问题

### key_Task

> [key.c](main/key.c)

Task里先管理了一下开机和恢复出厂设置，然后创建了一个定时器中断，周期性读取按键状态，在KeyScan函数中处理。而主循环里则是检测旋钮状态，并把状态发送到gui_evt_queue这个消息队列中

- 为什么要开两次机？

- 其实KeyScan里也是将按键状态发送到gui_evt_queue中进行消息处理，为什么不把他们放到一起？

## adc_task

> [adc_dac.c](main/adc_dac.c)


首先清空当前测量任务，创建定时向网络发送测量值的服务，然后进入一个巨大的死循环

死循环里，首先检测通断档进行蜂鸣器的鸣响时间设置，然后等待测量过程结束。此时一个新的测量周期开始了。

一个测量周期内，首先对电池的状进行电源管理，关机/显示电量；然后储存上一次测量的功能数据，然后开始测量，测量结束后，将测量数据发送到gui_evt_queue（屏幕显示）中，然后等待下一次测量周期。

- gear_sw：当前档位状态，定义在[switch_fun.h](main/switch_fun.c)中，它的值是一个emun，[adc_dac.c](main/adc_dac.c)中用途是在具体测量函数中判断档位

- current_fun：也是档位状态，但是更加笼统，只是描述了测量的对象，没有包括量程，作用是进入具体的的测量函数

- 1693行和1694行是创建了一个定时向网络发送测量值的定时中断服务，**注释掉了**

- 关于 `no_need_up` 变量，它的含义是是否刷新数据，这意味着它不仅仅控制着是否向 App 推送数据，还控制着是否向显示屏推送数据。所以通过修改它隔绝 WIFI 功能是不现实的。

- 这个文件里的修改：

    - 删掉了 1685 行的定时器初始化，因为这个定时器是用来向网络发送数据的，现在不需要了

> [calibration.c](main/calibration.c)   校准/0

### switch_fun_task

> [switch_fun.h](main/switch_fun.h)   档位/量程

包含一个 `switch_fun_task` 任务，先写日志，init i2c，通过AW9523选择档位，输出16V电源，写 GP8403，输出直流电压

- `analog_switch` 切换模拟开关，调用 `control_sw` ，输入 AW9523，推测可以通过更改 sw 的值来控制开关。其实这个task主要就实现了这一个功能

- 579行的 `gpio_set_level(V16_EN, 1);` 是用来输出16V电压供到GP8403的

- GP8403 是 DAC ，用来提供直流电压

### lcd_ui_task

首先创建 gui_event_queue 队列，用来接收 gui 控制的消息，然后创建一个定时器，用来实现长时间不使用时熄灭屏幕的功能，这里做了更改，由10s改为1min。接着就是相关硬件的init，将屏幕置为黑色，绘制边框，调节屏幕背光亮度，进入主循环。主循环里检测 `ui_display_st` 的值进行屏幕内容切换和档位切换。

### wifi_net_task

没有细看，网络部分就不管他啦

### usart0_task

主管接收数据，执行模拟开关的切换