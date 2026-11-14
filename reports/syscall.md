# MIT 6.1810 2026 syscall

官方：https://pdos.csail.mit.edu/6.828/2026/labs/syscall.html 。起点origin/syscall 99161db；不复用旧版trace/sysinfo任务。

interpose贯穿用户stub、编号、分发、进程字段；拒绝调用返回-1且不执行，open/exec只允许精确路径例外，短横线表示无例外。fork复制、exec保留、进程槽复用清零；重复设置只能增加mask或收紧路径，坏指针/过长字符串失败且保持原策略。该教学沙箱不是生产安全边界，不限制已有fd或路径别名。

attack仅改原user/attack.c，搜索分配页内公开标记及字母数字串，不修改secret、内存清零开关或官方测试。漏洞为当期有意关闭页初始化；不用于真实系统。

本地不编译/运行；远端原make grade和GDB验证完成前不得验收。answers中的编译地址须由真实远端诊断补齐。time.txt=0为本轮自动工作不足一小时按整小时记，不冒充学生个人耗时。Gradescope/校内身份未执行。
