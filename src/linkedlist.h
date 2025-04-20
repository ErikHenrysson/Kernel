#ifndef _linkedlist_
#define _linkedlist_
#include "kernel_functions.h"

listobj* extractFirst(list* list);
void insert(list* list, listobj* task);
void printList(list* list);
msg* detachFirst(mailbox* mBox);
exception addMessage(mailbox* mBox, msg* message);
void checkSleep(void);
listobj* extract(list* list, listobj* obj);
#endif