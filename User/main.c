#include "FreeRTOS.h"
#include "task.h"

/**************************************************************************************
																      全局变量
***************************************************************************************/
portCHAR flag1;
portCHAR flag2;
portCHAR flag3;

extern List_t pxReadyTasksLists[ configMAX_PRIORITIES ];

/**************************************************************************************
																   任务控制块 & STACK
***************************************************************************************/
TaskHandle_t Task1_Handle;
#define TASK1_STACK_SIZE 128
StackType_t Task1Stack[TASK1_STACK_SIZE];
TCB_t Task1TCB;

TaskHandle_t Task2_Handle;
#define TASK2_STACK_SIZE 128
StackType_t Task2Stack[TASK2_STACK_SIZE];
TCB_t Task2TCB;

TaskHandle_t Task3_Handle;
#define TASK3_STACK_SIZE 128
StackType_t Task3Stack[TASK3_STACK_SIZE];
TCB_t Task3TCB;


/**************************************************************************************
																    函数声明
***************************************************************************************/
void delay (uint32_t count);
void Task1_Entry( void *p_arg );
void Task2_Entry( void *p_arg );
void Task3_Entry( void *p_arg );

/**************************************************************************************
																   main 函数
***************************************************************************************/
int main()
{
	/*硬件初始化*/
	
	
	/*创建任务*/
	Task1_Handle = xTaskCreateStatic(Task1_Entry,
										"Task1_Entry",
										TASK1_STACK_SIZE,
										NULL,
										(UBaseType_t) 2,
										Task1Stack,
										&Task1TCB);
	
	Task2_Handle = xTaskCreateStatic(Task2_Entry,
										"Task2_Entry",
										TASK2_STACK_SIZE,
										NULL,
										(UBaseType_t) 2,
										Task2Stack,
										&Task2TCB);
	
	Task3_Handle = xTaskCreateStatic(Task3_Entry,
										"Task3_Entry",
										TASK3_STACK_SIZE,
										NULL,
										(UBaseType_t) 3,
										Task3Stack,
										&Task3TCB);
	
	
	portDISABLE_INTERRUPTS();
	
	vTaskStartScheduler();
	
	
	for(;;)
	{
		/*系统启动后不会到达这里*/
	}
}



/**************************************************************************************
																   main 函数
***************************************************************************************/
void delay (uint32_t count)
{
	for (; count!=0; count--);
}

void Task1_Entry( void *p_arg )
{
	for ( ;; )
	{
		
		flag1 = 1;
		delay( 100 );
		flag1 = 0;
		delay( 100 );
	}
}

void Task2_Entry( void *p_arg )
{
	for ( ;; )
	{
		
		flag2 = 1;
		delay( 100 );
		flag2 = 0;
		delay( 100 );
	}
}

void Task3_Entry( void *p_arg )
{
	for ( ;; )
	{
		
		flag3 = 1;
		vTaskDelay(100);
		flag3 = 0;
		vTaskDelay(100);
	}
}


/**********************************************************************
函数名称：vApplicationGetIdleTaskMemory
作用：给空闲函数填参数，在vTaskStartScheduler中被调用
***********************************************************************/
StackType_t IdleTaskStack[configMINIMAL_STACK_SIZE];
TCB_t IdleTaskTCB;
void vApplicationGetIdleTaskMemory( TCB_t **ppxIdleTaskTCBBuffer,
																		StackType_t **ppxIdleTaskStackBuffer,
																		uint32_t *pulIdleTaskStackSize)
{
	*ppxIdleTaskTCBBuffer = &IdleTaskTCB;
	*ppxIdleTaskStackBuffer = IdleTaskStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
