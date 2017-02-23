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
 *	objc-private.h
 *	Copyright 1988-1996, NeXT Software, Inc.
 */

#ifndef _OBJC_PRIVATE_H_
#define _OBJC_PRIVATE_H_

#include "objc-config.h"

/* Isolate（使脱离 隔离） ourselves from the definitions of id and Class in the compiler
 * and public headers.
 */

#ifdef _OBJC_OBJC_H_ //objc.h中定义了这个_OBJC_OBJC_H_
#error include objc-private.h before other headers
#endif

#define OBJC_TYPES_DEFINED 1
#define OBJC_OLD_DISPATCH_PROTOTYPES 0

#include <cstddef>  // for nullptr_t
#include <stdint.h>
#include <assert.h>

//向前声明 objc_class 的实现在 objc-runtime-new 里，它继承自 objc_object ------下一步需要去objc-runtime-new确认
struct objc_class;//类
struct objc_object;//实例对象

typedef struct objc_class *Class; // Class 是指向一个objc_class(结构体)的指针
typedef struct objc_object *id;   // id是一个指向一个objc_object(结构体)的指针

//这里是匿名的 namespace
namespace {
    struct SideTable;
};
/*
 struct objc_object {
 // 实际在objc_private.h中objc_object是没有Class isa这个字段的
 // 有的只是isa_t isa这个私有的字段，代表对象类型的Class cls字段存储在 isa_t 结构体中
 // Apple玩这么一手，轻易的就把底层实现给隐藏了
 Class isa  OBJC_ISA_AVAILABILITY;
 };
 */

#pragma mark - isa_t 联合体的定义

union isa_t //isa的真实替代 isa只是暴露出来的供参观的，其实是isa_t
{
    isa_t() { }  //析构函数？？？ 是构造函数 构造函数和析构函数的区别 (*^__^*) 嘻嘻……
    isa_t(uintptr_t value) : bits(value) { }   //???

    Class cls; //类
    uintptr_t bits;

    
    // 用 64 bit 存储一个内存地址显然是种浪费，毕竟很少有那么大内存的设备。于是可以优化存储方案，用一部分额外空间存储其他内容。isa 指针第一位 indexed 为 1 即表示使用优化的 isa 指针
    
    // SUPPORT_NONPOINTER_ISA 用于标记是否支持优化的 isa 指针，其字面含义意思是 isa 的内容不再是类的指针了，而是包含了更多信息，比如 引用计数，析构状态，被其他 weak 变量引用情况。
    
#if SUPPORT_NONPOINTER_ISA  //支持非指针内容的isa 也就是支持优化了之后的isa 会将isa的高位标示一些标志

    // extra_rc must be the MSB-most(指最高有效位) field (so it matches carry/overflow flags)
    // indexed must be the LSB (fixme or get rid of it)
    // shiftcls must occupy the same bits that a real class pointer would
    // bits + RC_ONE is equivalent to extra_rc + 1
    // RC_HALF is the high bit of extra_rc (i.e. half of its range)

    // extra_rc 必须在最高的有效位
    // indexed 必须在最低的位置
    // 因为 RC_ONE 是 1ULL<<45 ，所以bits + RC_ONE 等于 extra_rc + 1  ？？？？？？
    // RC_HALF（18位） 是 extra_rc（19位）能存储的最大数的一半  ？？？？？
    
    // future expansion:
    // uintptr_t fast_rr : 1;     // no r/r overrides
    // uintptr_t lock : 2;        // lock for atomic property, @synch
    // uintptr_t extraBytes : 1;  // allocated with extra bytes

# if __arm64__
#   define ISA_MASK        0x0000000ffffffff8ULL //说明 第0到63位中 第28位到第60位的内容存的是ISA真实的地址
#   define ISA_MAGIC_MASK  0x000003f000000001ULL  //MSAK可以指明具体的存放位置
#   define ISA_MAGIC_VALUE 0x000001a000000001ULL
    struct {
        uintptr_t indexed           : 1;
        uintptr_t has_assoc         : 1;
        uintptr_t has_cxx_dtor      : 1;
        uintptr_t shiftcls          : 33; // MACH_VM_MAX_ADDRESS 0x1000000000
        uintptr_t magic             : 6;
        uintptr_t weakly_referenced : 1;
        uintptr_t deallocating      : 1;
        uintptr_t has_sidetable_rc  : 1;
        uintptr_t extra_rc          : 19;
#       define RC_ONE   (1ULL<<45)
#       define RC_HALF  (1ULL<<18)
    };

# elif __x86_64__
#   define ISA_MASK        0x00007ffffffffff8ULL
#   define ISA_MAGIC_MASK  0x001f800000000001ULL
#   define ISA_MAGIC_VALUE 0x001d800000000001ULL
    struct {
        uintptr_t indexed           : 1;
        uintptr_t has_assoc         : 1;
        uintptr_t has_cxx_dtor      : 1;
        uintptr_t shiftcls          : 44; // MACH_VM_MAX_ADDRESS 0x7fffffe00000
        uintptr_t magic             : 6;
        uintptr_t weakly_referenced : 1;
        uintptr_t deallocating      : 1;
        uintptr_t has_sidetable_rc  : 1;
        uintptr_t extra_rc          : 8;
#       define RC_ONE   (1ULL<<56)
#       define RC_HALF  (1ULL<<7)
    };

# else
    // Available bits in isa field are architecture-specific.
#   error unknown architecture
# endif

// SUPPORT_NONPOINTER_ISA
#endif

};

