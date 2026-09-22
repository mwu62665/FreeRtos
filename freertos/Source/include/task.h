#ifndef INC_TASK_H
#define INC_TASK_H

#include "FreeRTOS.h"

#define taskYIELD() portYIELD()
#define tskIDLE_PRIORITY ( ( UBaseType_t ) 0U )

/*任务就绪列表*/
extern List_t pxReadyTasksLists[ configMAX_PRIORITIES ];

/*******************************************************
属性：TCB结构体
重要参数：1. ...
					2. ...
					... ...
					5.xTicksToDelay 是任务控制块的一个成员，用于记录任务需要延时
						的时间，单位为 SysTick 的中断周期。
********************************************************/
typedef struct tskTaskControlBlock
{
	volatile StackType_t *pxTopOfStack; /*栈顶*/

	ListItem_t xStateListItem;					/*状态节点*/

	StackType_t *pxStack;								/*栈起始地址*/

	char pcTaskName[ configMAX_TASK_NAME_LEN ];/*任务名*/
	
	TickType_t xTicksToDelay; /* 用于延时 */ 
	
	UBaseType_t uxPriority;		/*任务优先级*/
}tskTCB;

typedef tskTCB TCB_t;

typedef void * TaskHandle_t;

#if( configSUPPORT_STATIC_ALLOCATION == 1 )  /* 是否支持静态分配 */

TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,           /* 任务函数 */
                                const char * const pcName,           /* 任务名称 */
                                const uint32_t ulStackDepth,         /* 任务栈大小 */
                                void * const pvParameters,           /* 任务形参 */
                                UBaseType_t Priority,								 /* 任务优先级 */
																StackType_t * const puxStackBuffer,  /* 任务栈起始地址 */
                                TCB_t * const pxTaskBuffer );        /* TCB 起始地址 */

#endif /* configSUPPORT_STATIC_ALLOCATION == 1 */

/*将任务就绪列表初始化*/
void prvInitialiseTaskLists(void);

/*启动调度器*/																
void vTaskStartScheduler(void);
	
/*切换任务*/
void vTaskSwitchContext(void);

void vTaskDelay( const TickType_t xTicksToDelay );																

/*进入临界区，不保存旧值，不能嵌套*/
#define taskENTER_CRITICAL() portENTER_CRITICAL()

/*进入临界区，保存旧值，允许嵌套*/
#define taskENTER_CRITICAL_FROM_ISR() portSET_INTERRUPT_MASK_FROM_ISR()

/*退出临界区，不能嵌套*/
#define taskEXIT_CRITICAL() portEXIT_CRITICAL()

/*退出临界区，保存旧值，允许嵌套*/
#define taskEXIT_CRITICAL_FROM_ISR(x) portCLEAR_INTERRUPT_MASK_FROM_ISR(x)



#endif
