# MIT xv6 2026 pgtbl

官方：https://pdos.csail.mit.edu/6.828/2026/labs/pgtbl.html ，起点d1eb62f6339ae811083a0c2f24fc496f8390fd9d。实现vmprint、pgaccess、只读PID共享页、内核2MiB superpages；不替换成旧版用户superpage题。

pgaccess上限4096页、按起址所在页扫描，位图ceil(n/8)；全范围/输出映射与写权限预验证，失败不清A位，成功copyout后清A并sfence。USYSCALL每进程单独分配，fork获得子PID、exec复用，失败与回收路径处理。仅内核完整对齐区间使用superpage，用户映射仍4KiB，文本/数据权限边界不合并。

本地不编译执行，原grader和测试保持；最终CI前未验收。time.txt=0仅本轮自动工作不足一个活动小时，不冒充学生个人记录；答案格式分非人工评分，Gradescope未执行。实际叶页解释、superpage数量与节约页表数待远端证据。

远端已观察原46/46及完整usertests ALL TESTS PASSED；独立程序确认33页位图只写5字节、重复查询清零、错误指针/只读输出失败保留A位、负数/0/4097页/MAXVA范围错误、USYSCALL映射RU及真实写/执行fault、fork/exec PID一致。初轮%p参数类型、零分usertests日志标签和大小写十六进制解析问题均已修正；原测试不改。

run43581178049实时统计95大页、579普通叶，95页底层页表（389120字节）节省；20个print_pgtbl条目和vmprint叶逐项解释见answers-pgtbl.txt。64MiB PLIC使用32大页、128MiB RAM的完整对齐段使用63大页；首2MiB保留文本/数据分界普通页。独立GDB读取每个实际PTE、展开为4KiB单位，与全部设备/RAM/栈/守护页的预期映射和权限对照，再由虚拟区间计算全4KiB基准底层表数与实际数的差值；最终提交CI通过后才可验收该检查。
