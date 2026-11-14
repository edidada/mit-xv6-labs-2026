# MIT xv6 2026 pgtbl

官方：https://pdos.csail.mit.edu/6.828/2026/labs/pgtbl.html ，起点d1eb62f6339ae811083a0c2f24fc496f8390fd9d。实现vmprint、pgaccess、只读PID共享页、内核2MiB superpages；不替换成旧版用户superpage题。

pgaccess上限4096页、按起址所在页扫描，位图ceil(n/8)；全范围/输出映射与写权限预验证，失败不清A位，成功copyout后清A并sfence。USYSCALL每进程单独分配，fork获得子PID、exec复用，失败与回收路径处理。仅内核完整对齐区间使用superpage，用户映射仍4KiB，文本/数据权限边界不合并。

本地不编译执行，原grader和测试保持；最终CI前未验收。time.txt=0仅本轮自动工作不足一个活动小时，不冒充学生个人记录；答案格式分非人工评分，Gradescope未执行。实际叶页解释、superpage数量与节约页表数待远端证据。