/*
 objc.h中的objc_object定义
 struct objc_object {
 // 实际在objc_private.h中objc_object是没有Class isa这个字段的
 // 有的只是isa_t isa这个私有的字段，代表对象类型的Class cls字段存储在 isa_t 结构体中
 // Apple玩这么一手，轻易的就把底层实现给隐藏了
 Class isa  OBJC_ISA_AVAILABILITY;
 };
 */

//objc_object的定义：
struct objc_object {
private:
    isa_t isa;  //isa 64位 中存了不少信息，包括对象的类型，引用计数和其他乱七八糟的 如上笔记
                // 是 objc_object 结构体内唯一一个成员变量

public:

    // ISA() assumes this is NOT a tagged pointer object
    Class ISA();

    // getIsa() allows this to be a tagged pointer object
    Class getIsa();

    // initIsa() should be used to init the isa of new objects only.
    // If this object already has an isa, use changeIsa() for correctness.
    // initInstanceIsa(): objects with no custom RR/AWZ
    // initClassIsa(): class objects
    // initProtocolIsa(): protocol objects
    // initIsa(): other objects
    void initIsa(Class cls /*indexed=false*/);
    void initClassIsa(Class cls /*indexed=maybe*/);
    void initProtocolIsa(Class cls /*indexed=maybe*/);
    void initInstanceIsa(Class cls, bool hasCxxDtor);

    // changeIsa() should be used to change the isa of existing objects.
    // If this is a new object, use initIsa() for performance.
    Class changeIsa(Class newCls);

    bool hasIndexedIsa();
    bool isTaggedPointer();
    bool isClass();

    // object may have associated objects?
    bool hasAssociatedObjects();
    void setHasAssociatedObjects();

    // object may be weakly referenced?
    bool isWeaklyReferenced();
    void setWeaklyReferenced_nolock();

    // object may have -.cxx_destruct implementation?
    bool hasCxxDtor();  // 对象是否有C++的析构函数

    // Optimized calls to retain/release methods
    id retain();
    void release();
    id autorelease();

    // Implementations of retain/release methods
    id rootRetain();
    bool rootRelease();
    id rootAutorelease();
    bool rootTryRetain();
    bool rootReleaseShouldDealloc();
    uintptr_t rootRetainCount();

    // Implementation of dealloc methods
    bool rootIsDeallocating();
    void clearDeallocating();
    void rootDealloc();

private:
    void initIsa(Class newCls, bool indexed, bool hasCxxDtor);

    // Slow paths for inline control
    id rootAutorelease2();
    bool overrelease_error();

#if SUPPORT_NONPOINTER_ISA
    // Unified retain count manipulation for nonpointer isa
    id rootRetain(bool tryRetain, bool handleOverflow);
    bool rootRelease(bool performDealloc, bool handleUnderflow);
    id rootRetain_overflow(bool tryRetain);
    bool rootRelease_underflow(bool performDealloc);

    void clearDeallocating_slow();

    // Side table retain count overflow for nonpointer isa
    void sidetable_lock();
    void sidetable_unlock();

    void sidetable_moveExtraRC_nolock(size_t extra_rc, bool isDeallocating, bool weaklyReferenced);
    bool sidetable_addExtraRC_nolock(size_t delta_rc);
    size_t sidetable_subExtraRC_nolock(size_t delta_rc);
    size_t sidetable_getExtraRC_nolock();
#endif

    // Side-table-only retain count
    bool sidetable_isDeallocating();
    void sidetable_clearDeallocating();

    bool sidetable_isWeaklyReferenced();
    void sidetable_setWeaklyReferenced_nolock();

    id sidetable_retain();
    id sidetable_retain_slow(SideTable& table);

    uintptr_t sidetable_release(bool performDealloc = true);
    uintptr_t sidetable_release_slow(SideTable& table, bool performDealloc = true);

    bool sidetable_tryRetain();

    uintptr_t sidetable_retainCount();
#if DEBUG
    bool sidetable_present();
#endif
};
//struct objc_object 定义结束

#if __OBJC2__
typedef struct method_t *Method;
typedef struct ivar_t *Ivar;
typedef struct category_t *Category;
typedef struct property_t *objc_property_t;
#else
typedef struct old_method *Method;
typedef struct old_ivar *Ivar;
typedef struct old_category *Category;
typedef struct old_property *objc_property_t;
#endif

// Public headers

#include "objc.h"
#include "runtime.h"
#include "objc-os.h"
#include "objc-abi.h"
#include "objc-api.h"
#include "objc-config.h"
#include "objc-internal.h"
#include "maptable.h"
#include "hashtable2.h"

#if SUPPORT_GC
#include "objc-auto.h"
#endif

/* Do not include message.h here. */
/* #include "message.h" */

#define __APPLE_API_PRIVATE
#include "objc-gdb.h"
#undef __APPLE_API_PRIVATE


// Private headers

#if __OBJC2__
#include "objc-runtime-new.h"
#else
#include "objc-runtime-old.h"
#endif

#include "objc-references.h"
#include "objc-initialize.h"
#include "objc-loadmethod.h"


#if SUPPORT_PREOPT  &&  __cplusplus
#include <objc-shared-cache.h>
using objc_selopt_t = const objc_opt::objc_selopt_t;
#else
struct objc_selopt_t;
#endif


__BEGIN_DECLS


#if (defined(OBJC_NO_GC) && SUPPORT_GC)  ||  \
    (!defined(OBJC_NO_GC) && !SUPPORT_GC)
#   error OBJC_NO_GC and SUPPORT_GC inconsistent
#endif

