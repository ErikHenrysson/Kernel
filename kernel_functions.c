#include "globalvar.h"
#include "linkedlist.h"
#include "kernel_functions.h"
#include "stdio.h"

//Global variables
int Ticks;
int KernelMode;
TCB *PreviousTask, *NextTask;
list *ReadyList, *WaitingList, *TimerList;
int leavingObj;

//Functions
//LABB 1
exception init_kernel (void);
exception create_task(void(*task_body)(), uint Deadline);
void terminate (void);
void run (void);

//LABB 2
mailbox* create_mailbox (uint nMessages, uint nDataSize);
exception remove_mailbox (mailbox* mBox);
exception send_wait( mailbox* mBox, void* pData);
exception receive_wait ( mailbox* mBox, void* pData);
exception send_no_wait( mailbox *mBox, void* pData);
exception receive_no_wait( mailbox* mBox, void* pData);

//LABB3
exception wait(uint nTicks);
void set_ticks(uint nTicks);
uint ticks(void);
uint deadline(void);
void set_deadline(uint deadline);
void TimerInt(void);
  
void idle(void) //idle task for ReadyList structure
{while(1);}

exception init_kernel (void) //initializes global variables and creates idle task
{
  Ticks = 0;
  ReadyList = (list*)calloc(1,sizeof(list));
  if(ReadyList == 0)
    return FAIL;
  WaitingList = (list*)calloc(1,sizeof(list));
  if(WaitingList == 0)
    return FAIL;
  TimerList = (list*)calloc(1,sizeof(list));
  if(TimerList == 0)
    return FAIL;
  
  //init all the lists
  ReadyList->pHead = NULL;
  ReadyList->pTail = NULL;
  WaitingList->pHead = NULL;
  WaitingList->pTail = NULL;
  TimerList->pHead = NULL;
  TimerList->pTail = NULL;
  
  create_task(idle,UINT_MAX);
  KernelMode = INIT;
  
  return OK;
}


exception create_task(void(*task_body)(), uint Deadline) //creates a task and puts it into the readylist
{
  //allocate memory for the new task
  TCB* newTask;
  newTask = (TCB*)calloc(1, sizeof(TCB));
  if(newTask==0)
    return FAIL;
  //set the structure of the task
  newTask->Deadline=Deadline+Ticks;
  newTask->PC = task_body;
  newTask->SPSR = 0x21000000;
  newTask->StackSeg [STACK_SIZE - 2] = 0x21000000;
  newTask->StackSeg [STACK_SIZE - 3] = (unsigned int) task_body;
  newTask->SP = &(newTask->StackSeg [STACK_SIZE - 9]);  
  
  //allocate memory for the listobject
  listobj* newObj;
  newObj = (listobj*)calloc(1, sizeof(listobj));
  if (newObj == 0)
    return FAIL;
  //set the structure of the listobject
  newObj->pTask = newTask;
  newObj->pNext = NULL;
  newObj->pPrevious = NULL;
  
  //if run hasnt been called its ok to insert directly into the readylist
  if(KernelMode==INIT)
    insert(ReadyList, newObj);
  else //otherwise:
  {
  isr_off(); //disable interrupts
  PreviousTask = NextTask;
  insert(ReadyList, newObj); //insert into readylsit
  NextTask = ReadyList->pHead->pTask;
  SwitchContext();  //switch context and !!enable interrupts!!
  }
  return OK;
}



void terminate (void) //ends a task
{
  isr_off();
  listobj* leavingObj;
  //extract the currently running task
  leavingObj = extractFirst(ReadyList); 
  NextTask = ReadyList->pHead->pTask;
  switch_to_stack_of_next_task();
  //free up the task and listobj
  free(leavingObj->pTask);
  free(leavingObj);
  LoadContext_In_Terminate();
 }

void run (void) //starts the RTMK
{
  Ticks = 0;
  KernelMode = RUNNING;
  NextTask = ReadyList->pHead->pTask;
  LoadContext_In_Run();
}


mailbox* create_mailbox (uint nMessages, uint nDataSize) //create a mailbox of specified datasize
{
  mailbox* mbox= (mailbox*)calloc(1, sizeof(mailbox));
  if(mbox == NULL) //if the memory allocation did not succeed, dont proceed
    return FAIL;
  //init values
  mbox->nMaxMessages= nMessages; //set size of mailbox 
  mbox->nDataSize = nDataSize; //set 
  mbox->nBlockedMsg=NULL;
  mbox->nMessages=NULL;
  mbox->pHead=NULL;
  mbox->pTail=NULL;
  mbox->pHead->pData=NULL;
  mbox->pTail->pData=NULL;
   
  return mbox;
}

