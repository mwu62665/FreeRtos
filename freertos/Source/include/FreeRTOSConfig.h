/* FreeRTOSConfig.h - 野火 FreeRTOS 例程配置文件 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* 1. 系统时钟配置：0=使用 32 位计数器，1=使用 16 位计数器 */
#define configUSE_16_BIT_TICKS                    0

/* 2. 任务名称最大长度（字符数） */
#define configMAX_TASK_NAME_LEN                   (16)

/* 3. 支持静态内存分配（1=开启，配合 xTaskCreateStatic 使用） */
#define configSUPPORT_STATIC_ALLOCATION           1

/* 4. 最大优先级数量（0~4，共 5 级） */
#define configMAX_PRIORITIES                      (5)

/* 5. 内核中断优先级
 *    255 = 0xFF，高四位有效 = 0xF = 15（最低优先级）
 *    这是 PendSV 和 SysTick 的中断优先级 */
#define configKERNEL_INTERRUPT_PRIORITY           255

/* 6. 系统调用中断最大优先级
 *    191 = 0xBF，高四位有效 = 0xB = 11
 *    高于此优先级的中断不能调用 FreeRTOS API（如 xQueueSendFromISR） */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY      191

/* 7. 中断处理函数重映射
 *    将 FreeRTOS 移植层的中断处理函数映射到 CMSIS 标准命名 */
#define xPortPendSVHandler      PendSV_Handler
#define xPortSysTickHandler     SysTick_Handler
#define vPortSVCHandler         SVC_Handler

/*8.系统时钟频率*/
#define configCPU_CLOCK_HZ (( unsigned long ) 25000000)
#define configTICK_RATE_HZ (( TickType_t ) 100)

/*9.*/
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 128 )


/*configUSE_PREEMPTION 是在 FreeRTOSConfig.h 的一个宏，默认
为 1，表示有任务就绪且就绪任务的优先级比当前优先级高时，需要执行一次任务切换，
即将 xSwitchRequired 的值置为 pdTRUE。 */
#define configUSE_PREEMPTION            1

#define configUSE_TIME_SLICING  1
#endif /* FREERTOS_CONFIG_H */
