/*
 * @Description: In User Settings Edit
 * @Author: your name
 * @Date: 2018-12-18 15:54:28
 * @LastEditTime: 2019-01-23 20:29:46
 * @LastEditors: OBKoro1
 */

#include "osapi.h"
#include "common_types.h"

#include "string.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"


#ifdef SIMPLE_LOG

#include "log.h"
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "OSAL"
#else
#define LOG_TAG "OSAL"
#endif

#define OSAL_LogI Logi
#define OSAL_LogD Logd
#define OSAL_LogW Logw
#define OSAL_LogE Loge

#endif

#define MAX_PRIORITY 255

/*tasks */
typedef struct {
  int free;
  TaskHandle_t id;
  char name[OS_MAX_API_NAME];
  int creator;
  uint32 stack_size;
  uint32 priority;
  osal_task_entry delete_hook_pointer;
  StaticTask_t xTaskBuffer;
} OS_task_internal_record_t;

/* queues */
typedef struct {
  int free;
  QueueHandle_t id;
  UBaseType_t queue_length;
  UBaseType_t ItemSize;
  char name[OS_MAX_API_NAME];
} OS_queue_internal_record_t;

OS_task_internal_record_t OS_task_table[OS_MAX_TASKS];

OS_queue_internal_record_t OS_queue_table[OS_MAX_QUEUES];
/*----------------------------------------------------------------------------
 * Name: OS_PriorityRemap
 *
 * Purpose: Remaps the OSAL priority into one that is viable for this OS
----------------------------------------------------------------------------*/

int32 OS_PriorityRemap(uint32 InputPri) {
  if (InputPri > configMAX_PRIORITIES) {
    return configMAX_PRIORITIES;
  }
  return InputPri;
}

int OS_InterruptSafeLock(void) {
  portDISABLE_INTERRUPTS();
  
  return 0;
}

int OS_InterruptSafeUnlock(void) {
  portENABLE_INTERRUPTS();
  return 0;
}

/* Task to be created. */
void vTaskCode(void *pvParameters){
    /* The parameter value is expected to be 1 as 1 is passed in the
    pvParameters value in the call to xTaskCreate() below.*/
    // configASSERT( ( ( uint32_t ) pvParameters ) == 1 );
    ((osal_task_entry)(pvParameters))();  // Call the registered callback function
}

/*---------------------------------------------------------------------------------------
   Name: OS_API_Init

   Purpose: Initialize the tables that the OS API uses to keep track of
information about objects

   returns: OS_SUCCESS or OS_ERROR
---------------------------------------------------------------------------------------*/
int32 OS_API_Init(void) { 
  
  memset(OS_task_table, 0, sizeof(OS_task_table));
  memset(OS_queue_table, 0, sizeof(OS_queue_table));
  OSAL_LogI("OSAL initialization succeeded ");
  return OS_SUCCESS;
}