#if SUPPORT_GC
#   include <auto_zone.h>
    // PRIVATE_EXTERN is needed to help the compiler know "how" extern these are
    PRIVATE_EXTERN extern int8_t UseGC;          // equivalent to calling objc_collecting_enabled()
    PRIVATE_EXTERN extern auto_zone_t *gc_zone;  // the GC zone, or NULL if no GC
    extern void objc_addRegisteredClass(Class c);
    extern void objc_removeRegisteredClass(Class c);
#else
#   define UseGC NO
#   define gc_zone NULL
#   define objc_addRegisteredClass(c) do {} while(0)
#   define objc_removeRegisteredClass(c) do {} while(0)
    /* Uses of the following must be protected with UseGC. */
    extern id gc_unsupported_dont_call();
#   define auto_zone_allocate_object gc_unsupported_dont_call
#   define auto_zone_retain gc_unsupported_dont_call
#   define auto_zone_release gc_unsupported_dont_call
#   define auto_zone_is_valid_pointer gc_unsupported_dont_call
#   define auto_zone_write_barrier_memmove gc_unsupported_dont_call
#   define AUTO_OBJECT_SCANNED 0
#endif


#define _objcHeaderIsReplacement(h)  ((h)->info  &&  ((h)->info->flags & OBJC_IMAGE_IS_REPLACEMENT))

/* OBJC_IMAGE_IS_REPLACEMENT:
   Don't load any classes
   Don't load any categories
   Do fix up selector refs (@selector points to them)
   Do fix up class refs (@class and objc_msgSend points to them)
   Do fix up protocols (@protocol points to them)
   Do fix up superclass pointers in classes ([super ...] points to them)
   Future: do load new classes?
   Future: do load new categories?
   Future: do insert new methods on existing classes?
   Future: do insert new methods on existing categories?
*/

#define _objcInfoSupportsGC(info) (((info)->flags & OBJC_IMAGE_SUPPORTS_GC) ? 1 : 0)
#define _objcInfoRequiresGC(info) (((info)->flags & OBJC_IMAGE_REQUIRES_GC) ? 1 : 0)
#define _objcHeaderSupportsGC(h) ((h)->info && _objcInfoSupportsGC((h)->info))
#define _objcHeaderRequiresGC(h) ((h)->info && _objcInfoRequiresGC((h)->info))

/* OBJC_IMAGE_SUPPORTS_GC:
    was compiled with -fobjc-gc flag, regardless of whether write-barriers were issued
    if executable image compiled this way, then all subsequent libraries etc. must also be this way
*/

#define _objcHeaderOptimizedByDyld(h)  ((h)->info  &&  ((h)->info->flags & OBJC_IMAGE_OPTIMIZED_BY_DYLD))

/* OBJC_IMAGE_OPTIMIZED_BY_DYLD:
   Assorted metadata precooked in the dyld shared cache.
   Never set for images outside the shared cache file itself.
*/

// Mach-O 格式全称为 Mach Object 文件格式的缩写，是 macOS/iOS 上可执行文件的格式
// Mach-O 可以分为3个部分
//     Header
//     Segment
//     Section
// 而一个 Segment 是可以包含多个 Section 的，见配图 mach_o_segments.gif

// 镜像应该指的就是可执行文件

typedef struct header_info {
    
    
    struct header_info *next;//链表
    
    /* typedef struct mach_header_64 headerType;
     * mach_header_64 的注释：The 64-bit mach header appears at the very beginning of the object file for 62-bit architectures.
     （at the very beginning of  在……一开始的时候)
     */
    
    const headerType *mhdr;
    const objc_image_info *info;
    const char *fname;  // same as Dl_info.dli_fname
    bool loaded;
    bool inSharedCache;
    bool allClassesRealized;

    // Do not add fields without editing ObjCModernAbstraction.hpp
    //添加文件就需要编辑修改ObjCModernAbstraction.hpp文件

    bool isLoaded() {
        return loaded;
    }

    bool isBundle() {
        return mhdr->filetype == MH_BUNDLE;
    }

    bool isPreoptimized() const;

#if !__OBJC2__ //非 Object-C 2.0
    struct old_protocol **proto_refs;
    struct objc_module *mod_ptr;
    size_t              mod_count;
# if TARGET_OS_WIN32
    struct objc_module **modules;
    size_t moduleCount;
    struct old_protocol **protocols;
    size_t protocolCount;
    void *imageinfo;
    size_t imageinfoBytes;
    SEL *selrefs;
    size_t selrefCount;
    struct objc_class **clsrefs;
    size_t clsrefCount;    
    TCHAR *moduleName;
# endif
#endif
} header_info;

extern header_info *FirstHeader;
extern header_info *LastHeader;
extern int HeaderCount;

extern void appendHeader(header_info *hi);
extern void removeHeader(header_info *hi);

extern objc_image_info *_getObjcImageInfo(const headerType *head, size_t *size);
extern bool _hasObjcContents(const header_info *hi);


/* selectors */
extern void sel_init(bool gc, size_t selrefCount);
extern SEL sel_registerNameNoLock(const char *str, bool copy);
extern void sel_lock(void);
extern void sel_unlock(void);

extern SEL SEL_load;
extern SEL SEL_initialize;
extern SEL SEL_resolveClassMethod;
extern SEL SEL_resolveInstanceMethod;
extern SEL SEL_cxx_construct;
extern SEL SEL_cxx_destruct;
extern SEL SEL_retain;
extern SEL SEL_release;
extern SEL SEL_autorelease;
extern SEL SEL_retainCount;
extern SEL SEL_alloc;
extern SEL SEL_allocWithZone;
extern SEL SEL_dealloc;
extern SEL SEL_copy;
extern SEL SEL_new;
extern SEL SEL_finalize;
extern SEL SEL_forwardInvocation;
extern SEL SEL_tryRetain;
extern SEL SEL_isDeallocating;
extern SEL SEL_retainWeakReference;
extern SEL SEL_allowsWeakReference;

