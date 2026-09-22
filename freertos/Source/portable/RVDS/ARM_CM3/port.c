#include "FreeRTOS.h"
#include "task.h"
#include "ARMCM3.h"


#define portINITIAL_XPSR              ( 0x01000000 )
#define portSTART_ADDRESS_MASK          ( ( StackType_t ) 0xfffffffeUL )

#define portNVIC_SYSPRI2_REG (* ( ( volatile uint32_t * ) 0xe000ed20 ) )

#define portNVIC_PENDSV_PRI (((uint32_t) configKERNEL_INTERRUPT_PRIORITY ) << 16UL)
#define portNVIC_SYSTICK_PRI (((uint32_t) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/*SysTick控制寄存器*/
#define portNVIC_SYSTICK_CTRL_REG (*((volatile uint32_t *) 0xe000e010 ))
/*SysTick重装载寄存器*/
#define portNVIC_SYSTICK_LOAD_REG (*((volatile uint32_t *) 0xe000e014 ))
	
/*SysTick时钟源选择*/
#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	/*确保SysTick的时钟与内核时钟一致*/
	#define portNVIC_SYSTICK_CLK_BIT ( 1UL << 2UL )
#else
	#define portNVIC_SYSTICK_CLK_BIT (0)
#endif

#define portNVIC_SYSTICK_INT_BIT ( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT ( 1UL << 0UL )


/*将图表置1*/
#define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities )\
				( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
/*将图表置0*/
#define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities )\
				( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )


static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

void prvStartFirstTask( void );
void vPortSVCHandler( void );
void xPortPendSVHandler( void );
BaseType_t xTaskIncrementTick(void);

static void prvTaskExitError( void )
{
    /* 错误停止，不会执行到这里 */
    for(;;);
}


/*
*************************************************************************
函数名称：pxPortInitialiseStack
作用：初始化任务栈，伪造栈帧，模拟出一个“刚被异常/中断打断”的现场，在异常/中断返回时会自动将xPSR,R15(PC),
			R14(LR),R12,R3,R2,R1,R0载入CPU寄存器 
重点：在静态创建任务时被调用
*************************************************************************
*/
/*初始化栈时手动写一个栈帧，异常返回时自动加载到CPU的寄存器*/
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack,
                                    TaskFunction_t pxCode,
                                    void *pvParameters )
{
    /* 异常发生时自动加载到 CPU 的寄存器（初始值） */
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_XPSR;                    /* xPSR的bit24必须为1 */
    pxTopOfStack--;
    *pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK;  /* PC为任务函数地址 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) prvTaskExitError;   /* LR为错误返回地址 */
    pxTopOfStack -= 5; /* R12, R3, R2 and R1 默认初始化为 0 */
    *pxTopOfStack = ( StackType_t ) pvParameters;        /* R0为任务形参 */

    /* 异常发生时手动加载到 CPU 的寄存器（初始值） */
    pxTopOfStack -= 8;

    /* 返回栈顶指针，此时 pxTopOfStack 指向栈底 */
    return pxTopOfStack;
}


/**********************************************************************
													端口配置调度器启动  
函数名称：xPortStartScheduler
函数作用：启动调度器
重点：调低PendSV与SysTick的优先级，防止打断正在运行的更紧急的操作，如外部中断
重点函数：prvStartFirstTask
***********************************************************************/
void vPortSetupTimerInterrupt( void );

BaseType_t xPortStartScheduler(void)
{
	//配置PendSV 和 SysTick 的优先级为最低
	
	/*SysTick负责记录时间，每过一定时间产生中断
		PendSV负责任务切换
		这两个中断都是Cotrex-M3的内核中断*/
	/*设置为最低优先级是为了防止这两个异常打断正在运行的更紧急的操作*/
	portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
	portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;
	
	/*初始化SysTick*/
	vPortSetupTimerInterrupt();
	
	// 启动第一个任务，不再返回
	prvStartFirstTask();
	
	return 0;

}	


