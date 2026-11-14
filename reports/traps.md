# MIT xv6 2026 traps

官方：https://pdos.csail.mit.edu/6.828/2026/labs/traps.html 。起点cdb46aaccca2b22dc7422667f186969d3b31b1fc。

backtrace在printk.c按fp-8/16取返回地址与前帧，检查同一栈页、对齐、递增与边界，接入sys_pause及panic。alarm以用户定时中断计数，保存完整trapframe、禁止嵌套，sigreturn恢复包括a0的上下文；handler内禁用不销毁待恢复状态。负间隔/越界handler拒绝，地址0允许。新进程槽清零，因此fork子进程默认无alarm配置；exec成功清理旧镜像配置，exec失败不清。

原alarmtest/grader不修改，本地不编译运行，无新增Python源文件。远端原grade、usertests与独立诊断完成前未验收；call.asm精确地址和答案待实际工件。time.txt=0表示本轮自动操作不足一活动小时，不是学生个人耗时；问答格式分不替代人工评阅，Gradescope未执行。

首个原版完整成功a6c220a/run43582093340，95/95，backtrace三地址由原addr2line校验sysproc/syscall/trap，四alarm测试及完整usertests通过。独立检查额外验证负interval与越界handler拒绝且原配置仍生效、handler内禁用、不活跃sigreturn返回-1、fork默认不继承和exec清理、实际端序输出HE110 World（当前printf十六进制大写）。call.asm表明main将f/g内联常量折叠，0x30的relaxed jal调用0x70e printf，ra=0x34，不能照抄讲义jalr地址。

独立GDB在真实sys_sigreturn入口保存alarm_saved的31用户寄存器和epc，再到用户返回准备点逐字段比较，覆盖syscall分发器写返回值后的a0；命令、QEMU文本、保存/恢复JSON入工件。该检查、边界与最终提交全部CI通过后才验收；原alarmtest/bttest/call/grader/gradelib不改。
