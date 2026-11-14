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
break kfork
continue
finish
python
import gdb,struct,json
inf=gdb.selected_inferior()
def leaf(table,va):
    for level in (2,1,0):
        addr=table+((va>>(12+9*level))&511)*8
        pte=struct.unpack('<Q',bytes(inf.read_memory(addr,8)))[0]
        assert pte&1
        if pte&14:return pte
        table=(pte>>10)<<12
    raise AssertionError('no leaf')
def ref(pa):return int(gdb.parse_and_eval('kmem.refs[%d]'%((pa-0x80000000)//4096)))
parent=gdb.parse_and_eval('cpus[0].proc').dereference()
pid=int(parent['pid'])
children=[]
for i in range(64):
    p=gdb.parse_and_eval('proc[%d]'%i)
    if int(p['state'])!=0 and int(p['parent'])==int(parent.address):children.append(p)
assert len(children)==1
child=children[0]
shared=[]
for va in range(0,int(parent['sz']),4096):
    a=leaf(int(parent['pagetable']),va);b=leaf(int(child['pagetable']),va)
    assert (a>>10)==(b>>10) and not a&4 and not b&4
    assert (a&0x11f)==(b&0x11f)
    pa=(a>>10)<<12
    assert ref(pa)==2
    shared.append({'va':va,'pa':pa,'parent_flags':a&1023,'child_flags':b&1023,'refs':ref(pa)})
assert any(e['parent_flags']&256 for e in shared)
assert any((e['parent_flags']&10)==10 and not e['parent_flags']&256 for e in shared)
end
break vmfault if read == 0
continue
python
p=gdb.parse_and_eval('cpus[0].proc').dereference()
va=int(gdb.parse_and_eval('va'))&~4095
before=leaf(int(p['pagetable']),va)
assert before&256 and before&16 and not before&4
oldpa=(before>>10)<<12
oldref=ref(oldpa)
oldbytes=bytes(inf.read_memory(oldpa,4096))
end
finish
python
new=leaf(int(p['pagetable']),va)
newpa=(new>>10)<<12
assert new&4 and not new&256 and newpa!=oldpa
assert ref(newpa)==1 and ref(oldpa)==oldref-1
assert bytes(inf.read_memory(newpa,4096))==oldbytes
result={'fork_pages':shared,'fork_refcounts_exact':True,'readonly_text_stays_readonly':True,'split_va':va,'old_pa':oldpa,'new_pa':newpa,'old_ref_before':oldref,'old_ref_after':ref(oldpa),'new_ref':ref(newpa),'copied_4096_bytes_equal':True,'split_flags':new&1023}
with open('evidence/sharing.json','w') as out:json.dump(result,out,indent=2)
print('PASS: fork physical sharing and exact refs; real write-fault split and unchanged copied bytes')
end
detach
quit
'''
Path('evidence/sharing.gdb').write_text(commands)
with Path('evidence/sharing-qemu.txt').open('w') as out:
    q=subprocess.Popen(['make','qemu-gdb','TOOLPREFIX=riscv64-linux-gnu-','CPUS=1','GDBPORT=26000'],stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
    try:
        for _ in range(100):
            if q.poll() is not None:raise RuntimeError('QEMU exited')
            try:
                with socket.create_connection(('localhost',26000),timeout=.2):pass
                break
            except OSError:time.sleep(.1)
        else:raise TimeoutError('GDB port')
        r=subprocess.run(['gdb-multiarch','-nx','-q','-batch','-x','evidence/sharing.gdb'],capture_output=True,text=True,timeout=90)
        Path('evidence/sharing-gdb.txt').write_text(r.stdout+r.stderr)
        assert r.returncode==0,r.stdout+r.stderr
        assert Path('evidence/sharing.json').exists()
    finally:
        os.killpg(q.pid,signal.SIGTERM);q.wait(timeout=10)
PY
