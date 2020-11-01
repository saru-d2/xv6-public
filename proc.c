#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
// #define MLFQ

//-----my code
int mlfqDelQueue(struct proc *p, int qNo);
int mlfqAppendQueue(struct proc *p, int qNo);

struct proc *mlfqQueue[5][NPROC + 5];
int mlfqTimeLimit[] = {1, 2, 4, 8, 16};
int mlfqAgeLimit = 5;
int mlfqQueueSize[] = {0, 0, 0, 0, 0};
//-----
struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void)
{
  // cprintf("allocproccing\n");
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  //------------my code
  p->ioTime = 0;
  p->startTime = ticks;
  p->endTime = 0;
  p->runTime = 0;
  for (int i = 0; i < 5; i++)
    p->tickQ[i] = 0;
  p->cur_q = 0;
  p->priority = 60;
  p->n_run = 0;
  p->mlfqTimeCur = 0;
  p->mlfqQueueEnterTime = 0;
  p->demote = 0;
#ifdef MLFQ
  mlfqAppendQueue(p, 0);
  #ifdef GRAPH
          cprintf("BORN");
          cprintf("\nGRAPH ADD: %d %d %d\n", p->pid, 0, ticks);
#endif
#endif
  //----------

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
#ifdef MLFQ
  mlfqAppendQueue(p, 0);
#endif

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

#ifdef MLFQ
  mlfqAppendQueue(np, 0);
#endif

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  ///
  // cprintf("@%d@\n", curproc->n_run);
  ///
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  //-------my code
  curproc->endTime = ticks;
  //-------

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
#ifdef MLFQ
        mlfqDelQueue(p, p->cur_q);
#endif
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

//----------------my code
int waitx(int *wtime, int *rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.

        //my mod to wait()
        *rtime = p->runTime;
        *wtime = p->endTime - p->runTime - p->startTime;
        //

        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
#ifdef MLFQ
        mlfqDelQueue(p, p->cur_q);
#endif
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}
//----------end of my code

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef RR
  struct proc *p;

  cprintf("RR\n");
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.

      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->n_run++;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
#endif
#ifdef FCFS
  struct proc *p;

  cprintf("FCFS\n");
  struct proc *curProc = 0;
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
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
    if (curProc != 0)
    {
      c->proc = curProc;
      switchuvm(curProc);
      curProc->state = RUNNING;
      //--mycode
      curProc->n_run++;
      //--
      swtch(&(c->scheduler), curProc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
    curProc = 0;
  }
#endif
#ifdef PBS
  struct proc *p;

  cprintf("PBS\n");
  struct proc *curP = 0, *iterPr;
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
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
    release(&ptable.lock);
  }
#endif
#ifdef MLFQ
  struct proc *curP = 0, *temp;
  cprintf("MLFQ\n");
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
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
    curP = 0;
    // cprintf("oogabooga %d\n", mlfqQueueSize[0]);
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
    if (curP != 0 && curP->state == RUNNABLE)
    {
      c->proc = curP;
      switchuvm(curP);
      curP->state = RUNNING;
      curP->n_run++;
      swtch(&(c->scheduler), curP->context);
      switchkvm();
      if (curP != 0)
      {
        int next_q = curP->cur_q;
        // cprintf("!%d %d!\n", curP->mlfqTimeCur, mlfqTimeLimit[curP->cur_q]);
        if (curP->demote == 1)
        {
          if (curP->cur_q < 4)
          {
            #ifdef GRAPH
            cprintf("DEMOTED %d\n", curP->pid);
            #endif
            next_q = curP->cur_q + 1;
          }
          curP->demote = 0;
        }
#ifdef GRAPH
        if (next_q != curP->cur_q)
          cprintf("\nGRAPH ADD: %d %d %d\n", curP->pid, next_q, ticks);
#endif
        // curP->mlfqTimeCur = 0;
        mlfqAppendQueue(curP, next_q);
      }
      c->proc = 0;
    }
    release(&ptable.lock);
    curP = 0;
  }
#endif


}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        //DOC: sleeplock0
    acquire(&ptable.lock); //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      mlfqAppendQueue(p, p->cur_q);
      p->mlfqTimeCur = 0;
    }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
      {
        p->state = RUNNABLE;

#ifdef MLFQ
        mlfqAppendQueue(p, 0);
#endif
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

//------------

int psx()
{
  struct proc *p;

  acquire(&ptable.lock);

  cprintf("PID\tPriority\tState\t\tr_time\tw_time\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    cprintf("%d\t%d\t\t", p->pid, p->priority);
    switch (p->state)
    {
    case SLEEPING:
      cprintf("SLEEPING\t");
      break;
    case RUNNING:
      cprintf("RUNNING \t");
      break;
    case RUNNABLE:
      cprintf("RUNNABLE\t");
      break;
    default:
      break;
    }
    cprintf("%d\t%d\t%d\t%d\t", p->runTime,ticks - p->startTime - p->runTime, p->n_run, p->cur_q);
    for (int i = 0; i < 5; i++)
      cprintf("%d\t", p->tickQ[i]);
    cprintf("\n");
  }

  release(&ptable.lock);
  return 1;
  exit();
}

int set_priority(int nPri, int pid)
{
  cprintf("in set_priority %d %d\n", nPri, pid);

  if (nPri > 100 || nPri < 0)
    return -1;
  acquire(&ptable.lock);
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      int temp = p->priority;
      p->priority = nPri;
      release(&ptable.lock);
      cprintf("Priority of pid %d changed from %d to %d\n", pid, temp, nPri);
      return 1;
    }
  }
  release(&ptable.lock);
  return -1;
}

int updateProcTime()
{
  struct proc *p;
	acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == RUNNING)
    {
      p->runTime++;
      p->tickQ[p->cur_q]++;
      p->mlfqTimeCur++;
    }
    else if (p->state == SLEEPING)
      p->ioTime++;
  }
  release(&ptable.lock);
  return 1;
}

int mlfqDelQueue(struct proc *p, int qNo)
{
  // cprintf("removing from q %d\n", qNo);
  int pid = p->pid;
  int ind = -1;
  for (int i = 0; i < mlfqQueueSize[qNo]; i++)
  {
    if (mlfqQueue[qNo][i]->pid == pid)
    {
      ind = i;
      break;
    }
  }
  if (ind == -1)
    return -1;
  for (int i = ind; i < mlfqQueueSize[qNo] - 1; i++)
  {
    mlfqQueue[qNo][i] = mlfqQueue[qNo][i + 1];
  }
  mlfqQueueSize[qNo]--;

  return 1;
}

int mlfqAppendQueue(struct proc *p, int qNo)
{
  // cprintf("appending to q %d\n", qNo);
  int pid = p->pid;
  for (int i = 0; i < mlfqQueueSize[qNo]; i++)
  {
    if (mlfqQueue[qNo][i]->pid == pid)
      return -1;
  }
  mlfqQueue[qNo][mlfqQueueSize[qNo]] = p;
  p->mlfqQueueEnterTime = ticks;
  p->cur_q = qNo;
  mlfqQueueSize[qNo]++;

  return 1;
}

int demoteProc(struct proc* p){
  acquire(&ptable.lock);
  p->demote = 1;
  release(&ptable.lock);
  return 1;
}
//------------
