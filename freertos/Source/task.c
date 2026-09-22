#include "task.h"

/*任务就绪列表*/
List_t pxReadyTasksLists[ configMAX_PRIORITIES ];
TCB_t * pxCurrentTCB;
static TaskHandle_t xIdleTaskHandle					= NULL;
static volatile TickType_t xTickCount 				= ( TickType_t ) 0U;
static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;

/*两条任务延时列表，当xTickCount没有溢出时使用1链表，溢出后用2链表*/
static List_t xDelayedTaskList1;
static List_t xDelayedTaskList2;
/*任务延时列表指针，指向xTickCount没有溢出时使用的列表*/
static List_t * volatile pxDelayedTaskList;
/*任务延时列表指针，指向xTickCount溢出时使用的列表*/
static List_t * pxOverflowDelayedTaskList;

/*延时下个解锁的任务的解锁时刻，等于延时时的xTickCount+延时时间*/
static volatile TickType_t xNextTaskUnblockTime		= ( TickType_t ) 0U;

static volatile BaseType_t xNumOfOverflows 			= ( BaseType_t ) 0;
/*
*************************************************************************
*                               函数声明
*************************************************************************
*/		
/*Task初始化函数，创建任务使用的TCB，然后用句柄指向该TCB*/
static void prvInitialiseNewTask(TaskFunction_t pxTaskCode,		 /*任务函数*/
																const char * const pcName,		 /*任务名称*/
																const uint32_t ulStackDepth,	 /*任务栈大小*/	
																void * const pvParameters,		 /*任务形参*/
																UBaseType_t uxPriority,
																TaskHandle_t * const pxCreatedTask, /*任务句柄*/
																TCB_t *pxNewTCB);
														
static portTASK_FUNCTION( prvIdleTask, pvParameters );

static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait );
																
static void prvAddNewTaskToReadyList( TCB_t * pxNewTCB );
																
static void prvResetNextTaskUnblockTime(void);
/*
*************************************************************************
****************************    宏定义    ***********************************
*************************************************************************
*/	
																
/* 将任务添加到就绪列表 */                                    
#define prvAddTaskToReadyList( pxTCB )																   \
	taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );												   \
	vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \

/********************************************************
          查找最高优先级的就绪任务：通用方法   
********************************************************/															
/*
	函数名称;taskRECORD_READY_PRIORITY
	作用：每次创建新任务或阻塞结束时调用，更新uxTopReadyPriority												
*/
#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )

	/* uxTopReadyPriority 存的是就绪任务的最高优先级 */
	#define taskRECORD_READY_PRIORITY( uxPriority )														\
	{																									\
		if( ( uxPriority ) > uxTopReadyPriority )														\
		{																								\
			uxTopReadyPriority = ( uxPriority );														\
		}																								\
	} /* taskRECORD_READY_PRIORITY */


	
/*
	函数名称：taskSELECT_HIGHEST_PRIORITY_TASK
	作用：寻找最高优先级任务
*/
	#define taskSELECT_HIGHEST_PRIORITY_TASK()															\
	{																									\
		UBaseType_t uxTopPriority = uxTopReadyPriority;														\
																										\
		/* 寻找包含就绪任务的最高优先级的队列 */                                                          \
		while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) )							\
		{																								\
			--uxTopPriority;																			\
		}																								\
																										\
		/* 获取优先级最高的就绪任务的TCB，然后更新到pxCurrentTCB */							            \
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );			\
		/* 更新uxTopReadyPriority */                                                                    \
		uxTopReadyPriority = uxTopPriority;																\
	} /* taskSELECT_HIGHEST_PRIORITY_TASK */

	

	/**************************************************************
								查找最高优先级的就绪任务：优化方法 
	**************************************************************/
	
	/*原理：此时uxTopReadyPriority不再是十进制数，而是32位二进制数，每一位对应优先级位数
				在任务创建时portRESET_READY_PRIORITY按照任务优先级给对应的位数置1，表示该优先级有任务
				然后通过CLZ指令简化计算*/
	
	/*
	宏函数名称：taskRESET_READY_PRIORITY
	作用：当前优先级就绪链表没有任务时，将图表对应位置清零
	*/
	#define taskRESET_READY_PRIORITY( uxPriority )\
	{\
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) \
																  == (UBaseType_t) 0 )\
		{\
			portRESET_READY_PRIORITY( (uxPriority),\
																(uxTopReadyPriority) );\
		}\
		
	}
	
	//port.c中已有定义
	//#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )
    