/*---------------------------------------------------------------------------------------
    Name: OS_TaskCreate

    Purpose: Creates a task and starts running it.

    returns: OS_INVALID_POINTER if any of the necessary pointers are NULL
            OS_ERR_NAME_TOO_LONG if the name of the task is too long to be
copied OS_ERR_INVALID_PRIORITY if the priority is bad OS_ERR_NO_FREE_IDS if
there can be no more tasks created OS_ERR_NAME_TAKEN if the name specified
is already used by a task OS_ERROR if the operating system calls fail
OS_SUCCESS if success
            



NOTES: task_id is passed back to the user as the ID. stack_pointer is
usually null. the flags parameter is unused.

---------------------------------------------------------------------------------------*/
int32 OS_TaskCreate(uint32 *task_id, const char *task_name,
                    osal_task_entry function_pointer, uint32 *stack_pointer,
                    uint32 stack_size, uint32 priority, uint32 flags) {
  int os_priority;
  int possible_taskid;
  /* Check for NULL pointers */
  if ((task_name == NULL) || (function_pointer == NULL) || (task_id == NULL)) {
    OSAL_LogD("Invaild Parameter");
    return OS_INVALID_POINTER;
  }
  /* we don't want to allow names too long*/
  /* if truncated, two names might be the same */
  if (strlen(task_name) >= OS_MAX_API_NAME) {
    OSAL_LogD("Task name is too long");
    return OS_ERR_NAME_TOO_LONG;
  }

  /* Check for bad priority */
  if (priority > MAX_PRIORITY) {
    OSAL_LogD("Priority setting error");
    return OS_ERR_INVALID_PRIORITY;
  }

  /* Change OSAL priority into a priority that will work for this OS */
  os_priority = OS_PriorityRemap(priority);

  OS_InterruptSafeLock();

  for (possible_taskid = 0; possible_taskid < OS_MAX_TASKS; possible_taskid++) {
    if (OS_task_table[possible_taskid].free == FALSE) {
      OSAL_LogI("Found an idle task storage space");
      break;
    }
  }

  /* Check to see if the id is out of bounds */
  if (possible_taskid >= OS_MAX_TASKS ||
      OS_task_table[possible_taskid].free != FALSE) {
    OS_InterruptSafeUnlock();
    OSAL_LogI("No idle task storage space found");
    return OS_ERR_NO_FREE_IDS;
  }

  /* Check to see if the name is already taken */
  for (int i = 0; i < OS_MAX_TASKS; i++) {
    if ((OS_task_table[i].free == TRUE) &&
        (strcmp((char *)task_name, OS_task_table[i].name) == 0)) {
      OS_InterruptSafeUnlock();
      OSAL_LogD("Duplicate task name");
      return OS_ERR_NAME_TAKEN;
    }
  }

  /*
  ** Set the possible task Id to not free so that
  ** no other task can try to use it
  */
  OS_task_table[possible_taskid].free = FALSE;
  
  OS_InterruptSafeUnlock();

  if (stack_pointer == 0) {


    if (xTaskCreate(vTaskCode, task_name, stack_size, function_pointer,
                    os_priority,
                    &OS_task_table[possible_taskid].id) == pdPASS) {
      OS_InterruptSafeLock();
      OS_task_table[possible_taskid].free = TRUE;
      memcpy(OS_task_table[possible_taskid].name,task_name,strlen(task_name));
      OS_task_table[possible_taskid].stack_size = stack_size;
      OS_task_table[possible_taskid].priority = os_priority;
      OS_InterruptSafeUnlock();
      *task_id = possible_taskid;
      OSAL_LogD(
          "A task was successfully created. The task name is:<%s> The task ID "
          "is:%d "
          "The task stack size is:%d",
          OS_task_table[possible_taskid].name, possible_taskid,
          OS_task_table[possible_taskid].stack_size);
          return OS_SUCCESS;
      }else{
        OSAL_LogD("Create a task failed ");
        return OS_ERROR;
      }
  }
  #if (configSUPPORT_STATIC_ALLOCATION == 1)
    else {

      OS_task_table[possible_taskid].id = xTaskCreateStatic(
          vTaskCode, task_name, stack_size, function_pointer, os_priority,
          stack_pointer, &OS_task_table[possible_taskid].xTaskBuffer);
          
      if (OS_task_table[possible_taskid].id != 0) {
        OS_InterruptSafeLock();
        OS_task_table[possible_taskid].free = TRUE;
        OS_InterruptSafeUnlock();
        *task_id = possible_taskid;
        return OS_SUCCESS;
      }else {
        return OS_ERROR;
      }
    }
  #else
  return OS_ERROR;
  #endif
}