/* preoptimization */ //预先 优化
extern void preopt_init(void);
extern void disableSharedCacheOptimizations(void);
extern bool isPreoptimized(void);
extern header_info *preoptimizedHinfoForHeader(const headerType *mhdr);

extern objc_selopt_t *preoptimizedSelectors(void);

extern Protocol *getPreoptimizedProtocol(const char *name);

extern Class getPreoptimizedClass(const char *name);
extern Class* copyPreoptimizedClasses(const char *name, int *outCount);

extern Class _calloc_class(size_t size);

/* method lookup */  //方法 查找

// 如果没有找到的话，返回nil，而不是 _objc_msgForward_impcache，即不会进行消息转发
// 实现在 objc-runtime-new.mm 文件中
extern IMP lookUpImpOrNil(Class, SEL, id obj, bool initialize, bool cache, bool resolver);     //查询实现或者nil

/***********************************************************************
 * lookUpImpOrForward.
 * The standard IMP lookup.
 * initialize==NO tries to avoid +initialize (but sometimes fails)
 * cache==NO skips optimistic（开放式）unlocked lookup (but uses cache elsewhere)
 * Most callers should use initialize==YES and cache==YES.
 * inst（也就是 obj） is an instance of cls or a subclass thereof, or nil if none is known.
 *   If cls is an un-initialized metaclass then a non-nil inst is faster.
 * May return _objc_msgForward_impcache. IMPs destined for external use
 *   must be converted to _objc_msgForward or _objc_msgForward_stret.
 *   If you don't want forwarding at all, use lookUpImpOrNil() instead.
 **********************************************************************/
// 标准的查找 IMP 的函数
// 在 cls 类以及父类中寻找 sel 对应的 IMP，
// initialize == NO 表示尝试避免触发 +initialize (但有时失败)，
// cache == NO 表示跳过 optimistic unlocked lookup，即跳过前面不加锁的部分对缓存的查找，但是在 retry 里加锁的部分还是会优先查找缓存
// 大多数调用者应该用 initialize==YES and cache==YES.
// inst 是这个类的实例，或者它的子类的实例，也可能是 nil，
// 如果这个类是一个不是 initialized 状态的元类，那么 obj 非空的话，会快一点，
// resolver == YES 的话，如果在缓存和方法列表中都没有找到 IMP，就会进行 resolve，尝试动态添加方法
// 有可能返回 _objc_msgForward_impcache。IMPs 被用作外部的使用时（转发？？），一定要转为 _objc_msgForward 或者 _objc_msgForward_stret
// 如果确实不想转发，就用 lookUpImpOrNil() 代替
// 实现在 objc-runtime-new.mm 文件中
extern IMP lookUpImpOrForward(Class, SEL, id obj, bool initialize, bool cache, bool resolver); //查询实现或者转发


// 在指定 Class 中搜索 sel 对应的 IMP，先在缓存中找，
// 如果没有找到，再在 method list 里找，如果找到，就将 sel-IMP 对放入缓存中，并返回 IMP
// 如果没有找到，就将 sel-_objc_msgForward_impcache 放入缓存中，
// 并返回_objc_msgForward_impcache
// 但是这个函数只用于 object_cxxConstructFromClass() 和 object_cxxDestructFromClass() 两个函数
// 实现在 objc-runtime-new.mm 文件中
extern IMP lookupMethodInClassAndLoadCache(Class cls, SEL sel);         //在类和加载缓存中查询方法实现

// inst is an instance of cls or a subclass thereof, or nil if none is known.
// Non-nil inst is faster in some cases. See lookUpImpOrForward() for details.
extern bool class_respondsToSelector_inst(Class cls, SEL sel, id inst); //

extern bool objcMsgLogEnabled;
extern bool logMessageSend(bool isClassMethod,
                    const char *objectsClass,
                    const char *implementingClass,
                    SEL selector);

/* message dispatcher */ //消息 分发派遣 还有消息转发呢


/***********************************************************************
 * _class_lookupMethodAndLoadCache.
 * Method lookup for dispatchers ONLY. OTHER CODE SHOULD USE lookUpImp().
 * This lookup avoids optimistic cache scan because the dispatcher
 * already tried that.
 
 这个查找方法的函数只能被 dispatchers （也就是 objc_msgSend、objc_msgSend_stret 等函数）使用
 其他的代码应该使用 lookUpImp() 函数
 这个函数避免了扫描缓存，因为 dispatchers 已经尝试过扫描缓存了，正是因为缓存中没有找到，才调用这个方法找的
 **********************************************************************/
// 该方法会在 objc_msgSend 中，当在缓存中没有找到 sel 对应的 IMP 时被调用
// objc-msg-arm.s 文件中 STATIC_ENTRY _objc_msgSend_uncached 里可以找到
// 因为在调用这个方法之前，我们已经是从缓存无法找到这个方法了，所以这个方法避免了再去扫描缓存查找方法的过程，而是直接从方法列表找起。
extern IMP _class_lookupMethodAndLoadCache3(id, SEL, Class);



#if !OBJC_OLD_DISPATCH_PROTOTYPES
// 实现代码都在 objc-msg-arm.s 中