/***********************************************************************
函数名称：prvStartFirstTask
作用：开始第一个任务，调用SVC异常中断
初始状态：CPU处于特权级线程模式，使用MSP栈指针。
任务需求：1.freertos任务需要运行在非特权级的线程模式下，使用PSP栈指针（为了防止越界访问内核资源）
				  2.执行第一个任务需要将栈顶指针写入PSP，手动恢复R4-R11寄存器，自动弹出恢复PC，xPSR等寄存器
所以我们需要切换权限：特权级-->非特权级，切换栈指针：MSP-->PSP，恢复寄存器。这些都通过SVC中断实现。
困难与解决：1.切换栈指针需要修改CONTROL寄存器，普通的c代码没有权限，需要使用汇编代码
						2.关于恢复寄存器，直接使用汇编代码去写很麻烦，利用异常返回机制，在返回时CPU自动将PSP指定的栈的数据依次弹出写入对应寄存器
SVC的作用：SVC（Supervisor Call，监管者调用）是一个异常。当执行 SVC 0 指令时，CPU 会触发 SVC 异常，从而进入处理模式。
					在处理模式下CPU处于特权级，并且可以修改CONTROL寄存器，将栈指针修改为PSP。异常返回时硬件自动弹出任务栈的上下文。
************************************************************************/

__asm void prvStartFirstTask(void)
{
	PRESERVE8
	
	/*在cortex-m中，0xE000ED08是SCB_VTOR寄存器地址，
	里面存放向量表的起始地址*/
	ldr r0, =0xE000ED08	//r0内存储SCB_VTOR地址
	ldr r0, [r0]				//r0内存储向量表第一个条目地址，向量表第一个地址不是函数地址，而是初始MSP地址
	ldr r0, [r0]				//r0内存储第一个任务的栈顶指针（TCB的第一项），即初始MSP指向该TCB
	
	//设置主堆栈指针MSP
	/*msr用于写入特殊寄存器，比如现在的msp这条指令。
		现在msp内存储第一个任务的栈顶指针，即让msp指向第一个任务的堆栈*/
	//目前好像没有使用msp，而且r0内存储的不是第一个任务的栈顶指针
	msr msp, r0
	
	//使能全局中断与异常
	cpsie i
	cpsie f
	dsb
	isb
	
	//调用SVC去启动第一个任务
	svc 0
	nop
	nop
	
}

/***********************************************************************
函数名称：vPortSVCHandler
该函数是SVC中断内容，主要实现了如下功能：
	1.手动从第一个任务的伪造栈里取r4到r11存入寄存器
	2.赋值LR，指定异常返回时切换到Thread模式，并使用PSP作为栈指针，PSP指向第一个任务的栈指针（LR决定CPU异常返回时该做什么）
	3.异常返回时，自动从当亲PSP指向的堆栈里，把R0-R3, R12, LR, PC, xPSR 弹出到 CPU 寄存器。
启动SVC中断，需要函数名与向量表注册的名称一致，在FreeRTOSConfig.h中通过宏定义将vPortSVCHandler替换为SVC_Handler
***********************************************************************/
__asm void vPortSVCHandler(void)
{
	
	//msp的职责被pxCurrentTCB替代
	extern pxCurrentTCB;
	
	PRESERVE8
	
	ldr r3, =pxCurrentTCB
	ldr r1, [r3]
	
	//此时r0存储了栈顶指针
	ldr r0, [r1]
	
	//手动恢复r4到r11,其他的寄存器会被自动弹出恢复
	ldmia r0!, {r4-r11}
	
	//将新的r0 栈顶指针赋予psp
	msr psp, r0
	isb
	
	//将r0清零
	mov r0, #0
	//basepri赋0，即不屏蔽中断，在启动调度器前系统一般默认关闭中断防止干扰，现在第一个任务即将运行，必须开启中断
	msr basepri, r0
	
	//设置返回值LR（即R14），LR决定CPU异常返回时该做什么，LR = 0xFFFFFFFD意为异常返回后切换到Thread模式，并使用PSP作为栈指针
	orr r14, #0xd
	
	//异常返回
	bx r14
}


