#include "stdio.h"
#include "stdlib.h"
#include "kernel_functions.h"

//functions
listobj* extractFirst(list* list);
void insert(list* list, listobj* task);
void printList(list* list);
msg* detachFirst(mailbox* mBox);
exception addMessage(mailbox* mBox, msg* message);
listobj* extract(list* list, listobj* obj);
void checkSleep(void);


listobj* extractFirst(list* list) //extracts the first element in a given list, often used to get the currently running task from ReadyList
{
  if(list->pHead == NULL) //if its empty, dont do anything
    return NULL;
  if(list->pHead == list->pTail)//if its the last element
  {
    listobj* temp = list->pHead;
    list->pHead = NULL;
    list->pTail = NULL;
    return temp;
  }
  //else if there is more elements in the list
  listobj* temp = list->pHead;
  list->pHead = list->pHead->pNext;
  list->pHead->pPrevious= NULL;
 
  return temp;
}

msg* detachFirst(mailbox* mBox) //only need detachFirst because of FIFO
{
  if(mBox->pHead == 0 && mBox->pTail ==0)//if the mailbox is empty
    return FAIL;
  if(mBox->pHead==mBox->pTail) //if there is only one element in the mailbox
  {
    msg* temp = mBox->pHead;
    mBox->pHead = 0;
    mBox->pTail = 0;
    return temp;
  }
  else //if there is more than one element in the mailbox
  {
    msg* temp = mBox->pHead;
    mBox->pHead->pNext->pPrevious = 0;
    mBox->pHead = mBox->pHead->pNext;
    return temp;
  }
}

exception addMessage(mailbox* mBox, msg* message)//adds a message at the tail of the mailbox (FIFO)
{
  if(mBox->pHead == NULL) //the mailbox is empty
  {
    mBox->pHead = message;
    mBox->pTail = message;
    message->pNext = NULL;
    message->pPrevious = NULL;   
    return OK;
  }
  else if (mBox->pHead == mBox->pTail)//if there is only one element in the mailbox
  {
    mBox->pHead->pNext = message;
    mBox->pTail = message;
    message->pNext= NULL;
    message->pPrevious = mBox->pHead;
    return OK;
  }
  else //more than one elemnt in the list, add it last (FIFO)
  {
    mBox->pTail->pNext = message;
    message->pPrevious = mBox->pTail;
    message->pNext = NULL;
    mBox->pTail = message;
    return OK;
  }
    
}


void insert(list* list, listobj* obj)//inserts a task at the right place in a list, sort by deadline
{  
  obj->pNext = NULL;
  obj->pPrevious = NULL;
  if (list->pHead == NULL) //if list is empty
  {
    list->pHead = obj;
    list->pTail = obj;
    return;
  }
  else //if there is 2 or more elements, find the right place for the element
  {
    if(obj->pTask->Deadline < list->pHead->pTask->Deadline)//if it should be added first
    {
      list->pHead->pPrevious = obj;
      obj->pNext = list->pHead;
      list->pHead = obj;
      return;
    }  
    else
    {
    listobj* temp = list->pHead;//temporary node for itteration
    while(temp != NULL)
    {
      if (obj->pTask->Deadline < temp->pTask->Deadline) //if new task deadline is lower than temp element, insert
      {
        //if it finds an element with higher deadline, insert it
        temp->pPrevious->pNext = obj;
        obj->pPrevious = temp->pPrevious;
        temp->pPrevious = obj;
        obj->pNext = temp;
        return;
      }
      temp = temp->pNext;  
    }
    }
    //if no element has higher deadline, insert it last
    list->pTail->pNext = obj;
    obj->pPrevious = list->pTail;
    list->pTail = obj;
    obj->pNext = NULL;
  }
}

void printList(list* list) //prints the specified list, used for debugging
{
  listobj* temp = list->pHead;
  printf("---------------\n");
  while(temp!=NULL)
  {
    printf( "%u \n", temp->pTask->Deadline);
  temp = temp->pNext;
  }
  printf("---------------\n");
}


listobj* extract(list* list, listobj* obj) // extracts a given node in a given list
{
  if(list->pHead == NULL)// if the list is empty, do nothing
  {
    return FAIL;
  }
  else if(list->pHead == list->pTail && list->pHead == obj)//if there is only one element and it is the one to be extracted
  {
    list->pHead = NULL;
    list->pTail = NULL;
    return obj;
  }
  else //else if there is more than one element in the list, check all until you find it or NULL
  {
    if(list->pHead == obj && list->pHead->pPrevious == NULL)//if its the first element in the list
    {
      list->pHead->pNext->pPrevious = NULL;
      list->pHead = list->pHead->pNext;
      return obj;
    }
    else if (list->pTail == obj && list->pTail->pNext == NULL)//if its the last element
    {
      list->pTail->pPrevious->pNext = NULL;
      list->pTail = list->pTail->pPrevious;
      return obj;
    }
    else //if its not the first or the last its somewhere inbetween
    {
      listobj* temp = list->pHead;
      while(temp != NULL) //go until youve gone through the whole list
      {
         if(temp == obj)
         {
            temp->pPrevious->pNext = temp->pNext;
            temp->pNext->pPrevious = temp->pPrevious;
            return obj;
         }
         temp = temp->pNext;
      }
    }
  }
  return FAIL;
}

void checkSleep(void)//goes through the timerlist and waitinglist to see if any task is ready for excectuion or any deadline is passed
{
  if(TimerList->pHead == NULL && WaitingList->pHead == NULL)// if both the lists are empty, nothing will happen
    return;
  else //if there are more elements
  {
    listobj* temp = TimerList->pHead;
    while(temp != NULL) //if the list isnt empty, go through every node and check if they are ready for excecution
    {    
       if(temp->nTCnt <= ticks() || temp->pTask->Deadline <= ticks())//if you find a node with an expired sleep duration
       {
          listobj *obj= temp;
          temp = temp->pNext; 
          PreviousTask = NextTask;
          insert(ReadyList, extract(TimerList, obj));
          NextTask = ReadyList->pHead->pTask;
       }else
          temp = temp->pNext; 
    }
    listobj* temp2 = WaitingList->pHead;
    while(temp2 != NULL) //chheck waitinglist
    {
    if(temp2->pTask->Deadline <= ticks())//if you find a task with expired deadline
       {
         //temp2->pTask->Deadline = 0; //get it to excecute quickly
         listobj* obj2 = temp2;
         temp2 = temp2->pNext;
         PreviousTask = NextTask;
         insert(ReadyList, extract(WaitingList, obj2));
         NextTask = ReadyList->pHead->pTask;
           /*
      --------------------------------------------------
          "Clean up their mailbox entry"
      -------------------------------------------------
          */
       }else
          temp2 = temp2->pNext;
    }
  }
}
