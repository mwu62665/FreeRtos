#ifndef PORTMACRO_H
#define PORTMACRO_H

#include "stdint.h"
#include "stddef.h"
#include "FreeRTOSConfig.h"

/*基础类型重定义*/

#define portCHAR	 char
#define portFLOAT  float
#define portDOUBLE double
#define portLONG   long
#define portSHORT  short
#define portSTACK_TYPE uint32_t
#define portBASE_TYPE long

typedef portSTACK_TYPE StackType_t;
typedef portBASE_TYPE BaseType_t;
typedef unsigned long UBaseType_t;

#if (configUSE_16_BIT_TICKS == 1)
typedef uint16_t TickType_t;
//portMAX_DELAY宏定义
#define portMAX_DELAY (TickType_t) 0xffff

#else
typedef uint32_t TickType_t;
#define portMAX_DELAY (TickType_t) 0xffffffffUL
#endif


/* 中断控制状态寄存器，0xe000ed04
 * Bit 28 PENDSVSET: PendSV 挂起位
 */
#define portNVIC_INT_CTRL_REG		( * ( ( volatile uint32_t * ) 0xe000ed04 ) )	//NVIC中断控制器寄存器
#define portNVIC_PENDSVSET_BIT		( 1UL << 28UL )															//NVIC中断寄存器状态之一，该位28置1后选中挂起PendSV异常

#define portSY_FULL_READ_WRITE		( 15 )																			//保证原子操作，必须彻底隔离前面的指令和后面的指令

#define portYIELD()																\
{																				\
	/* 触发PendSV异常，进行任务切换 */								                \
	portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;								\
	__dsb( portSY_FULL_READ_WRITE );											\
	__isb( portSY_FULL_READ_WRITE );											\
}

#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
	#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1

	/* 检测优先级配置 */
	#if( configMAX_PRIORITIES > 32 )
		#error configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice.
	#endif

	/* 根据优先级设置/清除优先级位图中相应的位 */
	#define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
	#define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )

	/*-----------------------------------------------------------*/

	#define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities ) uxTopPriority = ( 31UL - ( uint32_t ) __clz( ( uxReadyPriorities ) ) )

#endif /* taskRECORD_READY_PRIORITY */

/*
************************************************************************
                                临界段保护 Start
************************************************************************
*/

/* 不带返回值的关闭中断函数，不能嵌套，不能在中断函数中使用 */
#define portDISABLE_INTERRUPTS() vPortRaiseBASEPRI()

/* 带返回值的关闭中断函数，不能嵌套，不能在中断函数中使用 */
/*由ulRetrun保存原来的优先级，后面用该变量去恢复原优先级*/
#define portSET_INTERRUPT_MASK_FROM_ISR() ulPortRaiseBASEPRI()



/*进入临界段，不能嵌套*/
#define portENTER_CRITICAL() vPortEnterCritical()
#define portDISABLE_INTERRUPTS() vPortRaiseBASEPRI()

/*进入临界段，不能嵌套*/
#define portSET_INTERRUPT_MASK_FROM_ISR() ulPortRaiseBASEPRI()

/*退出临界段，不能嵌套*/
#define portEXIT_CRITICAL() vPortExitCritical()
#define portENABLE_INTERRUPTS() vPortSetBASEPRI(0)

/*退出临界段，不能嵌套*/
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x) vPortSetBASEPRI(x)

#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )

#define portINLINE __inline

#ifndef portFORCE_INLINE
	#define portFORCE_INLINE __forceinline
#endif


/* 不带返回值的关闭中断函数，不能嵌套，不能在中断函数中使用 */
static portFORCE_INLINE void vPortRaiseBASEPRI(void)
{
	//系统中断的屏蔽优先级,优先级数目越大的优先级越低
	uint32_t ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
	__asm
	{
		//关闭所有中断
		msr basepri, ulNewBASEPRI
		dsb
		isb
	}
}

/* 带返回值的关闭中断函数，不能嵌套，不能在中断函数中使用 */
static portFORCE_INLINE uint32_t ulPortRaiseBASEPRI(void)
{
	uint32_t ulRetrun, ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
	
	__asm
	{
		mrs ulRetrun, basepri
		msr basepri, ulNewBASEPRI
		dsb
		isb
	}
	return ulRetrun;
}

/*开中断函数，ulBASEPRI=0 作为参数传入临界保护的关闭中断函数*/
static portFORCE_INLINE void vPortSetBASEPRI(uint32_t ulBASEPRI)
{
	__asm
	{
		msr basepri, ulBASEPRI
	}
}

/*
************************************************************************
                                临界段保护 End
************************************************************************
*/

#endif