/*--------------------------------------------------------------------------------------
     Name: OS_TaskDelete

    Purpose: Deletes the specified Task and removes it from the OS_task_table.

    returns: OS_ERR_INVALID_ID if the ID given to it is invalid
             OS_ERROR if the OS delete call fails
             OS_SUCCESS if success
---------------------------------------------------------------------------------------*/
int32 OS_TaskDelete(uint32 task_id){
  int possible_taskid;
  OS_InterruptSafeLock();
  possible_taskid = task_id;
  OS_InterruptSafeUnlock();
  if (possible_taskid > OS_MAX_TASKS && OS_task_table[possible_taskid].free != TRUE){
    OSAL_LogD("No matching task ID found");
    return OS_ERROR;
  }
  vTaskDelete(OS_task_table[possible_taskid].id);
  OSAL_LogD("%s will be deleted,task id = %d",
            OS_task_table[possible_taskid].name,
            possible_taskid);
  OS_InterruptSafeLock();
  memset(&OS_task_table[possible_taskid], 0,
         sizeof(OS_task_table[possible_taskid]));
  OS_InterruptSafeUnlock();
  
  return OS_SUCCESS;
}

/*--------------------------------------------------------------------------------------
     Name:    OS_TaskExit

     Purpose: Exits the calling task and removes it from the OS_task_table.

     returns: Nothing
---------------------------------------------------------------------------------------*/
void OS_TaskExit(void) {
  int possible_taskid;
  TaskHandle_t CurrentTaskHandle =  xTaskGetCurrentTaskHandle();
  vTaskDelete(0);
  /*Delete this task in the cache list*/
  OS_InterruptSafeLock();
  for (possible_taskid = 0; possible_taskid < OS_MAX_TASKS; possible_taskid++) {
    if (OS_task_table[possible_taskid].id == CurrentTaskHandle) {
      OSAL_LogD("%s will be deleted,task id = %d",
                OS_task_table[possible_taskid].name, possible_taskid);
      memset(&OS_task_table[possible_taskid], 0,
             sizeof(OS_task_table[possible_taskid]));
    }
  }
  OS_InterruptSafeUnlock();
}
/*---------------------------------------------------------------------------------------
   Name: OS_TaskGetId

   Purpose: This function returns the #defined task id of the calling task

   Notes: The OS_task_key is initialized by the task switch if AND ONLY IF the
          OS_task_key has been registered via OS_TaskRegister(..).  If this is
not called prior to this call, the value will be old and wrong.
---------------------------------------------------------------------------------------*/
uint32 OS_TaskGetId(void){
  int possible_taskid;
  uint32 id = 0xffffffff;
  TaskHandle_t CurrentTaskHandle = xTaskGetCurrentTaskHandle();
  OS_InterruptSafeLock();
  for (possible_taskid = 0; possible_taskid < OS_MAX_TASKS; possible_taskid++) {
    if (OS_task_table[possible_taskid].id == CurrentTaskHandle) {
      id = possible_taskid;//
      break;
    }
  }
  OS_InterruptSafeUnlock();
  OSAL_LogD("Task id = %d", id);
  return id;
}

/*---------------------------------------------------------------------------------------
   Name: OS_TaskDelay

   Purpose: Delay a task for specified amount of milliseconds

   returns: OS_ERROR if sleep fails
            OS_SUCCESS if success
---------------------------------------------------------------------------------------*/
int32 OS_TaskDelay(uint32 millisecond) {
  vTaskDelay((millisecond*configTICK_RATE_HZ) / 1000);
  return OS_SUCCESS;
}

/*--------------------------------------------------------------------------------------
    Name: OS_TaskGetIdByName

    Purpose: This function tries to find a task Id given the name of a task

    Returns: OS_INVALID_POINTER if the pointers passed in are NULL
             OS_ERR_NAME_TOO_LONG if th ename to found is too long to begin with
             OS_ERR_NAME_NOT_FOUND if the name wasn't found in the table
             OS_SUCCESS if SUCCESS
---------------------------------------------------------------------------------------*/
int32 OS_TaskGetIdByName(uint32 *task_id, const char *task_name){
  int possible_taskid;
  int32 ret = OS_ERROR;
  #if (INCLUDE_xTaskGetHandle == 1)
  TaskHandle_t TaskHandle = xTaskGetHandle(task_name);
  if (TaskHandle != 0){
    OS_InterruptSafeLock();
    for (possible_taskid = 0; possible_taskid < OS_MAX_TASKS;
         possible_taskid++) {
      if (OS_task_table[possible_taskid].id == TaskHandle) {
        *task_id = possible_taskid;  //
        OSAL_LogD("%s Task id = %d", task_name, possible_taskid);
        ret = OS_SUCCESS;
        break;
      }
    }
    OS_InterruptSafeUnlock();
    return ret;
  }
  #endif
  return ret;
}

