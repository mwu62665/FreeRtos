#include "list.h"

/*
************************************************************************
*                                节点初始化
************************************************************************
*/
void vListInitialiseItem(ListItem_t * const pxItem)
{
	//初始化该节点的所在链表为空，表明该节点没有插入任何链表
	pxItem->pvContainer = NULL;
}

/*
************************************************************************
*                                链表初始化
************************************************************************
*/
void vListInitialise( List_t * const pxList )
{
	//将链表索引指针指向最后一个节点
	pxList->pxIndex = (ListItem_t * ) & (pxList->xListEnd);
	
	// 将最后一个节点的pxNext、pxPrevious指针指向自己，表示链表为空
	pxList->xListEnd.pxNext = (ListItem_t * ) & (pxList->xListEnd);
	pxList->xListEnd.pxPrevious = (ListItem_t * ) & (pxList->xListEnd);
	
	//将结尾节点的辅助值设为最大值，作为升序插入时的哨兵
	pxList->xListEnd.xItemValue = portMAX_DELAY;
	
	//将链表节点计数器置为0，表示链表为空
	pxList->uxNumberOfItems = ( UBaseType_t) 0U;
}


/*
************************************************************************
*                              将节点插入到链表末尾
************************************************************************
*/
/*
为什么要用pxIndex，而不是直接去操控表头节点，也就是不直接使用链表中的pxList->pxIndex
1.放在CPU的寄存器上，访问速度快，而pxList是传入的指针，存储在内存里面。
2.用局部变量可以防止pxList->pxIndex被改变指向
*/
void vListInsertEnd(List_t * const pxList, ListItem_t * const pxNewListItem)
{
	//指向链表表头节点的指针，用于操控表头节点
	ListItem_t * const pxIndex = pxList->pxIndex;
	
	//改变新节点的指针指向,指向表头节点
	pxNewListItem->pxNext = pxIndex;
	pxNewListItem->pxPrevious = pxIndex->pxPrevious;
	//改变表头节点指针指向新节点
	pxIndex->pxPrevious->pxNext = pxNewListItem;
	pxIndex->pxPrevious = pxNewListItem;
	
	//记录该节点所在的链表
	pxNewListItem->pvContainer = (void*) pxList;
	
	//链表节点计数器++
	(pxList ->uxNumberOfItems)++;
}


/*
************************************************************************
*                    将节点按升序排列插入到链表中
************************************************************************
*/

void vListInsert(List_t * const pxList, ListItem_t * const pxNewListItem)
{
	ListItem_t * pxIterator;
	
	//获取新节点的辅助值（优先级）
	const TickType_t xValueOfInsert = pxNewListItem->xItemValue;
	
	//寻找新节点要插入的位置
	//1.插入到链表末尾
	if (xValueOfInsert == portMAX_DELAY)
	{
		pxIterator = pxList->xListEnd.pxPrevious;
	}
	//2.插入到链表头部
	else
	{
		for (pxIterator = (ListItem_t * ) & (pxList->xListEnd);
					pxIterator->pxNext->xItemValue <= xValueOfInsert;
					pxIterator = pxIterator->pxNext);
	}


	//pxNewListItem插入pxIterator后面
	pxNewListItem->pxNext = pxIterator->pxNext;
	pxNewListItem->pxNext->pxPrevious = pxNewListItem;
	pxNewListItem->pxPrevious = pxIterator;
	pxIterator->pxNext = pxNewListItem;
	
	//新节点记录所属链表
	pxNewListItem->pvContainer = (void*) pxList;
	
	//链表节点计数器++
	(pxList->uxNumberOfItems)++;
	
}


/*
************************************************************************
*                    将节点从链表中删除
************************************************************************
*/
UBaseType_t uxListRemove(ListItem_t * const pxItemToRemove)
{
	//获取节点所在的链表
	List_t * const pxList = (List_t*) pxItemToRemove->pvContainer;
	
	//将指定节点从链表中删除
	pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
	pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;
	
	//重新调整索引指针
	if (pxList->pxIndex == pxItemToRemove)
	{
		pxList->pxIndex = pxItemToRemove->pxPrevious;
	}
	
	//初始化该节点的所在链表
	pxItemToRemove->pvContainer = NULL;
	
	//链表节点计数器--
	(pxList->uxNumberOfItems)--;
	
	//返回剩余节点个数
	return pxList->uxNumberOfItems;
}
