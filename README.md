# OS assignment 5: Mdifications to xv6

command to run:
```bash
    run $SCHEDULER $CPUS
```
for example
```bash
    run FCFS 1
```
## Waitx sys_call and time user program
run
```bash
    time $COMMAND
```
for example
```bash
    time benchmark
```

time spawns a child process that execs the process and it returns the waittime and runtime of the child process.

## ps
run
```
    ps
```
It displays info of all active processes. It also displays information related to mlfq like the current q and the time spent in each of the 5 queues.

# Schedulers
## round robin
default scheduler, no changes were made.
## FCFS
We iterate through the proc table, looking for the task with least creation time.
As its not pre-emptive we do not yield in trap.c
    
 code snippet:
 ```c
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;
      if (curProc == 0)
      {
        curProc = p;
      }
      else if (curProc->startTime > p->startTime)
      {
        curProc = p;
      }
    }
```

### PBS
Iterate through the proc-table looking for the process with highest priority.
And execute that process
    
code snippet:
```c
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;
      curP = p;
      for (iterPr = ptable.proc; iterPr < &ptable.proc[NPROC]; iterPr++)
      {
        if (iterPr->state != RUNNABLE)
          continue;
        if (iterPr->priority < curP->priority)
          curP = iterPr;
      }
      if (curP == 0)
        ;
      else
      {
        c->proc = curP;
        switchuvm(curP);
        curP->state = RUNNING;
        curP->n_run++;
        swtch(&(c->scheduler), curP->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        curP = 0;
      }
    }
```
    
Implemented syscall set_priority that sets the priority of the process..
Also made user command setPriority
usage:
```bash
    setPriority $NEWPRIORITY $PID
```
    
### MLFQ
5 queues are declared (from 0 - 4)