int32 OS_TaskSetPriority(uint32 task_id, uint32 new_priority){
  if (task_id > OS_MAX_TASKS || new_priority > configMAX_PRIORITIES) {
    return OS_ERROR;
  }
  vTaskPrioritySet(OS_task_table[task_id].id, new_priority);
  return OS_SUCCESS;
}

/*
** Message Queue API
*/

/*
** Queue Create now has the Queue ID returned to the caller.
*/
/*---------------------------------------------------------------------------------------
   Name: OS_QueueCreate

   Purpose: Create a message queue which can be refered to by name or ID

   Returns: OS_INVALID_POINTER if a pointer passed in is NULL
            OS_ERR_NAME_TOO_LONG if the name passed in is too long
            OS_ERR_NO_FREE_IDS if there are already the max queues created
            OS_ERR_NAME_TAKEN if the name is already being used on another queue
            OS_ERROR if the OS create call fails
            OS_SUCCESS if success

   Notes: the flags parameter is unused.
---------------------------------------------------------------------------------------*/
int32 OS_QueueCreate(uint32 *queue_id, const char *queue_name,
                     uint32 queue_depth, uint32 data_size, uint32 flags){
      uint32 possible_qid;
      if (queue_id == 0 || strlen(queue_name) == 0 || queue_depth == 0 ||
          data_size == 0) {
        OSAL_LogD("Input parameter error");
        return OS_INVALID_POINTER;
      }

      /* we don't want to allow names too long*/
      /* if truncated, two names might be the same */
      if (strlen(queue_name) >= OS_MAX_API_NAME) {
        OSAL_LogD("queue name is too long");
        return OS_ERR_NAME_TOO_LONG;
      }
      /* Check Parameters */

      OS_InterruptSafeLock();

      for (possible_qid = 0; possible_qid < OS_MAX_QUEUES; possible_qid++) {
        /*find an idle queue free*/
        if (OS_queue_table[possible_qid].free == FALSE) {
          OSAL_LogD("Find a free queue");
          break;
        }
      }

      if (possible_qid >= OS_MAX_QUEUES ||
          OS_queue_table[possible_qid].free != FALSE) {
        OSAL_LogD("Available queue not found");
        OS_InterruptSafeUnlock();
        return OS_ERR_NO_FREE_IDS;
      }

      /* Check to see if the name is already taken */

      for (int i = 0; i < OS_MAX_QUEUES; i++) {
        if ((OS_queue_table[i].free == TRUE) &&
            strcmp((char *)queue_name, OS_queue_table[i].name) == 0) {
          OS_InterruptSafeUnlock();
          return OS_ERR_NAME_TAKEN;
        }
      }

      /* Set the possible task Id to not free so that
       * no other task can try to use it */

      OS_queue_table[possible_qid].free = FALSE;

      OS_InterruptSafeUnlock();

      QueueHandle_t xQueue;
      OS_queue_table[possible_qid].id = xQueueCreate(queue_depth, data_size);
      if ( OS_queue_table[possible_qid].id != NULL ){
        OS_InterruptSafeLock();
        OS_queue_table[possible_qid].free = TRUE;
        memcpy(OS_queue_table[possible_qid].name, queue_name,
               strlen(queue_name));
        OS_queue_table[possible_qid].queue_length = queue_depth;
        OS_queue_table[possible_qid].ItemSize = data_size;
        OS_InterruptSafeUnlock();
        *queue_id = possible_qid;
        return OS_SUCCESS;
       }else{
         OSAL_LogD("Queue creation failed");
         return OS_ERROR;
      }
}

