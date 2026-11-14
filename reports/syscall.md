# MIT 6.1810 2026 syscall

官方：https://pdos.csail.mit.edu/6.828/2026/labs/syscall.html 。起点origin/syscall 99161db；不复用旧版trace/sysinfo任务。

interpose贯穿用户stub、编号、分发、进程字段；拒绝调用返回-1且不执行，open/exec只允许精确路径例外，短横线表示无例外。fork复制、exec保留、进程槽复用清零；重复设置只能增加mask或收紧路径，坏指针/过长字符串失败且保持原策略。该教学沙箱不是生产安全边界，不限制已有fd或路径别名。

attack仅改原user/attack.c，搜索分配页内公开标记及字母数字串，不修改secret、内存清零开关或官方测试。漏洞为当期有意关闭页初始化；不用于真实系统。

本地不编译/运行；远端原make grade和GDB验证完成前不得验收。answers中的编译地址须由真实远端诊断补齐。time.txt=0为本轮自动工作不足一小时按整小时记，不冒充学生个人耗时。Gradescope/校内身份未执行。

初次完整验收：b4972f8 / run43557901617 原评分60/60（含8组sandbox、attack和答案/时间格式），96组独立隔离场景通过；边界涵盖坏指针、MAXPATH无终止符、精确路径别名拒绝、失败更新原子性、重复策略收紧、子进程策略不影响父进程、96次槽位复用和mask符号位。原grader、sbtest、secret、gradelib与清零条件均保持原版本。最初内核strcmp未提供，已换strncmp；独立runner须经原test/run_tests初始化。诊断初停启动中断，已限定scause=13。

GDB：首次open15、init/pid1、SPP0；空指针副本lw a5,0(zero)，scause13/stval0/sepc80001d76。此地址为本工具链诊断结果；脚本将GDB反汇编与该副本kernel.asm逐指令核对。最终提交须再次通过全部CI，永久工件包括原评分、完整QEMU输出、GDB命令/输出/空指针副本反汇编，不能把格式分等同人工答案评阅或MIT成绩。