/* 查找最高优先级的就绪任务：根据处理器架构优化后的方法 */
#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

	#define taskRECORD_READY_PRIORITY( uxPriority )	portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

	/*-----------------------------------------------------------*/

	#define taskSELECT_HIGHEST_PRIORITY_TASK()														    \
	{																								    \
		UBaseType_t uxTopPriority;																		    \
																									    \
		/* 寻找最高优先级 */								                            \
		portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority );								    \
		/* 获取优先级最高的就绪任务的TCB，然后更新到pxCurrentTCB */                                       \
		listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );		    \
	} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

	/*-----------------------------------------------------------*/
#if 0
	#define taskRESET_READY_PRIORITY( uxPriority )														\
	{																									\
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 )	\
		{																								\
			portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) );							\
		}																								\
	}
#else
    #define taskRESET_READY_PRIORITY( uxPriority )											            \
    {																							        \
            portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) );					        \
    }
#endif
    
#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */													

															
/*
*************************************************************************
函数名称：taskSWITCH_DELAYED_LISTS
函数作用：xTickCount溢出时使用该函数切换延时列表
重点：需要调用prvInitialiseNewTask创建任务
*************************************************************************
*/
#define taskSWITCH_DELAYED_LISTS()\
{\
	List_t * pxTemp;\
	pxTemp = pxDelayedTaskList;\
	pxDelayedTaskList = pxOverflowDelayedTaskList;\
	pxOverflowDelayedTaskList = pxTemp;\
	xNumOfOverflows++;\
	prvResetNextTaskUnblockTime();\
}
		
		

/*
*************************************************************************
函数名称：xTaskCreateStatic
函数作用：静态创建任务
重点：需要调用prvInitialiseNewTask创建任务
*************************************************************************
*/	
/*静态创建任务，返回任务句柄*/
#if( configSUPPORT_STATIC_ALLOCATION == 1 )  /* 是否支持静态分配 */

TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,           /* 任务函数 */
                                const char * const pcName,           /* 任务名称 */
                                const uint32_t ulStackDepth,         /* 任务栈大小 */
                                void * const pvParameters,           /* 任务形参 */
                                UBaseType_t uxPriority,							 /* 任务优先级 */
																StackType_t * const puxStackBuffer,  /* 任务栈起始地址 */
                                TCB_t * const pxTaskBuffer )         /*任务控制块指针*/
{
    TCB_t *pxNewTCB;
    TaskHandle_t xReturn;  /* 句柄 */

    if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
    {
        pxNewTCB = ( TCB_t * ) pxTaskBuffer;									/*TCB指针赋给pxNewTCB*/
        pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

        /* 创建新的任务 */  /* (9) */
        prvInitialiseNewTask( pxTaskCode,        /* 任务函数 */
                              pcName,            /* 任务名称，字符串形式 */
                              ulStackDepth,      /* 任务栈大小，单位为字 */
                              pvParameters,      /* 任务形参 */
                              uxPriority,			   /*任务优先级*/
															&xReturn,          /* 任务句柄 */
                              pxNewTCB );        /* 任务TCB，已经拥有栈起始地址和栈大小 */
			
			/*将任务添加到就绪列表*/
			prvAddNewTaskToReadyList( pxNewTCB );
		}
    else
    {
        xReturn = NULL;
    }

    /* 如果创建任务成功，此时 xReturn 应该指向任务控制块 */  /* (10) */
    return xReturn;
}

#endif /* configSUPPORT_STATIC_ALLOCATION == 1 */


