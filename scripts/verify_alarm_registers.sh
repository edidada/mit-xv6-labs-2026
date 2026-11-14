#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p evidence
python3 - <<'PY'
import json, os, signal, socket, subprocess, threading, time
from pathlib import Path
commands=r'''set pagination off
set confirm off
file kernel/kernel
target remote :26000
break sys_sigreturn
continue
python
import gdb, json
p=gdb.parse_and_eval('cpus[0].proc').dereference()
assert int(p['alarm_active'])==1
saved=p['alarm_saved']
fields=[f.name for f in saved.type.fields() if not f.name.startswith('kernel_')]
assert len(fields)==32
snapshot={f:int(saved[f]) for f in fields}
end
finish
break prepare_return
continue
python
p=gdb.parse_and_eval('cpus[0].proc').dereference()
assert int(p['alarm_active'])==0
live=p['trapframe'].dereference()
actual={f:int(live[f]) for f in fields}
assert actual==snapshot,(snapshot,actual)
with open('evidence/register-restore.json','w') as out:json.dump({'user_fields':fields,'saved':snapshot,'restored':actual,'all_31_registers_and_pc':'passed'},out,indent=2)
print('PASS: all 31 user registers and interrupted PC restored after syscall return-value write')
end
detach
quit
'''
Path('evidence/register-restore.gdb').write_text(commands)
q=subprocess.Popen(['make','qemu-gdb','TOOLPREFIX=riscv64-linux-gnu-','CPUS=1','GDBPORT=26000'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
def console():
    tail=b''; sent=False
    with Path('evidence/register-restore-qemu.txt').open('wb') as out:
        while True:
            c=q.stdout.read(1)
            if not c:break
            out.write(c);out.flush();tail=(tail+c)[-256:]
            if not sent and tail.endswith(b'$ '):
                q.stdin.write(b'alarmtest\n');q.stdin.flush();sent=True
thread=threading.Thread(target=console,daemon=True);thread.start()
try:
    for _ in range(100):
        if q.poll() is not None:raise RuntimeError('QEMU exited')
        try:
            with socket.create_connection(('localhost',26000),timeout=.2):pass
            break
        except OSError:time.sleep(.1)
    else:raise TimeoutError('GDB port')
    r=subprocess.run(['gdb-multiarch','-nx','-q','-batch','-x','evidence/register-restore.gdb'],capture_output=True,text=True,timeout=90)
    Path('evidence/register-restore.txt').write_text(r.stdout+r.stderr)
    assert r.returncode==0,r.stdout+r.stderr
    assert Path('evidence/register-restore.json').exists()
finally:
    os.killpg(q.pid,signal.SIGTERM);q.wait(timeout=10);thread.join(timeout=5)
Path('evidence/commit.txt').write_text(subprocess.check_output(['git','rev-parse','HEAD'],text=True))
PY