extern void _objc_msgForward_impcache(void);       //消息转发
extern void _objc_ignored_method(void);            //消息忽略
extern void _objc_msgSend_uncached_impcache(void); //消息发送 不缓存
#else
extern id _objc_msgForward_impcache(id, SEL, ...);
extern id _objc_ignored_method(id, SEL, ...);
extern id _objc_msgSend_uncached_impcache(id, SEL, ...);
#endif

/* errors */
extern void __objc_error(id, const char *, ...) __attribute__((format (printf, 2, 3), noreturn));
extern void _objc_inform(const char *fmt, ...) __attribute__((format (printf, 1, 2)));
extern void _objc_inform_on_crash(const char *fmt, ...) __attribute__((format (printf, 1, 2)));
extern void _objc_inform_now_and_on_crash(const char *fmt, ...) __attribute__((format (printf, 1, 2)));
extern void _objc_inform_deprecated(const char *oldname, const char *newname) __attribute__((noinline));
extern void inform_duplicate(const char *name, Class oldCls, Class cls);
extern bool crashlog_header_name(header_info *hi);
extern bool crashlog_header_name_string(const char *name);

/* magic */
/***********************************************************************
 * _objc_getFreedObjectClass.  Return a pointer to the dummy（假的） freed（释放的）
 * object class.  Freed objects get their isa pointers replaced with
 * a pointer to the freedObjectClass, so that we can catch usages of
 * the freed object.（没有看明白）
 **********************************************************************/
extern Class _objc_getFreedObjectClass (void);

/* map table additions */
extern void *NXMapKeyCopyingInsert(NXMapTable *table, const void *key, const void *value);
extern void *NXMapKeyFreeingRemove(NXMapTable *table, const void *key);

/* hash table additions */
extern unsigned _NXHashCapacity(NXHashTable *table);
extern void _NXHashRehashToCapacity(NXHashTable *table, unsigned newCapacity);

/* property attribute parsing */ //成员变量属性 解析
extern const char *copyPropertyAttributeString(const objc_property_attribute_t *attrs, unsigned int count);
extern objc_property_attribute_t *copyPropertyAttributeList(const char *attrs, unsigned int *outCount);
extern char *copyPropertyAttributeValue(const char *attrs, const char *name);

/* locking */  //锁
extern void lock_init(void);
extern rwlock_t selLock;
extern mutex_t cacheUpdateLock;   // 用户方法缓存更新时的互斥锁
extern recursive_mutex_t loadMethodLock;
#if __OBJC2__
extern rwlock_t runtimeLock;
#else
extern mutex_t classLock;
extern mutex_t methodListLock;
#endif


// 用这个类包装 monitor_t 类的对象的意义是，在构造函数中会自动调用 lock.enter() 给互斥量加锁
// 而在析构时，会自动调用 lock.leave() 给互斥量解锁
// 就省了每次都要小心翼翼的检查 lock.leave()，如果忘了写，会很麻烦，而且 BUG 不好排查
// 另一点更保证了安全，就是 monitor_locker_t 继承自 nocopy_t 类
// 父类 nocopy_t 是不能被拷贝的类，因为没有拷贝构造函数
// 所以，monitor_t 锁被封装在 monitor_locker_t 中非常安全

//monitor_t 锁被封装在 monitor_locker_t  monitor_locker_t 是不可复制的
class monitor_locker_t : nocopy_t {
    monitor_t& lock;
  public:
    monitor_locker_t(monitor_t& newLock) : lock(newLock) { lock.enter(); }
    ~monitor_locker_t() { lock.leave(); }  //析构函数？？
};

class mutex_locker_t : nocopy_t {
    mutex_t& lock;
  public:
    mutex_locker_t(mutex_t& newLock) 
        : lock(newLock) { lock.lock(); }
    ~mutex_locker_t() { lock.unlock(); }
};

//递归锁 可以递归上锁 不会引起思索 上了锁 之后又上一层锁
class recursive_mutex_locker_t : nocopy_t {
    recursive_mutex_t& lock;
  public:
    recursive_mutex_locker_t(recursive_mutex_t& newLock) 
        : lock(newLock) { lock.lock(); }
    ~recursive_mutex_locker_t() { lock.unlock(); }
};

//读写锁 读锁
class rwlock_reader_t : nocopy_t {
    rwlock_t& lock;
  public:
    rwlock_reader_t(rwlock_t& newLock) : lock(newLock) { lock.read(); }
    ~rwlock_reader_t() { lock.unlockRead(); }
};

//读写锁  写锁
class rwlock_writer_t : nocopy_t {
    rwlock_t& lock;
  public:
    rwlock_writer_t(rwlock_t& newLock) : lock(newLock) { lock.write(); }
    ~rwlock_writer_t() { lock.unlockWrite(); }
};

/* ignored selector support */

/* Non-GC: no ignored selectors
   GC (i386 Mac): some selectors ignored, remapped to kIgnore
   GC (others): some selectors ignored, but not remapped 
*/

static inline int ignoreSelector(SEL sel)
{
#if !SUPPORT_GC
    return NO;
#elif SUPPORT_IGNORED_SELECTOR_CONSTANT
    return UseGC  &&  sel == (SEL)kIgnore;
#else
    return UseGC  &&  
        (sel == @selector(retain)       ||  
         sel == @selector(release)      ||  
         sel == @selector(autorelease)  ||  
         sel == @selector(retainCount)  ||  
         sel == @selector(dealloc));
#endif
}

