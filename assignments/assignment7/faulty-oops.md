# Analysis of faulty.ko oops message in ARM64 architecture
## The oops message

**`Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000`**  *>  Main reason of the failure*\
Mem abort info:\
  ESR = 0x0000000096000045\
  EC = 0x25: DABT (current EL), IL = 32 bits\
  SET = 0, FnV = 0  
  EA = 0, S1PTW = 0  
  FSC = 0x05: level 1 translation fault\
Data abort info:\
  ISV = 0, ISS = 0x00000045\
  CM = 0, WnR = 1\
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041bed000\
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000\
Internal error: Oops: 0000000096000045 [#1] SMP\
Modules linked in: hello(O) faulty(O) scull(O)\
CPU: 0 PID: 159 Comm: sh Tainted: G           O       6.1.44 #1\
Hardware name: linux,dummy-virt (DT)\
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)\
pc : faulty_write+0x10/0x20 [faulty]\
lr : vfs_write+0xc8/0x390    *>The return address of the function that called the current one. This is often where the "guilty" caller is found*\
sp : **`ffffffc008e13d20`**  *> The current stack pointer address (in kernel space)*\
`x29: ffffffc008e13d80 x28: ffffff8001b2c240 x27: 0000000000000000`  *>  ARM64 30xRegisters status, important lr, pc(program counter), sp(stack pointer - the current top of the stack), far_el1. lr (Link Register/x30): The return address of the function that called the current one.*\
`x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000`\
`x23: 0000000000000006 x22: 0000000000000006 x21: ffffffc008e13dc0`\
`x20: 000000556b77a9f0 x19: ffffff8001bdc700 x18: 0000000000000000`\
`x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000`\
`x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000`\
`x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000`\
`x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000`\
`x5 : 0000000000000001 x4 : ffffffc000787000 x3 : ffffffc008e13dc0`\
`x2 : 0000000000000006 x1 : 0000000000000000 x0 : 0000000000000000`\
Call trace:\
 faulty_write+0x10/0x20 [faulty]   ***>  3.The VFS calls your module's faulty_write function, in [faulty] k-module. The error happens at 0x10/0x20 -> 16bytes/32bytes func.***\
 ksys_write+0x74/0x110             ***> 2. Kernel hits vfs_write***\
 __arm64_sys_write+0x1c/0x30  ***> 1. User calls a syscall write***\
 invoke_syscall+0x54/0x130\
 el0_svc_common.constprop.0+0x44/0xf0\
 do_el0_svc+0x2c/0xc0\
 el0_svc+0x2c/0x90\
 el0t_64_sync_handler+0xf4/0x120\
 el0t_64_sync+0x18c/0x190\
Code: d2800001 d2800000 d503233f d50323bf **(b900003f)**  *>   In ARM64 assembly, this is an str (store) instruction*\
---[ end trace 0000000000000000 ]---\