/***********************************************************************
函数名称：xPortPendSVHandler
属性：PendSV的中断函数
注意：进入PendSV中断时，r14被硬件设置为 EXC_RETURN
功能：将当前任务入栈，恢复新任务进入CPU寄存器
************************************************************************/
__asm void xPortPendSVHandler(void)
{
	extern pxCurrentTCB;
	extern vTaskSwitchContext;
	
	PRESERVE8
	
	/*保存当前任务数据到任务栈*/
	mrs r0, psp			//R0=当前任务的psp
	isb
	
	ldr r3, =pxCurrentTCB		//r3=&pxCurrentTCB
	ldr r2, [r3]						//r2=pxCurrent(指向当前TCB)
	
	stmdb r0!, {r4-r11}			//手动保存r4-r11到任务栈, ! 表示更新 R0（R0 最终指向 R4 的下方，即新的栈顶）
	str r0, [r2]						//更新TCB中的栈顶指针,将r0的32位数据存储到r2指向的内存地址，即pxCurrentTCB->pxTopOfStack = R0
	
	

	/*临界区+任务切换*/
	stmdb sp!, {r3,r14}  		//临时保存r3(r3=&pxCurrentTCB)，r14(lr)到主栈
	
	mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
	msr basepri, r0					//关中断，进入临界区
	dsb
	isb
	
	bl vTaskSwitchContext		//调用c函数：将pxCurrentTCB指向新任务
	
	mov r0, #0
	msr basepri, r0					//开中断，退出临界区
	
	
	/*恢复新任务上下文*/
	ldmia sp!, {r3,r14}			//恢复r3(r3=&pxCurrentTCB)，r14(lr)
	
	ldr r1, [r3]						//r1=pxCurrentTCB（新任务的TCB）
	ldr r0, [r1]						//r0=新任务的栈顶指针
	ldmia r0!, {r4-r11}			//手动恢复r4-r11
	msr psp, r0							//更新psp指向新的任务栈顶
	isb
	
	
	/*异常返回，CPU检测r14是EXC_RETURN，进行操作：1.从psp指向的任务栈恢复R0,R1,R2,R3,R12,LR,PC,xPSR
																							 2.更新psp
																							 3.开始执行新任务的代码*/
	bx r14									
	nop
	
}

void vPortEnterCritical(void)
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;
	
	if (uxCriticalNesting == 1)
	{
		//configASSERT((portNVIC_INT_CTRL_REG &  portVECTACTIVE_MASK )== 0 );
	}
}

void vPortExitCritical(void)
{
	//configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if (uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}

/***********************************************************************
函数名称：xPortSysTickHandler
属性：SysTick的中断函数
功能：更新时间Tick，作为系统时基
注意：真正更新系统时基的函数是xTaskIncrementTick()
************************************************************************/
void xPortSysTickHandler(void)
{
	/*关中断*/
	vPortRaiseBASEPRI();
	
	
	{
		if(xTaskIncrementTick() != pdFALSE )
		{
			taskYIELD();
		}		
	}

	/*开中断*/
	portENABLE_INTERRUPTS();
}

/***********************************************************************
函数名称：vPortSetupTimerInterrupt
作用：初始化systick中断，让其周期性调用
************************************************************************/
void vPortSetupTimerInterrupt(void)
{
	/* 设置重装载寄存器的值 */
	portNVIC_SYSTICK_LOAD_REG = (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ) -1UL;
	
	/* 设置系统定时器的时钟等于内核时钟
		 使能 SysTick 定时器中断
		 使能 SysTick 定时器 */
	portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT |
																portNVIC_SYSTICK_INT_BIT |
															  portNVIC_SYSTICK_ENABLE_BIT );
}