exception remove_mailbox (mailbox* mBox)//removes mailbox
{
  if(mBox->nMessages == 0)//the mailbox is empty and can be removed
  {
    free(mBox);
    return OK;
  }
  else //the mailbox has messages, dont remove it.
  return NOT_EMPTY;
}

exception send_wait( mailbox* mBox, void* pData) //Sends message and puts itself in waitinglist if necessary
{
 isr_off();
 exception status;
 if(mBox->pHead->Status==RECEIVER && mBox->pHead->pBlock != NULL)//if the reciever is waiting and it is blocked
 {
  memcpy(mBox->pHead->pData, pData, mBox->nDataSize);
  msg* m = detachFirst(mBox);
  //---------
  free(m->pData);
  free(m);
  //--------
  //Fråga hazem, varför inte free här?
  //TÄNK, ta bort om det inte behövs mer!
  PreviousTask = NextTask;
  insert(ReadyList, extract(WaitingList,m->pBlock));
  NextTask = ReadyList->pHead->pTask;
 }
 else //if there is no waiting recieve message
 {
    msg* m;  
    m = (msg*)calloc(1, sizeof(msg));
    m->pData = (char*)calloc(1,mBox->nDataSize);
    if (m == NULL||m->pData==NULL) //if any of the callocs failed, return FAIL
    {
      status = FAIL;
      return status;
    }
    memcpy(m->pData, pData, mBox->nDataSize); //copy the value of pdata to msgdata in struct
    m->Status= SENDER; //set status to sender
    m->pBlock = ReadyList->pHead;
    addMessage(mBox, m);//inserts message last in the list.
    mBox->nMessages++;
    PreviousTask = NextTask;
    //nexttask skickas in i waitinglist och tas bort från readylist
    insert(WaitingList, extractFirst(ReadyList));
    NextTask=ReadyList->pHead->pTask;
 }
 SwitchContext();

 if (ReadyList->pHead->pTask->Deadline <= Ticks)
 {
   isr_off();
   //remove send message
   
   msg* temp = detachFirst(mBox);
   free(temp->pData);
   free(temp);
    //mBox->pHead->Status=NULL;
   //free(detachFirst(mBox));
   mBox->nMessages--;
   ReadyList->pHead->pMessage = NULL;
   isr_on();
   return DEADLINE_REACHED; 
 }
 else
   return OK;
}

exception receive_wait (mailbox* mBox, void* pData) //recieves a message and waits if necessary
{
  
  isr_off();
  exception status;
  if (mBox->pHead->Status==SENDER) //if there is a message from a sender
  {
    memcpy(pData, mBox->pHead->pData, mBox->nDataSize);
    //Fråga Hazem varför inte free?
    msg* m = detachFirst(mBox);
    mBox->nMessages--;
    if (m->pBlock != NULL) //if the sender is waiting
    {
      PreviousTask = NextTask;
      //insert(ReadyList, extractFirst(WaitingList));
      insert(ReadyList, extract(WaitingList,m->pBlock));
      NextTask = ReadyList->pHead->pTask;
      //-------
      free(m->pData);
      free(m);
      //-------
      status = OK;
    }
    else //if it wasnt waiting
    {
      
      free(m->pData);//?????
      free(m);
      
      //free(detachFirst(mBox));//??????????????????????????????
      status = OK;
    }
  }
  else //if there was no sender, create empty envelope
   {
    msg* m;
    m = (msg*)calloc(1, sizeof(msg));
    m->pData = (char*)calloc(1, sizeof(char));
    if(m == NULL || m->pData == NULL)
    {
      return FAIL;
    }
    m->Status = RECEIVER;
    addMessage(mBox, m);
    mBox->nMessages++;
    m->pBlock = ReadyList->pHead;
    PreviousTask = NextTask;
    insert(WaitingList, extractFirst(ReadyList));
    NextTask = ReadyList->pHead->pTask;
    status = OK;
   }
  
  SwitchContext();
  if(ReadyList->pHead->pTask->Deadline <= Ticks)
  {
    isr_off();
    //remove recieve message
    
    msg* temp = detachFirst(mBox);
    free(temp->pData);
    free(temp);
    
    //free(detachFirst(mBox));
    mBox->nMessages--;
    ReadyList->pHead->pMessage = NULL;
    //enable interrupt
    isr_on();
    return DEADLINE_REACHED;
  }
  return status;
}

