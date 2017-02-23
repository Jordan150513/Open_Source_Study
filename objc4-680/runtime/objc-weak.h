/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _OBJC_WEAK_H_
#define _OBJC_WEAK_H_

#include <objc/objc.h>
#include "objc-config.h"

__BEGIN_DECLS

/*
The weak table is a hash table governed by a single spin lock.
An allocated blob of memory, most often an object, but under GC any such 
allocation, may have its address stored in a __weak marked storage location 
through use of compiler generated write-barriers or hand coded uses of the 
register weak primitive. Associated with the registration can be a callback 
block for the case when one of the allocated chunks of memory is reclaimed. 
The table is hashed on the address of the allocated memory.  When __weak 
marked memory changes its reference, we count on the fact that we can still 
see its previous reference.

So, in the hash table, indexed by the weakly referenced item, is a list of 
all locations where this address is currently being stored.
 
For ARR, we also keep track of whether an arbitrary object is being 
deallocated by briefly placing it in the table just prior to invoking 
dealloc, and removing it via objc_clear_deallocating just prior to memory 
reclamation.

*/

/// The address of a __weak object reference
//一个weak对象的引用的地址 （指向指针的指针）weak_referrer_t指针指向一个weak对象 也就是weak_referrer_t指针里存储的是指向weak对象的指针的地址
typedef objc_object ** weak_referrer_t;

#if __LP64__
#define PTR_MINUS_1 63
#else
#define PTR_MINUS_1 31
#endif

/**
 * The internal structure stored in the weak references table. 
 * It maintains and stores
 * a hash set of weak references pointing to an object.
 * If out_of_line==0, the set is instead a small inline array.
 */

/*
 弱引用表(存储)的内部结构
 若引用表维护并存储一个（弱引用指针指向一个对象的）哈希集合
 如果out_of_line为0，那么这个弱引用表中的集合，将会用一个简单的线性数组代替。
 */
#define WEAK_INLINE_COUNT 4

//referent：指示物
//referrer:推荐介绍提到某人某事的人 也是是索引
//定义一个weak实体的类型 weak_entry_t，就是weak_table_t中存的实体单元
struct weak_entry_t {
    DisguisedPtr<objc_object> referent;//被指向的对象
    union {
        struct {
            weak_referrer_t *referrers;//referrers是一个数组，数组中存的是指针，每一个指针是指向一个weak_referrer_t类型的对象的一个指针，
                                       //也就是指向 referent 的对象们的地址（二级指针）
                                        //referrers是数组的首地址，用 calloc 在堆上分配的，所以需要需要手动，怎么知道是“用 calloc 在堆上分配的“
            uintptr_t        out_of_line : 1;
            uintptr_t        num_refs : PTR_MINUS_1;
            uintptr_t        mask;
            uintptr_t        max_hash_displacement;
        };
        struct {
            // out_of_line=0 is LSB of one of these (don't care which)
            weak_referrer_t  inline_referrers[WEAK_INLINE_COUNT];
            
            // 如果 out_of_line == 0，结构体里就只存一个数组，
            // 一样，数组里存指向 referent 的对象们的地址，数组最多可存放 4 个元素
            // 栈上分配，不需要手动 free
        };
    };
};

/**
 * The global weak references table. Stores object ids as keys,
 * and weak_entry_t structs as their values.
 */
/*
 全局的若引用表，表中存储弱引用对象按照ids作为key，弱引用实体结构作为value的键值对的形式存储(ids:weak_entry_t)
 */
//定义 全局的若引用表 weak_table_t 的结构
struct weak_table_t {
    weak_entry_t *weak_entries;//weak_entries数组，数组中存储的是指向若引用实体对象的指针们
    size_t    num_entries;//弱引用的对象的总数
    uintptr_t mask;
    uintptr_t max_hash_displacement;
};

/// Adds an (object, weak pointer) pair to the weak table.
//给弱引用表添加一个(object, weak pointer)（对象：对象的弱引用指针）键值对
id weak_register_no_lock(weak_table_t *weak_table, id referent, 
                         id *referrer, bool crashIfDeallocating);

/// Removes an (object, weak pointer) pair from the weak table.
//从弱引用表中移除一个(object, weak pointer)(对象，对象的弱引用指针)键值对
void weak_unregister_no_lock(weak_table_t *weak_table, id referent, id *referrer);

#if DEBUG
/// Returns true if an object is weakly referenced somewhere.
//如果是debug模式，就会：查询弱引用的对象是否存在全局的弱引用表中。弱引用对象在弱引用表中是否有存储他的引用指针
bool weak_is_registered_no_lock(weak_table_t *weak_table, id referent);
#endif

/// Assert a weak pointer is valid and retain the object during its use.
//确定一下 一个弱引用指针所指向的对象是否是可用的，并且将这个对象retain保留下来
id weak_read_no_lock(weak_table_t *weak_table, id *referrer);

/// Called on object destruction. Sets all remaining weak pointers to nil.
//对象销毁的时候，设置所有的弱引用指针都为nil，清除所有的weak table中的指针，置为nil
void weak_clear_no_lock(weak_table_t *weak_table, id referent);

__END_DECLS

#endif /* _OBJC_WEAK_H_ */
