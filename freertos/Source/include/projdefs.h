#ifndef PROJDEFS_H
#define PROJDEFS_H

typedef void (*TaskFunction_t)( void * );	/*任务入口*/

#define pdFALSE ( ( BaseType_t ) 0 )
#define pdTRUE ( ( BaseType_t ) 1 )

 #define pdPASS ( pdTRUE )
 #define pdFAIL ( pdFALSE )
 
 #endif /* PROJDEFS_H */