/*
*************************************************************************
函数名称：prvInitialiseNewTask 
作用：任务初始化函数，创建任务使用的TCB，然后用句柄指向该TCB    
重点：pxPortInitialiseStack伪造栈帧
*************************************************************************
*/
static void prvInitialiseNewTask(TaskFunction_t pxTaskCode,		 /*任务函数*/
																const char * const pcName,		 /*任务名称*/
																const uint32_t ulStackDepth,	 /*任务栈大小*/	
																void * const pvParameters,		 /*任务形参*/
																UBaseType_t uxPriority,
																TaskHandle_t * const pxCreatedTask, /*任务句柄*/
																TCB_t *pxNewTCB)			
{
	StackType_t *pxTopOfStack;
	UBaseType_t x;
	
	/*获取栈顶地址*/
	pxTopOfStack = pxNewTCB->pxStack + (ulStackDepth - (uint32_t)1);
	
	/*按8字节对齐*/
	/*为什么要重新对齐？ ARM芯片规定栈指针必须是8的倍数*/
	/*栈指针不可以越界，这里向下取整&~0x0007，是把地址低3位清0，这样栈顶指针一定是8的倍数*/
	pxTopOfStack = (StackType_t * )\
								 ( ( (uint32_t) pxTopOfStack) & (~( (uint32_t)0x0007) ) );
	
	/*复制任务名称到TCB中*/
	for ( x= (UBaseType_t) 0; x< (UBaseType_t) configMAX_TASK_NAME_LEN; x++ )
	{
		pxNewTCB->pcTaskName [x] = pcName [x];
		
		if (pcName[x] == 0x00)
		{
			break;
		}
	}
	
	/*任务名称长度不能超过configMAX_TASK_NAME_LEN*/
	pxNewTCB->pcTaskName[configMAX_TASK_NAME_LEN-1] = '\0';
	
	/*初始化TCB中的xStateListItem节点*/
	vListInitialiseItem( & ( pxNewTCB->xStateListItem ) );
	
	/*设置xStateListItem节点的拥有者*/
	listSET_LIST_ITEM_OWNER( &(pxNewTCB->xStateListItem),pxNewTCB );
	
	/*初始化优先级*/
	/*防止优先级溢出，过大自动设置为最高优先级*/
	if ( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = (UBaseType_t) configMAX_PRIORITIES - (UBaseType_t) 1U;
	}
	pxNewTCB->uxPriority = uxPriority;
	
	/*初始化任务栈*/
	//此时pxTopOfStack指向栈顶
	pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack,
																									pxTaskCode,
																									pvParameters );
	
	/*任务句柄指向任务控制块*/
	if  ( (void*) pxCreatedTask != NULL )
	{
		*pxCreatedTask = (TaskHandle_t) pxNewTCB;
	}
	
}



extern TCB_t Task1TCB;
extern TCB_t Task2TCB;
/*
*************************************************************************
函数名称：prvInitialiseTaskLists 
作用：将任务就绪列表(pxReadyTasksLists)，任务延时列表(xDelayedTaskList1&2)初始化
*************************************************************************
*/
void prvInitialiseTaskLists(void)
{
	UBaseType_t uxPriority;
	
	for (uxPriority = (UBaseType_t) 0U;
			uxPriority < (UBaseType_t) configMAX_PRIORITIES;
			uxPriority++ )
	{
		vListInitialise ( &( pxReadyTasksLists[ uxPriority ] ) );
	}
	vListInitialise( &xDelayedTaskList1 );
	vListInitialise( &xDelayedTaskList2 );
	
	pxDelayedTaskList = &xDelayedTaskList1;
	pxOverflowDelayedTaskList = &xDelayedTaskList2;
}


/**********************************************************************
函数名称：vTaskStartScheduler
作用：启动调度器，从就绪列表中找到优先级最高的任务然后去执行该任务
重点：
主要功能：1.pxCurrentTCB 指向当前即即将运行的任务TCB
***********************************************************************/
extern TCB_t IdleTaskTCB;

void vApplicationGetIdleTaskMemory( TCB_t **ppxIdleTaskTCBBuffer,
																		StackType_t **ppxIdleTaskStackBuffer,
																		uint32_t *pulIdleTaskStackSize);

