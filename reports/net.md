# xv6 2026 networking

官方：https://pdos.csail.mit.edu/6.828/2026/labs/net.html ，起点cfc4a80d5460a22ada2ea2097d779de8dc666d48。

2026使用DEC 21143 Tulip；四字32位DMA描述符、16项环、OWN所有权、末项bit25、CSR每8字节间隔。参考原厂EC-QWC4D-TE手册3.2/4.2/4.3，官方链接404时使用存档 https://rsync.osuosl.org/pub/netwinder/pub/netwinder/docs/nw/tulip/21143hrm.pdf 。发送非阻塞，满环丢包并回收；完成中断及时释放闲置发送页。接收重新挂入新页后交付net_rx，缺页时丢包并复用原DMA页。独立收发锁避免ARP回复递归加锁，屏障先准备描述符再交付OWN。

UDP使用128个绑定槽，每端口最多16包FIFO，未绑定/溢出丢包。元数据复用不再需要的包头，不额外分配页。bind重复失败；unbind释放待收队列并唤醒等待者，generation防止解绑后同槽重新绑定被旧接收者误读。recv按当前sleep_prepare/sleep协议等待，复制源IP/端口及截断payload后释放原页，copyout失败也回收。IPv4版本/长度/分片/协议/目的地址及UDP长度基本校验；不实现完整协议栈、可选TCP/完整ARP等挑战。

原nettest/nettest.py/grader/gradelib不改，远端先运行原171分。本地不编译运行，无新增Python源文件。当前实现待CI及独立边界测试，尚未验收。time=0仅自动活动不足一小时，非个人学习时长；Gradescope未操作。