static inline int ignoreSelectorNamed(const char *sel)
{
#if !SUPPORT_GC
    return NO;
#else
    // release retain retainCount dealloc autorelease
    return (UseGC &&
            (  (sel[0] == 'r' && sel[1] == 'e' &&
                (strcmp(&sel[2], "lease") == 0 || 
                 strcmp(&sel[2], "tain") == 0 ||
                 strcmp(&sel[2], "tainCount") == 0 ))
               ||
               (strcmp(sel, "dealloc") == 0)
               || 
               (sel[0] == 'a' && sel[1] == 'u' && 
                strcmp(&sel[2], "torelease") == 0)));
#endif
}

/* GC startup */
extern void gc_init(bool wantsGC);
extern void gc_init2(void);

/* Exceptions */
struct alt_handler_list;
extern void exception_init(void);
extern void _destroyAltHandlerList(struct alt_handler_list *list);

/* Class change notifications (gdb only for now) */
#define OBJC_CLASS_ADDED (1<<0)
#define OBJC_CLASS_REMOVED (1<<1)
#define OBJC_CLASS_IVARS_CHANGED (1<<2)
#define OBJC_CLASS_METHODS_CHANGED (1<<3)
extern void gdb_objc_class_changed(Class cls, unsigned long changes, const char *classname)
    __attribute__((noinline));

#if SUPPORT_GC

/* Write barrier implementations */
extern id objc_getAssociatedObject_non_gc(id object, const void *key);
extern void objc_setAssociatedObject_non_gc(id object, const void *key, id value, objc_AssociationPolicy policy);

extern id objc_getAssociatedObject_gc(id object, const void *key);
extern void objc_setAssociatedObject_gc(id object, const void *key, id value, objc_AssociationPolicy policy);

/* xrefs */
extern objc_xref_t _object_addExternalReference_non_gc(id obj, objc_xref_t type);
extern id _object_readExternalReference_non_gc(objc_xref_t ref);
extern void _object_removeExternalReference_non_gc(objc_xref_t ref);

extern objc_xref_t _object_addExternalReference_gc(id obj, objc_xref_t type);
extern id _object_readExternalReference_gc(objc_xref_t ref);
extern void _object_removeExternalReference_gc(objc_xref_t ref);

/* GC weak reference fixup. */
extern void gc_fixup_weakreferences(id newObject, id oldObject);

/* GC datasegment registration. */
extern void gc_register_datasegment(uintptr_t base, size_t size);
extern void gc_unregister_datasegment(uintptr_t base, size_t size);

/* objc_dumpHeap implementation */
extern bool _objc_dumpHeap(auto_zone_t *zone, const char *filename);

#endif


// Settings from environment variables
#define OPTION(var, env, help) extern bool var;
#include "objc-env.h"
#undef OPTION

extern void environ_init(void);

extern void logReplacedMethod(const char *className, SEL s, bool isMeta, const char *catName, IMP oldImp, IMP newImp);


// objc per-thread storage
// 线程数据 结构体 _objc_pthread_data 的定义

typedef struct {
    struct _objc_initializing_classes *initializingClasses; // for +initialize
    struct SyncCache *syncCache;  // for @synchronize
    struct alt_handler_list *handlerList;  // for exception alt handlers
    char *printableNames[4];  // temporary demangled names for logging
    // 数组，存储需要打印的类取消重整的名字，
    // 是一个 FIFO 的队列，有新的元素进来时，会将第一个元素释放，然后后面的元素向前挪一个单位，
    // 再把新来的元素放在末尾


    // If you add new fields here, don't forget to update
    // _objc_pthread_destroyspecific()

} _objc_pthread_data;

extern _objc_pthread_data *_objc_fetch_pthread_data(bool create);
extern void tls_init(void);

// encoding.h
extern unsigned int encoding_getNumberOfArguments(const char *typedesc);
extern unsigned int encoding_getSizeOfArguments(const char *typedesc);
extern unsigned int encoding_getArgumentInfo(const char *typedesc, unsigned int arg, const char **type, int *offset);
extern void encoding_getReturnType(const char *t, char *dst, size_t dst_len);
extern char * encoding_copyReturnType(const char *t);
extern void encoding_getArgumentType(const char *t, unsigned int index, char *dst, size_t dst_len);
extern char *encoding_copyArgumentType(const char *t, unsigned int index);

// sync.h
extern void _destroySyncCache(struct SyncCache *cache);

// arr
extern void arr_init(void);
extern id objc_autoreleaseReturnValue(id obj);

// block trampolines(蹦床 意译)
extern IMP _imp_implementationWithBlockNoCopy(id block);

// layout.h
typedef struct {
    uint8_t *bits;
    size_t bitCount;
    size_t bitsAllocated;
    bool weak;
} layout_bitmap;
extern layout_bitmap layout_bitmap_create(const unsigned char *layout_string, size_t layoutStringInstanceSize, size_t instanceSize, bool weak);
extern layout_bitmap layout_bitmap_create_empty(size_t instanceSize, bool weak);
extern void layout_bitmap_free(layout_bitmap bits);
extern const unsigned char *layout_string_create(layout_bitmap bits);
extern void layout_bitmap_set_ivar(layout_bitmap bits, const char *type, size_t offset);
extern void layout_bitmap_grow(layout_bitmap *bits, size_t newCount);
extern void layout_bitmap_slide(layout_bitmap *bits, size_t oldPos, size_t newPos);
extern void layout_bitmap_slide_anywhere(layout_bitmap *bits, size_t oldPos, size_t newPos);
extern bool layout_bitmap_splat(layout_bitmap dst, layout_bitmap src, 
                                size_t oldSrcInstanceSize);
