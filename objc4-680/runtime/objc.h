/*
 * Copyright (c) 1999-2007 Apple Inc.  All Rights Reserved.
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
/*
 *	objc.h
 *	Copyright 1988-1996, NeXT Software, Inc.
 */

//需要对比着objc_private.h进行对比学习
#ifndef _OBJC_OBJC_H_
#define _OBJC_OBJC_H_

#include <sys/types.h>      // for __DARWIN_NULL
#include <Availability.h>
#include <objc/objc-api.h>
#include <stdbool.h>

//在objc-private中定义了#define OBJC_TYPES_DEFINED 1，所以这不分是不会被编译的，是暴露出来以供参观的
#if !OBJC_TYPES_DEFINED

//Object-C 类的一个隐式的类型定义 objc_class == Class
/// An opaque type that represents an Objective-C class.
typedef struct objc_class *Class;

/// Represents an instance of a class.
//一个类的实例对象 objc_object
struct objc_object {
    // 实际在objc_private.h中objc_object是没有Class isa这个字段的
    // 有的只是isa_t isa这个私有的字段，代表对象类型的Class cls字段存储在 isa_t 结构体中
    // Apple玩这么一手，轻易的就把底层实现给隐藏了
    Class isa  OBJC_ISA_AVAILABILITY;
};

/// A pointer to an instance of a class.
//一个类的实例的指针 一个指针 指向一个类的实例对象 id这个指针指向objc_object objc_object是一个类的实例对象
typedef struct objc_object *id;
#endif



// objc_selector 的定义在源码中并没有给出，网上的说法是映射到一个 char * 字符串
// 也就是本质上是一个 char * 字符串
// 经过下面的实验，确实如此  struct objc_selector *  == SEL == char *
//
/*
 struct objc_selector * sel_1 = @selector(methedWithStr1:str2:);
 SEL sel_2 = @selector(methedWithStr1:str2:);
 (sel_1 == sel_2) == Yes
 说明两个指针是一样的
 objc_selector结构体的指针强转成一个指向 char的字符串 也就是强转成char * 打印出来 C String 就是selector name
 
 char * str_1 = (char *)sel_1;
 printf("%s\n", str_1);  // methedWithStr1:str2:
 */

/// An opaque type that represents a method selector.
// 选择子的隐式定义类型 也就是方法选择子
typedef struct objc_selector *SEL;

/// A pointer to the function of a method implementation.
//指向一个方法实现的指针 -- IMP --implementation
//IMP的定义
// IMP 是一个指向方法的函数实现的指针

//#define OBJC_OLD_DISPATCH_PROTOTYPES 0
//object-private中定义为0
#if !OBJC_OLD_DISPATCH_PROTOTYPES
typedef void (*IMP)(void /* id, SEL, ... */ ); //会编译这个
#else
typedef id (*IMP)(id, SEL, ...); 
#endif

#define OBJC_BOOL_DEFINED

/// Type to represent a boolean value.
//代表boolean值的类型定义

#if (TARGET_OS_IPHONE && __LP64__)  ||  TARGET_OS_WATCH
#define OBJC_BOOL_IS_BOOL 1
typedef bool BOOL;
#else
#define OBJC_BOOL_IS_CHAR 1
typedef signed char BOOL;
//在这种情况下，BOOL被明确的赋值为“c” 而不是“C”，即使 -funsigned-char 已经被使用了
// BOOL is explicitly signed so @encode(BOOL) == "c" rather than "C" 
// even if -funsigned-char is used.
#endif

#if __has_feature(objc_bool)
#define YES __objc_yes
#define NO  __objc_no
#else
#define YES ((BOOL)1)
#define NO  ((BOOL)0)
#endif

#ifndef Nil
# if __has_feature(cxx_nullptr)
#   define Nil nullptr
# else
#   define Nil __DARWIN_NULL
# endif
#endif

#ifndef nil
# if __has_feature(cxx_nullptr)
#   define nil nullptr
# else
#   define nil __DARWIN_NULL
# endif
#endif

#if ! (defined(__OBJC_GC__)  ||  __has_feature(objc_arc))
#define __strong /* empty */
#endif

#if !__has_feature(objc_arc)
#define __unsafe_unretained /* empty */
#define __autoreleasing /* empty */
#endif

//返回一个方法的名字 Selector的名字，传进去一个selector，返回给方法的名字
//返回一个C string作为selector的名字

// 取得 sel 中的方法名，其实 sel 就是一个 char * 字符串
// 强转为 char * ，就得到了方法名

/** 
 * Returns the name of the method specified by a given selector.
 * 
 * @param sel A pointer of type \c SEL. Pass the selector whose name you wish to determine.
 * 
 * @return A C string indicating the name of the selector.
 */
OBJC_EXPORT const char *sel_getName(SEL sel)
    __OSX_AVAILABLE_STARTING(__MAC_10_0, __IPHONE_2_0);


//用Object-C 运行时系统注册一个方法，遍历方法名看是否是选择子，返回选择子的value----（这里不对），map应该是 映射 之意，映射 方法名 和 selector，并返回 seletor
//参数是一个 C String 类型的指针，返回是一个方法的选择子名称
// 在 runtime 中注册一个方法，映射 方法名 和 selector，并返回 seletor
/** 
 * Registers a method with the Objective-C runtime system, maps the method 
 * name to a selector, and returns the selector value.
 * 
 * @param str A pointer to a C string. Pass the name of the method you wish to register.
 * 
 * @return A pointer of type SEL specifying the selector for the named method.
 * 
 * @note You must register a method name with the Objective-C runtime system to obtain the
 *  method’s selector before you can add the method to a class definition. If the method name
 *  has already been registered, this function simply returns the selector.
 */
