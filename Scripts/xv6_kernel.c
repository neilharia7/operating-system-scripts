// the registers xv6 will save and restore to stop and subsequently restart a process

struct context {
    /*
    eip -> instruction pointer -> holds the address of the next instruction to be executed by the CPU
    when a process is paused (e.g. during a context switch),  the current value of `eip` is saved.
    when the process resumes, this value is restored, allowing the CPU to continue execution from
    where it left off.
     */
    int eip;
    /*
    esp -> stack pointer -> points to the top of the current stack
    (specifically the value of teh stack pointer in the process's memory). The stack is used for
    managing function calls, local variables, and returning from fuctions. Saving and restoring the `esp`
    ensures that the process's stack is maintained correctly when it is switched out and back in.
     */
    int esp;
    /*
    
     */

};