exception send_no_wait( mailbox *mBox, void* pData) //sends message without waiting for reciever
{
  isr_off();
  exception status;
  if(mBox->pHead->Status==RECEIVER && mBox->pHead->pBlock != NULL) //if receiving task is waiting
  {
    memcpy(mBox->pHead->pData, pData, mBox->nDataSize);
    msg* m = detachFirst(mBox);
    //---
    free(m->pData);
    free(m);
    //----
    mBox->nMessages--;
    PreviousTask = NextTask;
    insert(ReadyList, extract(WaitingList,m->pBlock));
    NextTask = ReadyList->pHead->pTask;
    SwitchContext();
    status = OK;
  }
  else //if the reciever is not there, create the message and put it in the mailbox
  {
    msg* m;
    m = (msg*)calloc(1, sizeof(msg));
    m->pData = (char*)calloc(1, sizeof(char));
    if (m == NULL ||m->pData == NULL) //if any of the callocs failed, return FAIL
    {
      status = FAIL;
      return status;
    }
    memcpy(m->pData,pData,mBox->nDataSize);
    m->Status = SENDER; //set status to sender
    if (mBox->nMaxMessages == mBox->nMessages) //if mailbox is full
    {
      //Fråga Hazem, varför inte free?
      
      msg* temp = detachFirst(mBox);
      free(temp->pData);
      free(temp);
      
      //free(detachFirst(mBox)); //remove oldest message check with hazem if free!!
      mBox->nMessages--;
    }
    addMessage(mBox, m);//inserts message last in the list.
    mBox->nMessages++;
    status = OK;
  }
  return status;
}

exception receive_no_wait( mailbox* mBox, void* pData) //recieves message and but doesnt wait for sender, fails if the sender wasnt here earlier
{
  isr_off();
  exception status;
  if(mBox->nMessages==0) //if its empty we cant recieve anything
  {
    status = FAIL;
    return status;
  }
  
  if(mBox->pHead->Status==SENDER) //if there is a message from a sender 
  {
      //copy the first message in the mailbox to the currently running task  
     memcpy(pData, mBox->pHead->pData, mBox->nDataSize);
     msg* m = detachFirst(mBox);
     free(m->pData);
     free(m);
     mBox->nMessages--;
     if(m->pBlock != 0)//if something was waiting
     {
      PreviousTask = NextTask;
      insert(ReadyList, extract(WaitingList,m->pBlock));
      NextTask = ReadyList->pHead->pTask;
      SwitchContext();
      status = OK;
     }
     else //if it wasnt waiting
     {
       free(m->pData);
       free(m);
       status = OK;
     }
   }  
   else
   {
     return FAIL;
   }
   return status;
}

exception wait(uint nTicks) //sends the running task to the waitinglist for the specified duration
{
  
// disable interrupt
  isr_off();
  exception status;
  ReadyList->pHead->nTCnt = nTicks+Ticks;//sets nTCnt to when its ready again
  //update previous task
  PreviousTask = NextTask;
  //place running task in the timerlisdt
  insert(TimerList, extractFirst(ReadyList));
  //update next task
  NextTask = ReadyList->pHead->pTask;
  //switch context
  SwitchContext();
  
  if(ReadyList->pHead->pTask->Deadline <= Ticks)//if deadline is reached then status is deadline reached *!*!*!*!*!*!!*!*!!*!!*!*!
  {
    status = DEADLINE_REACHED;
  }
  else
  {
    status = OK;
  }
  return status;
}

void set_ticks(uint nTicks) //set global variable ticks to speciefied value
{
  Ticks = nTicks;
}

uint ticks(void) //returns the current value of Ticks
{
  return Ticks;
}

uint deadline(void) //returns the deadline of the currently running task
{
  return ReadyList->pHead->pTask->Deadline;
}

void set_deadline(uint deadline) //set the deadline for the currently running task and reschedule the readylist accordingly
{
//disable interrupt
  isr_off();
  //set the deadline field in the calling tcb
  ReadyList->pHead->pTask->Deadline = deadline+Ticks;
  //update prevt
  PreviousTask = NextTask;
  //reschedule readylist
  insert(ReadyList, extractFirst(ReadyList));
  //updarte nextt
  NextTask = ReadyList->pHead->pTask;
  //switch context
  SwitchContext();
}



void TimerInt(void) // Increments Ticks, checks the waitinglist and timerlist for tasks ready for excecution in the timerlist and passed deadlines in the waitinglist
{
  Ticks++;
  checkSleep();
}