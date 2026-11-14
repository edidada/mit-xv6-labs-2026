# xv6 2026 COW

官方：https://pdos.csail.mit.edu/6.828/2026/labs/cow.html ，起点04d468d0726182082437dd957626f292d51dbaf4。

uvmcopy共享物理页，只有原可写页置RSW COW并清W，真只读页保持；保留lazy空洞。引用计数与freelist同锁，分配为1，fork增引用，最后释放才填充/归链；失败子映射回滚，不提前释放共享页。vmfault仅对用户COW写访问复制，copyout复用同路径，真实只读与guard保持保护；分配失败由原trap处理终止进程，内核copyout返回错误。每次写故障都分配复制，即便只剩一个引用；不声称实现可选的单引用优化。

本地仅编辑/静态检查，无新增Python源文件。原cowtest/grader/gradelib和usertests不改，最终CI前未验收。time=0仅自动活动不足一小时，非学生个人时间；Gradescope未执行。

独立远端检查：8轮各4子进程，共32组跨两页pipe read/copyout与其他页直接写，父进程逐字节保持；lazy空洞在子进程写入、父进程仍读取零；子进程真正耗尽物理内存（含剩余pipe缓冲页）后写共享页，必须异常退出且父数据保持。GDB另在首个fork完成时检查父子每页PA一致、W清除、文本无COW、引用精确2；真实写故障返回后检查新PA、W与COW标志、旧/新引用变化及整4096字节副本一致。最终CI前不声称这些额外检查通过。