/*--------------------------------------------------------------------------------------
    Name: OS_QueueDelete

    Purpose: Deletes the specified message queue.

    Returns: OS_ERR_INVALID_ID if the id passed in does not exist
             OS_ERROR if the OS call to delete the queue fails
             OS_SUCCESS if success

    Notes: If There are messages on the queue, they will be lost and any
subsequent calls to QueueGet or QueuePut to this queue will result in errors
---------------------------------------------------------------------------------------*/
int32 OS_QueueDelete(uint32 queue_id){
  if(queue_id >= OS_MAX_QUEUES){
    OSAL_LogD("Find a free queue");
    return OS_QUEUE_ID_ERROR;
  }
  OS_InterruptSafeLock();
  if (OS_queue_table[queue_id].id != 0){
    vQueueDelete(OS_queue_table[queue_id].id);
    memset(&OS_queue_table[queue_id], 0, sizeof(OS_queue_internal_record_t));
  }
  OS_InterruptSafeUnlock();
  return OS_SUCCESS;
}
/*---------------------------------------------------------------------------------------
 Name: OS_QueueGet
 
 Purpose: Receive a message on a message queue.  Will pend or timeout on the
 receive. Returns: OS_ERR_INVALID_ID if the given ID does not exist
 OS_ERR_INVALID_POINTER if a pointer passed in is NULL
 OS_QUEUE_EMPTY if the Queue has no messages on it to be recieved
 OS_QUEUE_TIMEOUT if the timeout was OS_PEND and the time expired
 OS_QUEUE_INVALID_SIZE if the size of the buffer passed in is not big enough for
 the maximum size message OS_SUCCESS if success
 ---------------------------------------------------------------------------------------*/
int32 OS_QueueGet(uint32 queue_id, void *data, uint32 size, uint32 *size_copied,
                  int32 timeout){
  
  if(queue_id >= OS_MAX_QUEUES || data == 0 || size == 0 || size_copied == 0){
    OSAL_LogD("Queue read data parameter error");
    return OS_QUEUE_ID_ERROR;
  }
  OS_InterruptSafeLock();
  QueueHandle_t xQueue = OS_queue_table[queue_id].id;
  UBaseType_t ItemSize = OS_queue_table[queue_id].ItemSize;
  OS_InterruptSafeUnlock();
  if (size < ItemSize){
    OSAL_LogD("Queue read data parameter error");
    return OS_QUEUE_ID_ERROR;
  } 
  BaseType_t IsInterrupt = xPortIsInsideInterrupt();
  if (IsInterrupt){
    BaseType_t queue_valid = xQueueReceiveFromISR(xQueue, data, 0);
    if (queue_valid){
      *size_copied = ItemSize;
      OSAL_LogI("Successfully read data from the queue");
      return OS_SUCCESS;
    }else{
      OSAL_LogI("Queue data is empty");
      return OS_QUEUE_EMPTY;
    }
  }else{
    BaseType_t queue_valid = xQueueReceive(xQueue, data, timeout);
    if (queue_valid){
      *size_copied = ItemSize;
      OSAL_LogI("Successfully read data from the queue");
      return OS_SUCCESS;
    }else{
      OSAL_LogI("Queue data is empty");
      return OS_QUEUE_EMPTY;
    }
  } 
  
}
/*---------------------------------------------------------------------------------------
 Name: OS_QueuePut
 
 Purpose: Put a message on a message queue.
 

 Returns: OS_ERR_INVALID_ID if the queue id passed in is not a valid queue
 OS_INVALID_POINTER if the data pointer is NULL
 OS_QUEUE_FULL if the queue cannot accept another message
 OS_ERROR if the OS call returns an error
 OS_SUCCESS if SUCCESS
 

 Notes: The flags parameter is not used.  The message put is always configured
 to immediately return an error if the receiving message queue is full.
 ---------------------------------------------------------------------------------------*/
