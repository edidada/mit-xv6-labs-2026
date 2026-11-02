# MIT 6.1810 Fall 2026 util 实验报告

源码基线来自 MIT 官方 `git://g.csail.mit.edu/xv6-labs-2026`，util commit `8138d07d83441198b898e37d22840f5900cc0c32`；本仓库为官方 Git 镜像，不是社区旧年份 fork。实验分支 `edidada/util`。

复用先前本人 2025 util 中的 sleep/find 框架，逐项对比当期接口和官方新测试后移植；没有复用旧年的评分结果。sleep 调用 pause；sixfive 用取模累计判断完整数字，避免整型溢出，输出规范化前导零，新增无文件参数时从 stdin 读取。find 递归遍历，跳过点目录，检查路径长度和参数上限，支持 -exec 并等待子进程。

新版 memdump 接口额外传 len：每种格式先检查剩余字节，S 在 len 范围内读取，遇零或输入结束停止；固定宽度字段用 memmove 读取以避免未对齐访问，p 输出完整 64 位十六进制。固定宽度字段截断输入报 not enough data，不读取清零后的无效区域；示例年份保持官方 2026。s 只用于题目提供的有效指针示例，不把任意 stdin 地址当作可信字符串。

保留官方 grade-lab-util 不变；独立 util.yml 安装 RISC-V GCC、QEMU 和 GDB，执行完整 make grade，并检查最终得分等于满分，保存 util-grade.txt 和所有 QEMU 输出。该测试覆盖新版 stdin、文件缺失、h/c 格式、截断和 usage、递归查找与执行边界。本地未编译，最终通过以本分支完整 Actions 为准；未向 Gradescope 提交。

首次远程评分 145/156：S 应允许 EOF 结束的文本，现改为逐字节有界输出。time.txt 为 0，表示自动化完成、未申报人工学习小时，不能用它估计学习所需时间。

提交 8584d66 的官方评分已达 156/156，但 workflow 自检读取的 grade.log 被上游 make clean 的 *.log 清理规则移除。因此日志改为 util-grade.txt，重新验证整个 workflow；未改动官方评分脚本。