extern bool layout_bitmap_or(layout_bitmap dst, layout_bitmap src, const char *msg);
extern bool layout_bitmap_clear(layout_bitmap dst, layout_bitmap src, const char *msg);
extern void layout_bitmap_print(layout_bitmap bits);


// fixme runtime
extern Class look_up_class(const char *aClassName, bool includeUnconnected, bool includeClassHandler);
extern "C" const char *map_2_images(enum dyld_image_states state, uint32_t infoCount, const struct dyld_image_info infoList[]);
extern const char *map_images_nolock(enum dyld_image_states state, uint32_t infoCount, const struct dyld_image_info infoList[]);
extern const char * load_images(enum dyld_image_states state, uint32_t infoCount, const struct dyld_image_info infoList[]);
extern bool load_images_nolock(enum dyld_image_states state, uint32_t infoCount, const struct dyld_image_info infoList[]);
extern void unmap_image(const struct mach_header *mh, intptr_t vmaddr_slide);
extern void unmap_image_nolock(const struct mach_header *mh);
extern void _read_images(header_info **hList, uint32_t hCount);
extern void prepare_load_methods(const headerType *mhdr);
extern bool hasLoadMethods(const headerType *mhdr);
extern void _unload_image(header_info *hi);
extern const char ** _objc_copyClassNamesForImage(header_info *hi, unsigned int *outCount);


extern const header_info *_headerForClass(Class cls);

extern Class _class_remap(Class cls);
extern Class _class_getNonMetaClass(Class cls, id obj);
extern Ivar _class_getVariable(Class cls, const char *name, Class *memberOf);
extern uint32_t _class_getInstanceStart(Class cls);

extern unsigned _class_createInstancesFromZone(Class cls, size_t extraBytes, void *zone, id *results, unsigned num_requested);
extern id _objc_constructOrFree(id bytes, Class cls);

extern const char *_category_getName(Category cat);
extern const char *_category_getClassName(Category cat);
extern Class _category_getClass(Category cat);
extern IMP _category_getLoadMethod(Category cat);

extern id object_cxxConstructFromClass(id obj, Class cls);
extern void object_cxxDestruct(id obj);

extern void _class_resolveMethod(Class cls, SEL sel, id inst);

#define OBJC_WARN_DEPRECATED \
    do { \
        static int warned = 0; \
        if (!warned) { \
            warned = 1; \
            _objc_inform_deprecated(__FUNCTION__, NULL); \
        } \
    } while (0) \

__END_DECLS


#ifndef STATIC_ASSERT
#   define STATIC_ASSERT(x) _STATIC_ASSERT2(x, __LINE__)
#   define _STATIC_ASSERT2(x, line) _STATIC_ASSERT3(x, line)
#   define _STATIC_ASSERT3(x, line)                                     \
        typedef struct {                                                \
            int _static_assert[(x) ? 0 : -1];                           \
        } _static_assert_ ## line __attribute__((unavailable)) 
#endif

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))


static __inline uint32_t _objc_strhash(const char *s) {
    uint32_t hash = 0;
    for (;;) {
	int a = *s++;
	if (0 == a) break;
	hash += (hash << 8) + a;
    }
    return hash;
}

#if __cplusplus

// 计算 x 的 log2(x)，并取下限
/*
 x       x  log2(x)
 0 -     0 - 0
 1 -     1 - 0
 2 -    10 - 1
 3 -    11 - 1
 4 -   100 - 2
 5 -   101 - 2
 6 -   110 - 2
 7 -   111 - 2
 8 -  1000 - 3
 9 -  1001 - 3
 10 -  1010 - 3
 11 -  1011 - 3
 12 -  1100 - 3
 13 -  1101 - 3
 14 -  1110 - 3
 15 -  1111 - 3
 16 - 10000 - 4
 17 - 10001 - 4
 18 - 10010 - 4
 19 - 10011 - 4
 */

//用这个干什么呢？？？

//用递归的方法实现计算log2(x)并且取下限，就是看 x 有多少位二进制嘛 ， 那就每次向右移动一位
template <typename T>
static inline T log2u(T x) {
    return (x<2) ? 0 : log2u(x>>1)+1;
}
//这个是求2的x次方是多少，那就每次左移一位
template <typename T>
static inline T exp2u(T x) {
    return (1 << x);
}
//这个是求2的x次方- 1 是多少
template <typename T>
static T exp2m1u(T x) { 
    return (1 << x) - 1; 
}

#endif

//全局的操作 new 和 删除 一定要确保 在任何一个APP中不能重写这些方法 并且要求 任何一个必须存在lib库？？？？
// Global operator new and delete. We must not use any app overrides.
// This ALSO REQUIRES each of these be in libobjc's unexported symbol list.
#if __cplusplus
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winline-new-delete"
#include <new>
inline void* operator new(std::size_t size) throw (std::bad_alloc) { return malloc(size); }
inline void* operator new[](std::size_t size) throw (std::bad_alloc) { return malloc(size); }
inline void* operator new(std::size_t size, const std::nothrow_t&) throw() { return malloc(size); }
inline void* operator new[](std::size_t size, const std::nothrow_t&) throw() { return malloc(size); }
inline void operator delete(void* p) throw() { free(p); }
inline void operator delete[](void* p) throw() { free(p); }
inline void operator delete(void* p, const std::nothrow_t&) throw() { free(p); }
inline void operator delete[](void* p, const std::nothrow_t&) throw() { free(p); }
#pragma clang diagnostic pop
#endif


