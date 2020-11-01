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
We add a process to q0 everytime a process becomes runnable
Ageing is realized through iterating through every queue thats not q0, and checking if time spent in that queue is greater than the agelimit for that queue (2^i)

```c
for (int i = 1; i < 5; i++)
    {
      for (int j = 0; j < mlfqQueueSize[i]; j++)
      {
        temp = mlfqQueue[i][j];
        int procAge = ticks - temp->mlfqQueueEnterTime;
        if (procAge > mlfqAgeLimit)
        {
          //move process to previous tier
          //like 1-> 0
          mlfqDelQueue(temp, i);
          mlfqAppendQueue(temp, i - 1);
          temp->mlfqTimeCur = 0;
#ifdef GRAPH
cprintf("PROMOTED\n");
          cprintf("\nGRAPH ADD: %d %d %d\n", temp->pid, i - 1, ticks);
#endif
        }
      }
    }
```

then we go over the 'head' of each queue and select the first one we get, and if its runnable we remove it from the queue
```c
for (int i = 0; i < 5; i++)
    {
      if (mlfqQueueSize[i] > 0)
      {
        curP = mlfqQueue[i][0];
        //remove proc
        mlfqDelQueue(curP, i);
        break;
      }
    }
```
then we execute it!.


In trap.c, right before yielding, we check if the process has exhausted the time in that queue. if it has we move it to the next queue.

```c
#ifdef MLFQ
  int ageLim[] = {1, 2, 4, 8, 16};
    if (myproc()->mlfqTimeCur > ageLim[myproc()->cur_q])
    {
      // myproc()->demote = 1;
      demoteProc(myproc());
      yield();
    }
#endif
```

##BONUS
so whenever a queue is changes for a process i output a line as follows
```c
          cprintf("\nGRAPH ADD: %d %d %d\n", p->pid, p->cur_q, ticks);
```
and i redirect the output to a file, from where i run a python script to make the graph. Library used: matlabplot.pyplot