OBJC_EXPORT SEL sel_registerName(const char *str)
    __OSX_AVAILABLE_STARTING(__MAC_10_0, __IPHONE_2_0);


//返回给定对象的类名 参数是一个 Object-C 的实例对象  返回的是一个 类的名称

// 本质上是对象的类的 demangledName？？？如何知道的？？
/** 
 * Returns the class name of a given object.
 * 
 * @param obj An Objective-C object.
 * 
 * @return The name of the class of which \e obj is an instance.
 */
OBJC_EXPORT const char *object_getClassName(id obj)
    __OSX_AVAILABLE_STARTING(__MAC_10_0, __IPHONE_2_0);


//返回一个指针 这个指针 指向的是这个实例对象的额外分配的字节 如果没有额外分配， 就返回undefined 这块内存里紧跟着存放的是对象的传统变量，但是可能跟最后一个变量是不毗邻的
/** 
 * Returns a pointer to any extra bytes allocated with an instance given object.
 * 
 * @param obj An Objective-C object.
 * 
 * @return A pointer to any extra bytes allocated with \e obj. If \e obj was
 *   not allocated with any extra bytes, then dereferencing the returned pointer is undefined.
 * 
 * @note This function returns a pointer to any extra bytes allocated with the instance
 *  (as specified by \c class_createInstance with extraBytes>0). This memory follows the
 *  object's ordinary ivars, but may not be adjacent(毗连) to the last ivar（变量）.
 * @note The returned pointer is guaranteed to be pointer-size aligned, even if the area following
 *  the object's last ivar is less aligned than that. Alignment greater than pointer-size is never
 *  guaranteed, even if the area following the object's last ivar is more aligned than that.
 * @note In a garbage-collected environment, the memory is scanned conservatively.
 */
// 取得 obj 对象中 extraBytes 额外数据的地址
OBJC_EXPORT void *object_getIndexedIvars(id obj)
    __OSX_AVAILABLE_STARTING(__MAC_10_0, __IPHONE_2_0);

//确定一个selector 是否是可用的 是否有一个对应的方法实现，在一些平台上，一个未知的 么有实现的selector 可能会导致崩溃
/** 
 * Identifies a selector as being valid or invalid.
 * 
 * @param sel The selector you want to identify.
 * 
 * @return YES if selector is valid and has a function implementation, NO otherwise. 
 * 
 * @warning On some platforms, an invalid reference (to invalid memory addresses) can cause
 *  a crash. 
 */
OBJC_EXPORT BOOL sel_isMapped(SEL sel)
    __OSX_AVAILABLE_STARTING(__MAC_10_0, __IPHONE_2_0);

//用运行时注册一个方法 传入方法名的 c String 返回一个指针 这个指针西乡了选择子的名称说明 ，如果没有找到，返回null
/** 
 * Registers a method name with the Objective-C runtime system.
 * 
 * @param str A pointer to a C string. Pass the name of the method you wish to register.
 * 
 * @return A pointer of type SEL specifying the selector for the named method.
 * 
 * @note The implementation of this method is identical to the implementation of \c sel_registerName.
 * @note Prior to OS X version 10.0, this method tried to find the selector mapped to the given name
 *  and returned \c NULL if the selector was not found. This was changed for safety, because it was
 *  observed that many of the callers of this function did not check the return value for \c NULL.
 */
OBJC_EXPORT SEL sel_getUid(const char *str)
    __OSX_AVAILABLE_STARTING(__MAC_10_0, __IPHONE_2_0);

//淘汰的ARC会话  即将到来的废除
//用CFBridgingRetain，CFBridgingRelease 和 __bridge casts 替代
// Obsolete ARC conversions. Deprecation forthcoming.
// Use CFBridgingRetain, CFBridgingRelease, and __bridge casts instead.

typedef const void* objc_objectptr_t;

#if __has_feature(objc_arc)
#   define objc_retainedObject(o) ((__bridge_transfer id)(objc_objectptr_t)(o))
#   define objc_unretainedObject(o) ((__bridge id)(objc_objectptr_t)(o))
#   define objc_unretainedPointer(o) ((__bridge objc_objectptr_t)(id)(o))
#else
#   define objc_retainedObject(o) ((id)(objc_objectptr_t)(o))
#   define objc_unretainedObject(o) ((id)(objc_objectptr_t)(o))
#   define objc_unretainedPointer(o) ((objc_objectptr_t)(id)(o))
#endif


#if !__OBJC2__

// The following declarations are provided here for source compatibility.

#if defined(__LP64__)
    typedef long arith_t;
    typedef unsigned long uarith_t;
#   define ARITH_SHIFT 32
#else
    typedef int arith_t;
    typedef unsigned uarith_t;
#   define ARITH_SHIFT 16
#endif

typedef char *STR;

#define ISSELECTOR(sel) sel_isMapped(sel)
#define SELNAME(sel)	sel_getName(sel)
#define SELUID(str)	sel_getUid(str)
#define NAMEOF(obj)     object_getClassName(obj)
#define IV(obj)         object_getIndexedIvars(obj)

#endif

#endif  /* _OBJC_OBJC_H_ */