class TimeLogger {
    uint64_t mStart;
    bool mRecord;
 public:
    TimeLogger(bool record = true) 
     : mStart(nanoseconds())
     , mRecord(record) 
    { }

    void log(const char *msg) {
        if (mRecord) {
            uint64_t end = nanoseconds();
            _objc_inform("%.2f ms: %s", (end - mStart) / 1000000.0, msg);
            mStart = nanoseconds();
        }
    }
};


// StripedMap<T> is a map of void* -> T, sized appropriately 
// for cache-friendly lock striping. 
// For example, this may be used as StripedMap<spinlock_t>
// or as StripedMap<SomeStruct> where SomeStruct stores a spin lock.


// 条纹图
// 为什么叫条纹图呢，因为内存被分割成了一条一条的
// 就像这样 | 64bits | 64bits | 64bits | 64bits | 64bits | 64bits | 64bits | 64bits | ....
// 一共有 64 块，每块的大小是 64 的整数倍

template<typename T>  //泛型
class StripedMap {

    enum { CacheLineSize = 64 };

#if TARGET_OS_EMBEDDED //嵌入
    enum { StripeCount = 8 };
#else
    enum { StripeCount = 64 };
#endif

    struct PaddedT {
        
        // 一个名称为value的变量，类型是T，并且内存以 CacheLineSize 对齐
        // 即占用内存大小是 CacheLineSize 的整数倍
        T value alignas(CacheLineSize);
    };

    // 最重要的存数据的数组，元素个数是StripeCount
    PaddedT array[StripeCount];

    static unsigned int indexForPointer(const void *p) {
        
        // 根据指针 p 存的对象的地址，计算对象在哪个 side table
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return ((addr >> 4) ^ (addr >> 9)) % StripeCount;
    }

 public:
    T& operator[] (const void *p) { 
        return array[indexForPointer(p)].value; 
    }
    const T& operator[] (const void *p) const { 
        return const_cast<StripedMap<T>>(this)[p]; 
    }

#if DEBUG
    StripedMap() {
        // Verify alignment expectations.
        uintptr_t base = (uintptr_t)&array[0].value;
        uintptr_t delta = (uintptr_t)&array[1].value - base;
        assert(delta % CacheLineSize == 0);
        assert(base % CacheLineSize == 0);
    }
#endif
};


// DisguisedPtr<T> acts like pointer type T*, except the 
// stored value is disguised to hide it from tools like `leaks`.
// nil is disguised as itself so zero-filled memory works as expected, 
// which means 0x80..00 is also diguised as itself but we don't care

// 将指针伪装成 DisguisedPtr 类型，可以防止 leaks 报内存泄漏

template <typename T>
class DisguisedPtr {
    uintptr_t value;

    static uintptr_t disguise(T* ptr) {
        return -(uintptr_t)ptr;
    }

    static T* undisguise(uintptr_t val) {
        return (T*)-val;
    }

 public:
    DisguisedPtr() { }
    DisguisedPtr(T* ptr) 
        : value(disguise(ptr)) { }
    DisguisedPtr(const DisguisedPtr<T>& ptr) 
        : value(ptr.value) { }

    DisguisedPtr<T>& operator = (T* rhs) {
        value = disguise(rhs);
        return *this;
    }
    DisguisedPtr<T>& operator = (const DisguisedPtr<T>& rhs) {
        value = rhs.value;
        return *this;
    }

    operator T* () const {
        return undisguise(value);
    }
    T* operator -> () const { 
        return undisguise(value);
    }
    T& operator * () const { 
        return *undisguise(value);
    }
    T& operator [] (size_t i) const {
        return undisguise(value)[i];
    }

    // pointer arithmetic operators omitted 
    // because we don't currently use them anywhere
};

// fixme type id is weird and not identical to objc_object*
static inline bool operator == (DisguisedPtr<objc_object> lhs, id rhs) {
    return lhs == (objc_object *)rhs;
}
static inline bool operator != (DisguisedPtr<objc_object> lhs, id rhs) {
    return lhs != (objc_object *)rhs;
}


// Pointer hash function.
// This is not a terrific hash, but it is fast 
// and not outrageously flawed for our purposes.

// Based on principles from http://locklessinc.com/articles/fast_hash/
// and evaluation ideas from http://floodyberry.com/noncryptohashzoo/
#if __LP64__
static inline uint32_t ptr_hash(uint64_t key)
{
    key ^= key >> 4;
    key *= 0x8a970be7488fda55;
    key ^= __builtin_bswap64(key);
    return (uint32_t)key;
}
#else
static inline uint32_t ptr_hash(uint32_t key)
{
    key ^= key >> 4;
    key *= 0x5052acdb;
    key ^= __builtin_bswap32(key);
    return key;
}
#endif

/*
  Higher-quality hash function. This is measurably slower in some workloads.
#if __LP64__
 uint32_t ptr_hash(uint64_t key)
{
    key -= __builtin_bswap64(key);
    key *= 0x8a970be7488fda55;
    key ^= __builtin_bswap64(key);
    key *= 0x8a970be7488fda55;
    key ^= __builtin_bswap64(key);
    return (uint32_t)key;
}
#else
static uint32_t ptr_hash(uint32_t key)
{
    key -= __builtin_bswap32(key);
    key *= 0x5052acdb;
    key ^= __builtin_bswap32(key);
    key *= 0x5052acdb;
    key ^= __builtin_bswap32(key);
    return key;
}
#endif
*/


// Inlined parts of objc_object's implementation
#include "objc-object.h"

#endif /* _OBJC_PRIVATE_H_ */

