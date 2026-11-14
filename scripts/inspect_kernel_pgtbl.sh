#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p evidence
python3 - <<'PY'
import os, signal, socket, subprocess, time
from pathlib import Path
commands=r'''set pagination off
set confirm off
file kernel/kernel
target remote :26000
break syscall
continue
python
import gdb, struct, json
inf=gdb.selected_inferior()
leaves=[]
levels=[0,0,0]
def walk(pa, level, base):
    levels[level]+=1
    values=struct.unpack('<512Q',bytes(inf.read_memory(pa,4096)))
    for i,pte in enumerate(values):
        if not pte&1:continue
        va=base+(i<<(12+9*level)); phys=(pte>>10)<<12
        if pte&14:
            leaves.append({'va':va,'pa':phys,'level':level,'flags':pte&0x3ff})
        else:
            assert level>0
            walk(phys,level-1,va)
walk(int(gdb.parse_and_eval('kernel_pagetable')),2,0)
etext=int(gdb.parse_and_eval('(unsigned long)&etext'))
trampoline=int(gdb.parse_and_eval('(unsigned long)&trampoline'))
actual={}
for p in leaves:
    size=1<<(12+9*p['level'])
    assert p['level']<=1 and p['pa']%size==0 and p['va']%size==0
    for off in range(0,size,4096):
        assert p['va']+off not in actual
        actual[p['va']+off]=(p['pa']+off,p['flags']&30)
expected={}
def region(va,pa,size,perm):
    for off in range(0,size,4096):expected[va+off]=(pa+off,perm)
region(0x10000000,0x10000000,4096,6)
region(0x10001000,0x10001000,4096,6)
region(0xc000000,0xc000000,0x4000000,6)
region(0x80000000,0x80000000,etext-0x80000000,10)
region(etext,etext,0x88000000-etext,6)
top=(1<<38)-4096
region(top,trampoline,4096,10)
stackpas=[]
for i in range(64):
    va=top-i*8192-12288
    assert va in actual
    pa,perm=actual[va]
    assert 0x80000000<=pa<0x88000000 and pa%4096==0 and perm==6
    stackpas.append(pa)
    expected[va]=(pa,6)
assert len(set(stackpas))==64
assert actual==expected, (len(actual),len(expected))
baseline_tables=len({va>>21 for va in expected})
superpages=sum(p['level']==1 for p in leaves)
saved=baseline_tables-levels[0]
assert saved==superpages
result={'mapped_4k_units':len(actual),'superpages':superpages,'regular_leaf_ptes':sum(p['level']==0 for p in leaves),'baseline_bottom_tables':baseline_tables,'actual_bottom_tables':levels[0],'saved_bottom_tables':saved,'etext':hex(etext),'regions_permissions_and_stack_guards':'passed','leaves':leaves}
with open('evidence/kernel-mappings.json','w') as f:json.dump(result,f,indent=2)
print('PASS: complete kernel mappings, permissions, 64 stack guards, alignment and saved table count')
end
detach
quit
'''
Path('evidence/kernel-mappings.gdb').write_text(commands)
with Path('evidence/kernel-mappings-qemu.txt').open('w') as out:
    q=subprocess.Popen(['make','qemu-gdb','TOOLPREFIX=riscv64-linux-gnu-','CPUS=1','GDBPORT=26000'],stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
    try:
        for _ in range(100):
            if q.poll() is not None:raise RuntimeError('QEMU exited')
            try:
                with socket.create_connection(('localhost',26000),timeout=.2):pass
                break
            except OSError:time.sleep(.1)
        else:raise TimeoutError('GDB port')
        r=subprocess.run(['gdb-multiarch','-nx','-q','-batch','-x','evidence/kernel-mappings.gdb'],capture_output=True,text=True,timeout=90)
        Path('evidence/kernel-mappings-gdb.txt').write_text(r.stdout+r.stderr)
        assert r.returncode==0,r.stdout+r.stderr
        assert Path('evidence/kernel-mappings.json').exists()
    finally:
        os.killpg(q.pid,signal.SIGTERM)
        q.wait(timeout=10)
PY