void vTaskStartScheduler(void)
{
	/*======================================创建空闲任务 start===============================*/
	TCB_t *pxIdleTaskTCBBuffer = NULL;				 //指向空闲任务控制块
	StackType_t *pxIdleTaskStackBuffer = NULL; //指向空闲任务栈起始地址
	uint32_t ulIdleTaskStackSize;
	
	/*获取空闲任务的内存：任务栈和任务TCB*/
	/*设置pxIdleTaskTCBBuffer、pxIdleTaskStackBuffer、ulIdleTaskStackSize的值 */
	vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer,
																 &pxIdleTaskStackBuffer,
																 &ulIdleTaskStackSize);
	
	xIdleTaskHandle = xTaskCreateStatic ((TaskFunction_t)prvIdleTask,		/*任务函数*/
																			 (char *)"IDLE	",							/*任务名称，字符串形式*/
																			 (uint32_t)ulIdleTaskStackSize,	/*任务栈大小，单位为字*/
																			 (void *) NULL,									/*任务形参*/
																			 (UBaseType_t) tskIDLE_PRIORITY,
																			 (StackType_t*)pxIdleTaskStackBuffer,/*任务栈起始地址*/
																			 (TCB_t *)pxIdleTaskTCBBuffer);	/*任务控制块*/
	

	/*======================================创建空闲任务end===============================*/
	
																			 
	xNextTaskUnblockTime = portMAX_DELAY;
																			 
	//启动调度器
	if (xPortStartScheduler() != pdFALSE)
	{
		/*调度器启动成功，则不会返回，即不会来到这里*/
	}
}

static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
	/* 防止编译器的警告 */
	( void ) pvParameters;
    
    for(;;)
    {
        /* 空闲任务暂时什么也不做 */
    }
}

/**********************************************************************
函数名称：vTaskSwitchContext
作用：任务切换函数，寻找优先级最高的就绪任务。被PendSV中断调用
注意：调用taskSELECT_HIGHEST_PRIORITY_TASK()
			寻找到优先级最高的就绪任务的 TCB，然后更新到 pxCurrentTCB
***********************************************************************/
void vTaskSwitchContext(void)
{
	/* 获取优先级最高的就绪任务的 TCB，然后更新到 pxCurrentTCB */
	taskSELECT_HIGHEST_PRIORITY_TASK();
}

/**********************************************************************
函数名称：vTaskDelay
作用：任务延时，任务进入阻塞状态
形参：xTicksToDelay 延时时间,单位为SysTick的中断次数
***********************************************************************/
void vTaskDelay( const TickType_t xTicksToDelay )
{
	TCB_t *pxTCB = NULL;
	
	/*获取当前任务的TCB*/
	pxTCB = pxCurrentTCB;
	
	/*将任务插入到延时列表,也做了就序列表移除，图表清零操作*/
	prvAddCurrentTaskToDelayedList(xTicksToDelay);
	
	/*任务切换*/
	taskYIELD();
}


/**********************************************************************
函数名称：xTaskIncrementTick
作用：1.时基更新
			2.当前时基与xNextTaskUnblockTime对比，将对应的延时任务解锁
***********************************************************************/
BaseType_t xTaskIncrementTick(void)
{
	TCB_t *pxTCB =NULL;
	TickType_t xItemValue;
	
	/*是否需要切换任务*/
	BaseType_t xSwitchRequired = pdFALSE;
	
	//更新系统时基计数器xTickCount, xTickCount是port.c中定义的全局变量
	const 	TickType_t xConstTickCount = xTickCount + 1;
	xTickCount = xConstTickCount;
	
	/*如果xConstTickCount溢出，则切换延时列表*/
	if (xConstTickCount == (TickType_t) 0U )
	{
		taskSWITCH_DELAYED_LISTS();
	}
	
	/*最近的延时任务到期*/
	if ( xConstTickCount>= xNextTaskUnblockTime )
	{
		for(;;)
		{
			/*延时列表为空，没有等待任务*/
			if ( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
			{
				/*延时列表为空，设置xNextTaskUnblockTime为最大值*/
				xNextTaskUnblockTime = portMAX_DELAY;
				break;
			}
			else /*延时列表不为空，有需要解锁的任务*/
			{
				/*提取延时列表辅助值最低的任务*/
				pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
				xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );
				
				/*当前时刻小于辅助值，任务现在还不需要解锁，跳出循环，
				即已经将所以即将解锁的任务添加到就绪列表*/
				if( xConstTickCount < xItemValue )
				{
					xNextTaskUnblockTime = xItemValue;
					break;
				}
				/*将任务从延时列表删除，消除等待状态*/
				(void) uxListRemove( &( pxTCB->xStateListItem ) );
				
				/*将解除等待的任务添加到就绪列表*/
				prvAddTaskToReadyList( pxTCB );
				
				#if (configUSE_PREEMPTION == 1)
				{
					/*判断刚解锁的任务优先级是否大于当前任务优先级*/
					if (pxTCB->uxPriority >= pxCurrentTCB->uxPriority)
					{
						xSwitchRequired = pdTRUE;
					}
				}
				#endif
				
			}
		}
	}/* xConstTickCount >= xNextTaskUnblockTime */
	
	#if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
	{
		if ( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) 
																	> (UBaseType_t) 1 )
		{
			xSwitchRequired = pdTRUE;
		}
	}
	#endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */

	return xSwitchRequired;
}


