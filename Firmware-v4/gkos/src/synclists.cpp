#include "threadproclist.h"
#include "completion_list.h"

ThreadList_t ThreadList;
ProcessList_t ProcessList;
MutexList_t MutexList;
CondList_t ConditionList;
RwLockList_t RwLockList;
UserspaceSemaphoreList_t UserspaceSemaphoreList;

CompletionList<id_t> ProcessExitCodes;
CompletionList<pidtid, pidtid_hash> ThreadExitCodes;
