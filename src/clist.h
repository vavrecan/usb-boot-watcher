/*
  Taken and modified from Linux (which is under GNU licence) source code (include/linux/list.h).

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef __LIST_H__
#define __LIST_H__

typedef struct ListHead 
{
	struct ListHead *Next;
	struct ListHead *Prev;
} ListHead;


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_POISON1  ((ListHead *) 0x00000100)
#define LIST_POISON2  ((ListHead *) 0x00000200)

#define LIST_ENTRY(ptr, type, member) \
CONTAINER_OF(ptr, type, member)

static __inline void __list_add(ListHead *xnew, ListHead *prev, ListHead *next)
{
	next->Prev = xnew;
	xnew->Next = next;
	xnew->Prev = prev;
	prev->Next = xnew;
}

static __inline void ListAdd(ListHead *xnew, ListHead *head)
{
	__list_add(xnew, head, head->Next);
}


static __inline void list_add_tail(ListHead *xnew, ListHead *head)
{
	__list_add(xnew, head->Prev, head);
}

static __inline void __list_del(ListHead *prev, ListHead *next)
{
	next->Prev = prev;
	prev->Next = next;
}

static __inline void ListRemove(ListHead *entry)
{
	__list_del(entry->Prev, entry->Next);
	entry->Next = LIST_POISON1;
	entry->Prev = LIST_POISON2;
}

static __inline int ListEmpty(ListHead *head)
{
	return head->Next == head;
}

static __inline void InitListHead(ListHead *head)
{
	head->Next = head;
	head->Prev = head;
}

#endif

#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)    ((ULONG)(ULONG_PTR)&(((type *)0)->field))
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(address, type, field)    \
((type *)((char *)(address) - FIELD_OFFSET(type, field)))
#endif