/**********************************************************************
函数名称：prvAddNewTaskToReadyList
作用：将新任务添加到就绪列表，同时让pxCurrentTCB指向最高优先级任务
***********************************************************************/
UBaseType_t uxCurrentNumberOfTasks;

static void prvAddNewTaskToReadyList( TCB_t * pxNewTCB )
{
	/*进入临界段*/
	taskENTER_CRITICAL();
	{
		/*全局任务计时器+1*/
		uxCurrentNumberOfTasks++;
		
		/*如果pxCurrent为空，则将pxCurrentTCB指向创建的新任务*/
		if (pxCurrentTCB == NULL)
		{
			pxCurrentTCB = pxNewTCB;
			
			if (uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
			{
				/*初始化相关列表*/
				prvInitialiseTaskLists();
			}
		}
		else /*如果pxCurrentTCB不为空，则将pxCurrentTCB指向最高优先级TCB*/
		{
			if (pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
			{
				pxCurrentTCB = pxNewTCB;
			}
		}
		/*将任务添加到就绪列表*/
		prvAddTaskToReadyList ( pxNewTCB );
	}
	taskEXIT_CRITICAL();
}


/**********************************************************************
函数名称：prvAddCurrentTaskToDelayedList
作用：1.将当前任务（即pxCurrentTCB指向的任务）添加到延时列表
			2.并将其从就序列表删除，清零对应的位图
			3.将延时时间设置为节点的排序值（即pxCurrentTCB->xStateListItem）
			4.更新xNextTaskUnblockTime
			
***********************************************************************/
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait )
{
	TickType_t xTimeToWake;
	
	/*记录当前时刻*/
	const TickType_t xConstTickCount = xTickCount;
	
	/*将任务从就绪列表移除*/
	if (uxListRemove ( &( pxCurrentTCB->xStateListItem ) )== ( UBaseType_t ) 0 )
	{
		/*将任务在优先级位图中的对应位置清零*/
		portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, 
		uxTopReadyPriority );
	}
	
	/*计算任务延时到期时，系统时基计数器xTickCount的值是多少*/
	xTimeToWake = xConstTickCount + xTicksToWait;
	
	/*将延时到期的值设置为节点的排序值*/
	listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ),
													 xTimeToWake);
	
	/*溢出*/
	if ( xTimeToWake < xConstTickCount )
	{
		vListInsert( pxOverflowDelayedTaskList,
								 &( pxCurrentTCB->xStateListItem ) );
	}
	else /*没有溢出*/
	{
		vListInsert ( pxDelayedTaskList,
									&( pxCurrentTCB->xStateListItem ));
		
		/*更新最近的任务wake时刻给xNextTaskUnblockTime*/
		if ( xTimeToWake < xNextTaskUnblockTime )
		{
			xNextTaskUnblockTime = xTimeToWake;
		}
	}
	
}


/**********************************************************************
函数名称：prvResetNextTaskUnblockTime
作用：复位 xNextTaskUnblockTime 的值，在taskSWITCH_DELAYED_LISTS()中被调用
***********************************************************************/
static void prvResetNextTaskUnblockTime(void)
{
	TCB_t *pxTCB;
	
	if ( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
	{
		/*当前延时列表为空，则设置xNextTaskUnblockTime等于最大值*/
		xNextTaskUnblockTime = portMAX_DELAY;
	}
	else
	{
		/* 当前列表不为空，则有任务在延时，则获取当前列表下第一个节点的排序值
		然后将该节点的排序值更新到 xNextTaskUnblockTime */
		( pxTCB ) = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY(pxDelayedTaskList );
		xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xStateListItem ) );
	}
}