int32 OS_QueuePut(uint32 queue_id, const void *data, uint32 size, uint32 flags){
  if (queue_id >= OS_MAX_QUEUES || data == 0 ) {
    OSAL_LogD("Queue read data parameter error");
    return OS_QUEUE_ID_ERROR;
  }
  OS_InterruptSafeLock();
  QueueHandle_t xQueue = OS_queue_table[queue_id].id;
  UBaseType_t ItemSize = OS_queue_table[queue_id].ItemSize;
  OS_InterruptSafeUnlock();

  BaseType_t IsInterrupt = xPortIsInsideInterrupt();
  if (IsInterrupt) {
    BaseType_t queue_valid = xQueueSendFromISR(xQueue, data, 0);
    if (queue_valid) {
      OSAL_LogI("Successfully save data to the queue");
      return OS_SUCCESS;
    } else {
      OSAL_LogI("Queue data is full");
      return OS_QUEUE_FULL;
    }
  } else {
    BaseType_t queue_valid = xQueueSend(xQueue, data, 0);
    if (queue_valid) {
      OSAL_LogI("Successfully save data to the queue");
      return OS_SUCCESS;
    } else {
      OSAL_LogI("Queue data is full");
      return OS_QUEUE_FULL;
    }
  }
}

int32 OS_QueueGetIdByName(uint32 *queue_id, const char *queue_name){
  if (queue_name == 0 || queue_id == 0){
    OSAL_LogD("Queue parameter error");
    return OS_QUEUE_ID_ERROR;
  }
  OS_InterruptSafeLock();
  for (int i = 0; i < OS_MAX_QUEUES; i++) {
    if ((OS_queue_table[i].free == TRUE) &&
        strcmp((char *)queue_name, OS_queue_table[i].name) == 0) {
      *queue_id = i;
      OS_InterruptSafeUnlock();
      return OS_SUCCESS;
    }
  }
  OS_InterruptSafeUnlock();
  return OS_QUEUE_ID_ERROR;
}

int32 OS_QueueGetInfo(uint32 queue_id, OS_queue_prop_t *queue_prop){
  return OS_ERROR;
}

#if 0


int32 OS_TaskInstallDeleteHandler(osal_task_entry function_pointer);


int32 OS_TaskRegister(void);

int32 OS_TaskGetInfo(uint32 task_id, OS_task_prop_t *task_prop);








/*
** Semaphore API
*/

int32 OS_BinSemCreate(uint32 *sem_id, const char *sem_name,
                      uint32 sem_initial_value, uint32 options);
int32 OS_BinSemFlush(uint32 sem_id);
int32 OS_BinSemGive(uint32 sem_id);
int32 OS_BinSemTake(uint32 sem_id);
int32 OS_BinSemTimedWait(uint32 sem_id, uint32 msecs);
int32 OS_BinSemDelete(uint32 sem_id);
int32 OS_BinSemGetIdByName(uint32 *sem_id, const char *sem_name);
int32 OS_BinSemGetInfo(uint32 sem_id, OS_bin_sem_prop_t *bin_prop);

int32 OS_CountSemCreate(uint32 *sem_id, const char *sem_name,
                        uint32 sem_initial_value, uint32 options);
int32 OS_CountSemGive(uint32 sem_id);
int32 OS_CountSemTake(uint32 sem_id);
int32 OS_CountSemTimedWait(uint32 sem_id, uint32 msecs);
int32 OS_CountSemDelete(uint32 sem_id);
int32 OS_CountSemGetIdByName(uint32 *sem_id, const char *sem_name);
int32 OS_CountSemGetInfo(uint32 sem_id, OS_count_sem_prop_t *count_prop);

/*
** Mutex API
*/

int32 OS_MutSemCreate(uint32 *sem_id, const char *sem_name, uint32 options);
int32 OS_MutSemGive(uint32 sem_id);
int32 OS_MutSemTake(uint32 sem_id);
int32 OS_MutSemDelete(uint32 sem_id);
int32 OS_MutSemGetIdByName(uint32 *sem_id, const char *sem_name);
int32 OS_MutSemGetInfo(uint32 sem_id, OS_mut_sem_prop_t *mut_prop);
#endif
