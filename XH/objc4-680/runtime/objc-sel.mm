/*
 * Copyright (c) 2012 Apple Inc.  All Rights Reserved.
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

#if __OBJC2__

#include "objc-private.h"
#include "objc-cache.h"

#if SUPPORT_PREOPT
static const objc_selopt_t *builtins = NULL;
#endif

#if SUPPORT_IGNORED_SELECTOR_CONSTANT
#error sorry
#endif


static size_t SelrefCount = 0;
        //NXMapTable 是在maptable.h中定义的
        /*
         typedef struct _NXMapTable {
        const struct _NXMapTablePrototype	*prototype;
        unsigned	count;
        unsigned	nbBucketsMinusOne;
        void	*buckets;
        } NXMapTable OBJC_MAP_AVAILABILITY;
         */
static NXMapTable *namedSelectors;  //用来存啥？？？？

static SEL search_builtins(const char *key);  //这个方法干啥的？返回了一个SEL类型的值 向前声明的方法 使用在后面 实现在后后面


/***********************************************************************
* sel_init
* Initialize selector tables and register selectors used internally.
  Initialize方法表 和 注册方法的时候 内部使用这个方法
**********************************************************************/
void sel_init(bool wantsGC, size_t selrefCount)
{
    // save this value for later
    SelrefCount = selrefCount; //自身引用的count计数

#if SUPPORT_PREOPT
    builtins = preoptimizedSelectors();//这个方法定义在objc-opt.mm文件中  返回预先优化过的方法们？？？？？？

    if (PrintPreopt  &&  builtins) {  //builtins 是在objc-share-cache中定义的 objc_stringhash_t struct
        uint32_t occupied = builtins->occupied;
        uint32_t capacity = builtins->capacity;
        
        _objc_inform("PREOPTIMIZATION: using selopt at %p", builtins);
        _objc_inform("PREOPTIMIZATION: %u selectors", occupied);
        _objc_inform("PREOPTIMIZATION: %u/%u (%u%%) hash table occupancy",
                     occupied, capacity,
                     (unsigned)(occupied/(double)capacity*100));
        }
#endif

    // Register selectors used by libobjc

    if (wantsGC) {
        // Registering retain/release/autorelease requires GC decision first.
        // sel_init doesn't actually need the wantsGC parameter, it just 
        // helps enforce the initialization order.
    }

    //sel_registerNameNoLock()这个方法在后面实现了注册方法的名字 不加锁 调用这里 外面加了锁
#define s(x) SEL_##x = sel_registerNameNoLock(#x, NO)
#define t(x,y) SEL_##y = sel_registerNameNoLock(#x, NO)

    sel_lock();
   //注册方法名字？？ 以后就可以识别这些方法了？？？？
    s(load);
    s(initialize);
    t(resolveInstanceMethod:, resolveInstanceMethod);
    t(resolveClassMethod:, resolveClassMethod);
    t(.cxx_construct, cxx_construct);
    t(.cxx_destruct, cxx_destruct);
    s(retain);
    s(release);
    s(autorelease);
    s(retainCount);
    s(alloc);
    t(allocWithZone:, allocWithZone);
    s(dealloc);
    s(copy);
    s(new);
    s(finalize);
    t(forwardInvocation:, forwardInvocation);
    t(_tryRetain, tryRetain);
    t(_isDeallocating, isDeallocating);
    s(retainWeakReference);
    s(allowsWeakReference);

    sel_unlock();

#undef s
#undef t
}


// 创建一个 selector，如果设置需要 copy，就在堆中分配内存-----参数copy传yes，就会是在堆中分配内存 存这个 name 然后返回 否则是在哪里？？栈？？应该不是栈  全局区域？ 代码区域 不是 常量区 应该是常量区 说了 const嘛
// 因为 selector 本质上就是 char * 字符串，所以直接强转就可以了
static SEL sel_alloc(const char *name, bool copy)
{
    selLock.assertWriting();
    return (SEL)(copy ? strdup(name) : name);
    //strdup() ----- char	*strdup(const char *__s1);
}

// 取得 sel 中的方法名，其实 sel 就是一个 char * 字符串
// 强转为 char * ，就得到了方法名
const char *sel_getName(SEL sel) //验证的demo是用的这个方法吗？ 也可以这样获取到name 见demo test
{
    // 不能为空
    if (!sel) {
        return "<null selector>";
    }
    // sel 本质上就是一个 char * 字符串
    return (const char *)(const void*)sel;
}

//干啥的？？？判断是否进行了 映射？？？
BOOL sel_isMapped(SEL sel) 
{
    if (!sel) return NO; //nil 的时候 还说啥

    const char *name = (const char *)(void *)sel; //这个强转是几个意思？？？？

    if (sel == search_builtins(name)) return YES;   //在XX里面能够找到，说明是已经映射过了

    rwlock_reader_t lock(selLock);  //没有映射的话 ，就需要上 readwrite的锁
    if (namedSelectors) {
        return (sel == (SEL)NXMapGet(namedSelectors, name));
    }
    return false;
}


static SEL search_builtins(const char *name) 
{
#if SUPPORT_PREOPT
    if (builtins) return (SEL)builtins->get(name); //name作为key去按照偏移量的方式去索引一个值 具体的是啥 只能以后慢慢看了
#endif
    return nil;
}

// 注册 SEL 的名字，能决定是否加锁和拷贝，拷贝即是否深拷贝 name，见 sel_alloc()
// 调用者：sel_getUid() / sel_registerName() / sel_registerNameNoLock()
static SEL __sel_registerName(const char *name, int lock, int copy) 
{
    SEL result = 0;

    if (lock) selLock.assertUnlocked();
    else selLock.assertWriting();

    if (!name) return (SEL)0;

    result = search_builtins(name);
    if (result) return result;
    
    if (lock) selLock.read();
    if (namedSelectors) {
        result = (SEL)NXMapGet(namedSelectors, name);
    }
    if (lock) selLock.unlockRead();
    if (result) return result;

    // No match. Insert.

    if (lock) selLock.write();

    if (!namedSelectors) {
        namedSelectors = NXCreateMapTable(NXStrValueMapPrototype, 
                                          (unsigned)SelrefCount);
    }
    if (lock) {
        // Rescan in case it was added while we dropped the lock
        result = (SEL)NXMapGet(namedSelectors, name);
    }
    if (!result) {
        result = sel_alloc(name, copy);
        // fixme choose a better container (hash not map for starters)
        NXMapInsert(namedSelectors, sel_getName(result), result);
    }

    if (lock) selLock.unlockWrite();
    return result;
}

// 注册 SEL 的名字，加锁、深拷贝
SEL sel_registerName(const char *name) {
    return __sel_registerName(name, 1, 1);     // YES lock, YES copy
}

// 注册 SEL 的名字，不加锁  fix-up的时候需要做这个事情 对协议的每一个方法的的名字
SEL sel_registerNameNoLock(const char *name, bool copy) {
    return __sel_registerName(name, 0, copy);  // NO lock, maybe copy
}

// 给 selLock 上写锁
void sel_lock(void)
{
    selLock.write();
}

// selLock 释放写锁
void sel_unlock(void)
{
    selLock.unlockWrite();
}


// 2001/1/24
// the majority of uses of this function (which used to return NULL if not found)
// did not check for NULL, so, in fact, never return NULL
//
SEL sel_getUid(const char *name) {
    return __sel_registerName(name, 2, 1);  // YES lock, YES copy
}


BOOL sel_isEqual(SEL lhs, SEL rhs)
{
    return bool(lhs == rhs);
}


#endif
