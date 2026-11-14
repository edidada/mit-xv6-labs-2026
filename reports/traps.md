# MIT xv6 2026 traps

官方：https://pdos.csail.mit.edu/6.828/2026/labs/traps.html 。起点cdb46aaccca2b22dc7422667f186969d3b31b1fc。

backtrace在printk.c按fp-8/16取返回地址与前帧，检查同一栈页、对齐、递增与边界，接入sys_pause及panic。alarm以用户定时中断计数，保存完整trapframe、禁止嵌套，sigreturn恢复包括a0的上下文；handler内禁用不销毁待恢复状态。负间隔/越界handler拒绝，地址0允许。新进程槽清零，因此fork子进程默认无alarm配置；exec成功清理旧镜像配置，exec失败不清。

原alarmtest/grader不修改，本地不编译运行，无新增Python源文件。远端原grade、usertests与独立诊断完成前未验收；call.asm精确地址和答案待实际工件。time.txt=0表示本轮自动操作不足一活动小时，不是学生个人耗时；问答格式分不替代人工评阅，Gradescope未执行。
