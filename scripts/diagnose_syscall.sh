#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p evidence
make kernel/kernel fs.img TOOLPREFIX=riscv64-linux-gnu-
python3 - <<'PY'
import os, signal, socket, subprocess, tempfile, time
from pathlib import Path
root=Path.cwd()
def diagnose(cwd, label, commands):
    script=root/'evidence'/f'{label}.gdb'
    script.write_text('set pagination off\nset confirm off\nset architecture riscv:rv64\nfile kernel/kernel\ntarget remote :26000\n'+commands+'\ndetach\nquit\n')
    with (root/'evidence'/f'{label}-qemu.txt').open('w') as out:
        q=subprocess.Popen(['make','qemu-gdb','TOOLPREFIX=riscv64-linux-gnu-','CPUS=1','GDBPORT=26000'],cwd=cwd,stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
        try:
            for _ in range(100):
                if q.poll() is not None: raise RuntimeError('QEMU exited')
                try:
                    with socket.create_connection(('localhost',26000),timeout=.2):pass
                    break
                except OSError:time.sleep(.1)
            else:raise TimeoutError('GDB port')
            r=subprocess.run(['gdb-multiarch','-q','-batch','-x',str(script)],cwd=cwd,capture_output=True,text=True,timeout=60)
            text=r.stdout+r.stderr
            (root/'evidence'/f'{label}.txt').write_text(text)
            assert r.returncode==0,text
            assert 'init' in text and 'PID=1' in text,text
            return text
        finally:
            os.killpg(q.pid,signal.SIGTERM)
            q.wait(timeout=10)
normal=diagnose(root,'normal', '''break syscall
continue
backtrace
set $p = cpus[0].proc
printf "PID=%d NAME=%s A7=%d SPP=%d\\n", $p->pid, $p->name, $p->trapframe->a7, ($sstatus >> 8) & 1
print /x *$p
print /x $sstatus''')
assert 'A7=15 SPP=0' in normal,normal
with tempfile.TemporaryDirectory() as tmp:
    arc=subprocess.run(['git','archive','HEAD'],capture_output=True,check=True).stdout
    subprocess.run(['tar','-x','-C',tmp],input=arc,check=True)
    p=Path(tmp)/'kernel/syscall.c';s=p.read_text();assert 'num = p->trapframe->a7;' in s
    p.write_text(s.replace('num = p->trapframe->a7;', 'num = *(volatile int *)0;'))
    subprocess.run(['make','kernel/kernel','fs.img','TOOLPREFIX=riscv64-linux-gnu-'],cwd=tmp,check=True)
    fault=diagnose(tmp,'null-load', '''break kerneltrap
continue
set $p = cpus[0].proc
printf "PID=%d NAME=%s SCAUSE=%d STVAL=%ld\\n", $p->pid, $p->name, $scause, $stval
print /x $sepc
x/i $sepc
backtrace''')
    assert 'SCAUSE=13 STVAL=0' in fault,fault
    (root/'evidence/null-kernel.asm').write_bytes((Path(tmp)/'kernel/kernel.asm').read_bytes())
print('PASS: normal first syscall, user previous mode, deliberate null-load and process identity')
PY
