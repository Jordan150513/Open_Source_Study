/*
 * Copyright (c) 2005-2007 Apple Inc.  All Rights Reserved.
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

#ifndef _OBJC_RUNTIME_NEW_H
#define _OBJC_RUNTIME_NEW_H

//根据(不同位数的)系统，设置mask_t 分别是什么类型 如果是64位 就是uint32_t，也就是unsigned int（无符号整数），
//或者是uint16_t == unsigned short，或者是uintptr_t==unsigned long
#if __LP64__
typedef uint32_t mask_t; // unsigned int  32 位 // x86_64 & arm64 asm are less efficient with 16-bits  //unsigned int == uint32_t
#else
typedef uint16_t mask_t; // unsigned short 16 位
#endif
typedef uintptr_t cache_key_t; // 也就是 unsigned long 64位？？


struct swift_class_t; //向前声明，具体的定义在L1238

#pragma mark - bucket_t
// cache_t 中存的实体，单一的一个 key - value 对
// bucket 可以翻译为 槽

//（槽）bucket_t结构体的定义  在下面cache_t结构体中使用，作为其成员
struct bucket_t {
private:
    cache_key_t _key; //cache_key_t 类型的 _key,也就是unsigned long（无符号长整形的）_key
    IMP _imp;         //IMP指针类型的_imp

public://提供给结构体外界的使用接口，存取结构体中的成员
    inline cache_key_t key() const { return _key; }          //获取本结构体中的cache_key_t（unsigned long）类型的key
    inline IMP imp() const { return (IMP)_imp; }             //获取IMP指针
    inline void setKey(cache_key_t newKey) { _key = newKey; }//设置key
    inline void setImp(IMP newImp) { _imp = newImp; }        //设置IMP

    void set(cache_key_t newKey, IMP newImp);                //IMP和key一起设置
};

//定义结构体 cache_t
struct cache_t {
    struct bucket_t *_buckets;  //一个指向（槽）bucket_t结构体类型的指针
    mask_t _mask;               // 16位或者32位的一个mask
    mask_t _occupied;           //16位或者32位的一个_occupied

public:
    struct bucket_t *buckets();         //获取_buckets _buckets的get方法
    mask_t mask();                      //mask的获取方法
    mask_t occupied();                  //occupied的获取方法
    void incrementOccupied();           //操作occupied自增
    void setBucketsAndMask(struct bucket_t *newBuckets, mask_t newMask);  //同时设置_buckets和_mask
    void initializeToEmpty();           //初始化cache_t本结构体为空的结构体

    mask_t capacity();                  //返回cache_t的容量，返回值是一个mask_t（unsigned int  32 位 或者 unsigned short 16 位）类型的
    bool isConstantEmptyCache();        //判断是否是一个常量空的cache 判断被初次创建之后 有没有被使用过
    bool canBeFreed();                  //判断是否可以被释放掉

    static size_t bytesForCapacity(uint32_t cap);                           //返回容量的字节数
    static struct bucket_t * endMarker(struct bucket_t *b, uint32_t cap);   //获取end marker

    void expand();  //扩充
    void reallocate(mask_t oldCapacity, mask_t newCapacity);    //再次分配空间 重新申请分配空间
    struct bucket_t * find(cache_key_t key, id receiver);        //查找

    static void bad_cache(id receiver, SEL sel, Class isa) __attribute__((noreturn));  //判断是否是坏的cache 缓存
};


// classref_t is unremapped class_t*
typedef struct classref * classref_t;       //classref_t 跟 class_t * 是一模一样的同一个 只不过有两个名字

/***********************************************************************
* entsize_list_tt<Element, List, FlagMask>
* Generic implementation of an array of non-fragile structs.
*
* Element is the struct type (e.g. method_t)
* List is the specialization of entsize_list_tt (e.g. method_list_t)
* FlagMask is used to stash extra bits in the entsize field
*   (e.g. method list fixup markers)
**********************************************************************/
template <typename Element, typename List, uint32_t FlagMask>
struct entsize_list_tt {            //结构体 entsize_list_tt 的定义
    uint32_t entsizeAndFlags;            //unsigned int 类型的 entsizeAndFlags 存储了 entsize（每一个元素占用的字节数） 和 Flags
    uint32_t count;                      //unsigned int 类型的 count
    Element first;                       // Element 类型的第一个元素

    uint32_t entsize() const {             //获取 entsize  每一个元素占用的字节数
        return entsizeAndFlags & ~FlagMask;
    }
    uint32_t flags() const {                //获取 flags
        return entsizeAndFlags & FlagMask;
    }

    Element& getOrEnd(uint32_t i) const {              //获取某个元素 i <= count
        assert(i <= count);
        return *(Element *)((uint8_t *)&first + i*entsize()); 
    }
    Element& get(uint32_t i) const {                    //获取某个元素 i < count
        assert(i < count);
        return getOrEnd(i);
    }

    size_t byteSize() const {
        return sizeof(*this) + (count-1)*entsize();  //获取占用的字节数 ：this指针指向的变量（含有第一个元素）的占用字节数 + （总数量-1）* 每一个元素占用的字节数
    }

    List *duplicate() const {           //重复：返回一个指向链表指针  重新创建 一个 复制其中内容
        return (List *)memdup(this, this->byteSize());     //memdup()的定义在objc-os.h中
    }

    struct iterator;                //枚举器的 向前声明
    const iterator begin() const { 
        return iterator(*static_cast<const List*>(this), 0); 
    }
    iterator begin() { 
        return iterator(*static_cast<const List*>(this), 0); 
    }
    const iterator end() const { 
        return iterator(*static_cast<const List*>(this), count); 
    }
    iterator end() { 
        return iterator(*static_cast<const List*>(this), count); 
    }

    struct iterator {               //枚举器的 定义
        uint32_t entsize;                   //实体元素element 所占用的内存大小 字节数 返回一个 unsigned int
        uint32_t index;                     // keeping track of this saves a divide in operator-
                                            //unsigned int类型的索引  跟踪索引可以在枚举器中有一个分界
        Element* element;                   //实体元素 element 是以指针类型 指向元素类型的一个指针来存储的

        typedef std::random_access_iterator_tag iterator_category;       //定义别名 iterator_category 就是 std
                                                                         //std是在random_access_iterator_tag有效范围内
                                                                        //random_access_iterator_tag 就是  public bidirectional_iterator_tag {}
        typedef Element value_type;                  //定义别名
        typedef ptrdiff_t difference_type;           //定义别名  索引的差值 delta 的类型
        typedef Element* pointer;                   //定义别名  pointer 指针 就是指向元素类型的指针
        typedef Element& reference;                 //定义别名  reference  就是元素类型的引用 引用类型跟元素的变量名是一模一样的
                                                    //---int &j=i,此时i和j代表同一个对象，改变j就是改变i.
        
                            // & :表示引用，就是对象的另一个名字， （传值的时候的引用传值）
                            //例如 int i = 10; int &j=i,此时i和j代表同一个对象，改变j就是改变i.
                            //但是不能这样 int &m = 10;，因为引用的要是一个对象。
                            //但是加上const 时 ，引用可以初始化为不同类型的对象或者初始化为右值，例如 可以写成 const int &n=12;

        iterator() { }      //默认构造函数

        iterator(const List& list, uint32_t start = 0)      //带参数的构造函数 List& list 说明list是引用类型，改变list就是改变之前的变量
            : entsize(list.entsize())                   //初始化实体类型大小size
            , index(start)                              //初始化index
            , element(&list.getOrEnd(start))            //初始化 element 指针
        { }

        const iterator& operator += (ptrdiff_t delta) {   //重构 += 操作符返回的是一个 枚举器的引用类型
            element = (Element*)((uint8_t *)element + delta*entsize);  //说明是按照数组的形式存储的 元素的地址都存在在连续的地址空间，可以直接加间距值去取数据
            index += (int32_t)delta;
            return *this;
        }
        const iterator& operator -= (ptrdiff_t delta) {
            element = (Element*)((uint8_t *)element - delta*entsize);
            index -= (int32_t)delta;
            return *this;
        }
        const iterator operator + (ptrdiff_t delta) const {
            return iterator(*this) += delta;
        }
        const iterator operator - (ptrdiff_t delta) const {
            return iterator(*this) -= delta;
        }

        iterator& operator ++ () { *this += 1; return *this; }
        iterator& operator -- () { *this -= 1; return *this; }
        iterator operator ++ (int) {
            iterator result(*this); *this += 1; return result;
        }
        iterator operator -- (int) {
            iterator result(*this); *this -= 1; return result;
        }

        ptrdiff_t operator - (const iterator& rhs) const {
            return (ptrdiff_t)this->index - (ptrdiff_t)rhs.index;
        }

        Element& operator * () const { return *element; }  //返回的是引用的类型
        Element* operator -> () const { return element; }   //返回的是指针的类型

        operator Element& () const { return *element; }

        bool operator == (const iterator& rhs) const {
            return this->element == rhs.element;
        }
        bool operator != (const iterator& rhs) const {
            return this->element != rhs.element;
        }

        bool operator < (const iterator& rhs) const {
            return this->element < rhs.element;
        }
        bool operator > (const iterator& rhs) const {
            return this->element > rhs.element;
        }
    };
};

// method_t 结构体的定义 代表 方法的实体
struct method_t {
    SEL name;            //selector 的name
    const char *types;   // types？？----XH版本解释--方法类型字符串，有的地方又称 method signature 方法签名
    IMP imp;             //方法的实现指针

    struct SortBySELAddress :
        public std::binary_function<const method_t&,
                                    const method_t&, bool>
    {
        bool operator() (const method_t& lhs,
                         const method_t& rhs)
        { return lhs.name < rhs.name; }
    };
};

//成员变量结构体 ivar_t 定义
struct ivar_t {
#if __x86_64__
    // *offset was originally 64-bit on some x86_64 platforms.
    // We read and write only 32 bits of it.
    // Some metadata provides all 64 bits. This is harmless for unsigned 
    // little-endian values.
    // Some code uses all 64 bits. class_addIvar() over-allocates the 
    // offset for their benefit.
#endif
    int32_t *offset;    //偏移量
    const char *name;   //成员变量的name
    const char *type;   //成员变量的类型
    // alignment is sometimes -1; use alignment() instead
    uint32_t alignment_raw;     //是否是线性对其的标志
    uint32_t size;              //size 尺寸大小

    uint32_t alignment() const {    //线性对齐的方法
        if (alignment_raw == ~(uint32_t)0) return 1U << WORD_SHIFT;
        return 1 << alignment_raw;
    }
};

//属性的结构体定义
struct property_t {
    const char *name;
    const char *attributes;
};

// Two bits of entsize are used for fixup markers.
// entsize 字段中有两位被用来 标记修订 markers
struct method_list_t : entsize_list_tt<method_t, method_list_t, 0x3> {
    bool isFixedUp() const;
    void setFixedUp();

    uint32_t indexOfMethod(const method_t *meth) const {
        uint32_t i = 
            (uint32_t)(((uintptr_t)meth - (uintptr_t)this) / entsize());
        assert(i < count);
        return i;
    }
};

struct ivar_list_t : entsize_list_tt<ivar_t, ivar_list_t, 0> {
};

struct property_list_t : entsize_list_tt<property_t, property_list_t, 0> {
};


typedef uintptr_t protocol_ref_t;  // protocol_t *, but unremapped

// Values for protocol_t->flags
#define PROTOCOL_FIXED_UP_2 (1<<31)  // must never be set by compiler
#define PROTOCOL_FIXED_UP_1 (1<<30)  // must never be set by compiler

#define PROTOCOL_FIXED_UP_MASK (PROTOCOL_FIXED_UP_1 | PROTOCOL_FIXED_UP_2)

// 协议 protocol_t 的结构定义 一个协议 一个协议可以有多个协议方法 协议方法也分为类方法和实例方法
struct protocol_t : objc_object {
    const char *mangledName;            //重整的名字
    struct protocol_list_t *protocols;  //指向一个协议列表的指针
    method_list_t *instanceMethods;     //协议中 实例方法 的指针
    method_list_t *classMethods;        //协议中 类方法列表 的指针
    method_list_t *optionalInstanceMethods;         // 可选实例方法列表 的指针
    method_list_t *optionalClassMethods;            // 可选类方法列表 的指针
    property_list_t *instanceProperties;
    uint32_t size;   // sizeof(protocol_t)
    uint32_t flags;
    // Fields below this point are not always present on disk.
    const char **extendedMethodTypes;
    const char *_demangledName;

    const char *demangledName();

    const char *nameForLogging() {
        return demangledName();
    }

    bool isFixedUp() const;
    void setFixedUp();

    bool hasExtendedMethodTypesField() const {
        return size >= (offsetof(protocol_t, extendedMethodTypes) 
                        + sizeof(extendedMethodTypes));
    }
    bool hasExtendedMethodTypes() const {
        return hasExtendedMethodTypesField() && extendedMethodTypes;
    }
};

//协议列表 结构 的定义 是一个列表 里面存的是多个协议 一个类遵守多个协议
struct protocol_list_t {
    //协议列表 没有继承自 枚举器 其他的方法列表 成员属性列表都继承自了一个枚举器
    
    // count is 64-bit by accident. 
    uintptr_t count;
    protocol_ref_t list[0]; // variable-size

    size_t byteSize() const {
        return sizeof(*this) + count*sizeof(list[0]);
    }

    protocol_list_t *duplicate() const {
        return (protocol_list_t *)memdup(this, this->byteSize());
    }

    typedef protocol_ref_t* iterator;
    typedef const protocol_ref_t* const_iterator;

    const_iterator begin() const {
        return list;
    }
    iterator begin() {
        return list;
    }
    const_iterator end() const {
        return list + count;
    }
    iterator end() {
        return list + count;
    }
};

struct locstamped_category_t {
    category_t *cat;
    struct header_info *hi;
};

struct locstamped_category_list_t {
    uint32_t count;
#if __LP64__
    uint32_t reserved;
#endif
    locstamped_category_t list[0];
};


// class_data_bits_t is the class_t->data field (class_rw_t pointer plus flags)
// The extra bits are optimized for the retain/release and alloc/dealloc paths.

// Values for class_ro_t->flags
// These are emitted by the compiler and are part of the ABI. 
// class is a metaclass
#define RO_META               (1<<0)
// class is a root class
#define RO_ROOT               (1<<1)
// class has .cxx_construct/destruct implementations
#define RO_HAS_CXX_STRUCTORS  (1<<2)
// class has +load implementation
// #define RO_HAS_LOAD_METHOD    (1<<3)
// class has visibility=hidden set
#define RO_HIDDEN             (1<<4)
// class has attribute(objc_exception): OBJC_EHTYPE_$_ThisClass is non-weak
#define RO_EXCEPTION          (1<<5)
// this bit is available for reassignment
// #define RO_REUSE_ME           (1<<6) 
// class compiled with -fobjc-arc (automatic retain/release)
#define RO_IS_ARR             (1<<7)
// class has .cxx_destruct but no .cxx_construct (with RO_HAS_CXX_STRUCTORS)
#define RO_HAS_CXX_DTOR_ONLY  (1<<8)

// class is in an unloadable bundle - must never be set by compiler
#define RO_FROM_BUNDLE        (1<<29)
// class is unrealized future class - must never be set by compiler
#define RO_FUTURE             (1<<30)
// class is realized - must never be set by compiler
#define RO_REALIZED           (1<<31)

// Values for class_rw_t->flags
// These are not emitted by the compiler and are never used in class_ro_t. 
// Their presence should be considered in future ABI versions.
// class_t->data is class_rw_t, not class_ro_t
#define RW_REALIZED           (1<<31)
// class is unresolved future class
#define RW_FUTURE             (1<<30)
// class is initialized
#define RW_INITIALIZED        (1<<29)
// class is initializing
#define RW_INITIALIZING       (1<<28)
// class_rw_t->ro is heap copy of class_ro_t
#define RW_COPIED_RO          (1<<27)
// class allocated but not yet registered
#define RW_CONSTRUCTING       (1<<26)
// class allocated and registered
#define RW_CONSTRUCTED        (1<<25)
// GC:  class has unsafe finalize method
#define RW_FINALIZE_ON_MAIN_THREAD (1<<24)
// class +load has been called
#define RW_LOADED             (1<<23)
#if !SUPPORT_NONPOINTER_ISA
// class instances may have associative references
#define RW_INSTANCES_HAVE_ASSOCIATED_OBJECTS (1<<22)
#endif
// class has instance-specific GC layout
#define RW_HAS_INSTANCE_SPECIFIC_LAYOUT (1 << 21)
// available for use
// #define RW_20       (1<<20)
// class has started realizing but not yet completed it
#define RW_REALIZING          (1<<19)

// NOTE: MORE RW_ FLAGS DEFINED BELOW


// Values for class_rw_t->flags or class_t->bits
// These flags are optimized for retain/release and alloc/dealloc
// 64-bit stores more of them in class_t->bits to reduce pointer indirection.

#if !__LP64__

// class or superclass has .cxx_construct implementation
#define RW_HAS_CXX_CTOR       (1<<18)
// class or superclass has .cxx_destruct implementation
#define RW_HAS_CXX_DTOR       (1<<17)
// class or superclass has default alloc/allocWithZone: implementation
// Note this is is stored in the metaclass.
#define RW_HAS_DEFAULT_AWZ    (1<<16)
// class's instances requires raw isa
// not tracked for 32-bit because it only applies to non-pointer isa
// #define RW_REQUIRES_RAW_ISA

// class is a Swift class
#define FAST_IS_SWIFT         (1UL<<0)
// class or superclass has default retain/release/autorelease/retainCount/
//   _tryRetain/_isDeallocating/retainWeakReference/allowsWeakReference
#define FAST_HAS_DEFAULT_RR   (1UL<<1)
// data pointer
#define FAST_DATA_MASK        0xfffffffcUL

#elif 1
// Leaks-compatible version that steals low bits only.

// class or superclass has .cxx_construct implementation
#define RW_HAS_CXX_CTOR       (1<<18)
// class or superclass has .cxx_destruct implementation
#define RW_HAS_CXX_DTOR       (1<<17)
// class or superclass has default alloc/allocWithZone: implementation
// Note this is is stored in the metaclass.
#define RW_HAS_DEFAULT_AWZ    (1<<16)

// class is a Swift class
#define FAST_IS_SWIFT           (1UL<<0)
// class or superclass has default retain/release/autorelease/retainCount/
//   _tryRetain/_isDeallocating/retainWeakReference/allowsWeakReference
#define FAST_HAS_DEFAULT_RR     (1UL<<1)
// class's instances requires raw isa
#define FAST_REQUIRES_RAW_ISA   (1UL<<2)
// data pointer
#define FAST_DATA_MASK          0x00007ffffffffff8UL

#else
// Leaks-incompatible version that steals lots of bits.

// class is a Swift class
#define FAST_IS_SWIFT           (1UL<<0)
// class's instances requires raw isa
#define FAST_REQUIRES_RAW_ISA   (1UL<<1)
// class or superclass has .cxx_destruct implementation
//   This bit is aligned with isa_t->hasCxxDtor to save an instruction.
#define FAST_HAS_CXX_DTOR       (1UL<<2)
// data pointer
#define FAST_DATA_MASK          0x00007ffffffffff8UL
// class or superclass has .cxx_construct implementation
#define FAST_HAS_CXX_CTOR       (1UL<<47)
// class or superclass has default alloc/allocWithZone: implementation
// Note this is is stored in the metaclass.
#define FAST_HAS_DEFAULT_AWZ    (1UL<<48)
// class or superclass has default retain/release/autorelease/retainCount/
//   _tryRetain/_isDeallocating/retainWeakReference/allowsWeakReference
#define FAST_HAS_DEFAULT_RR     (1UL<<49)
// summary bit for fast alloc path: !hasCxxCtor and 
//   !requiresRawIsa and instanceSize fits into shiftedSize
#define FAST_ALLOC              (1UL<<50)
// instance size in units of 16 bytes
//   or 0 if the instance size is too big in this field
//   This field must be LAST
#define FAST_SHIFTED_SIZE_SHIFT 51

// FAST_ALLOC means
//   FAST_HAS_CXX_CTOR is set
//   FAST_REQUIRES_RAW_ISA is not set
//   FAST_SHIFTED_SIZE is not zero
// FAST_ALLOC does NOT check FAST_HAS_DEFAULT_AWZ because that 
// bit is stored on the metaclass.
#define FAST_ALLOC_MASK  (FAST_HAS_CXX_CTOR | FAST_REQUIRES_RAW_ISA)
#define FAST_ALLOC_VALUE (0)

#endif

//class_ro_t 结构体定义 read only ----用来存储 类 在编译期就已经确定的属性、方法以及遵循的协议
//后面还有 类的读写结构的定义 class_rw_t
struct class_ro_t {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
#ifdef __LP64__
    uint32_t reserved;
#endif

    const uint8_t * ivarLayout;
    
    const char * name;
    method_list_t * baseMethodList;
    protocol_list_t * baseProtocols;
    const ivar_list_t * ivars;

    const uint8_t * weakIvarLayout;
    property_list_t *baseProperties;

    method_list_t *baseMethods() const {
        return baseMethodList;
    }
};


/***********************************************************************
* list_array_tt<Element, List>
* Generic（普通的） implementation for metadata that can be augmented by categories.
*
* Element is the underlying metadata type (e.g. method_t)
* List is the metadata's list type (e.g. method_list_t)
*
* A list_array_tt has one of three values:
* - empty
* - a pointer to a single list
* - an array of pointers to lists
*
 Element 是元数据类型，比如 method_t
 List 是元数据的列表类型，比如 method_list_t
 
 一个 list_array_tt 的值可能有三种情况：
 - 空的
 - 一个指针指向一个单独的列表
 - 一个数组，数组中都是指针，每个指针分别指向一个列表
 
* countLists/beginLists/endLists iterate the metadata lists
* count/begin/end iterate the underlying metadata elements
**********************************************************************/
template <typename Element, typename List>

// list_array_tt 类的自定义 最基本 抽象的类  下面的
class list_array_tt {
    struct array_t {
        uint32_t count;
        List* lists[0];

        static size_t byteSize(uint32_t count) {
            return sizeof(array_t) + count*sizeof(lists[0]);
        }
        size_t byteSize() {
            return byteSize(count);
        }
    };

 protected:
    class iterator {
        List **lists;
        List **listsEnd;
        typename List::iterator m, mEnd;

     public:
        iterator(List **begin, List **end) 
            : lists(begin), listsEnd(end)
        {
            if (begin != end) {
                m = (*begin)->begin();
                mEnd = (*begin)->end();
            }
        }

        const Element& operator * () const {
            return *m;
        }
        Element& operator * () {
            return *m;
        }

        bool operator != (const iterator& rhs) const {
            if (lists != rhs.lists) return true;
            if (lists == listsEnd) return false;  // m is undefined
            if (m != rhs.m) return true;
            return false;
        }

        const iterator& operator ++ () {
            assert(m != mEnd);
            m++;
            if (m == mEnd) {
                assert(lists != listsEnd);
                lists++;
                if (lists != listsEnd) {
                    m = (*lists)->begin();
                    mEnd = (*lists)->end();
                }
            }
            return *this;
        }
    };          //iterator类 结束

 private:
    union {
        List* list;
        uintptr_t arrayAndFlag;
    };

    bool hasArray() const {
        return arrayAndFlag & 1;
    }

    array_t *array() {
        return (array_t *)(arrayAndFlag & ~1);
    }

    void setArray(array_t *array) {
        arrayAndFlag = (uintptr_t)array | 1;
    }

 public:

    uint32_t count() {
        uint32_t result = 0;
        for (auto lists = beginLists(), end = endLists(); 
             lists != end;
             ++lists)
        {
            result += (*lists)->count;
        }
        return result;
    }

    iterator begin() {
        return iterator(beginLists(), endLists());
    }

    iterator end() {
        List **e = endLists();
        return iterator(e, e);
    }


    uint32_t countLists() {
        if (hasArray()) {
            return array()->count;
        } else if (list) {
            return 1;
        } else {
            return 0;
        }
    }

    List** beginLists() {
        if (hasArray()) {
            return array()->lists;
        } else {
            return &list;
        }
    }

    List** endLists() {
        if (hasArray()) {
            return array()->lists + array()->count;
        } else if (list) {
            return &list + 1;
        } else {
            return &list;
        }
    }

    void attachLists(List* const * addedLists, uint32_t addedCount) {
        if (addedCount == 0) return;

        if (hasArray()) {
            // many lists -> many lists
            uint32_t oldCount = array()->count;
            uint32_t newCount = oldCount + addedCount;
            setArray((array_t *)realloc(array(), array_t::byteSize(newCount)));
            array()->count = newCount;
            memmove(array()->lists + addedCount, array()->lists, 
                    oldCount * sizeof(array()->lists[0]));
            memcpy(array()->lists, addedLists, 
                   addedCount * sizeof(array()->lists[0]));
        }
        else if (!list  &&  addedCount == 1) {
            // 0 lists -> 1 list
            list = addedLists[0];
        } 
        else {
            // 1 list -> many lists
            List* oldList = list;
            uint32_t oldCount = oldList ? 1 : 0;
            uint32_t newCount = oldCount + addedCount;
            setArray((array_t *)malloc(array_t::byteSize(newCount)));
            array()->count = newCount;
            if (oldList) array()->lists[addedCount] = oldList;
            memcpy(array()->lists, addedLists, 
                   addedCount * sizeof(array()->lists[0]));
        }
    }

    void tryFree() {
        if (hasArray()) {
            for (uint32_t i = 0; i < array()->count; i++) {
                try_free(array()->lists[i]);
            }
            try_free(array());
        }
        else if (list) {
            try_free(list);
        }
    }

    template<typename Result>
    Result duplicate() {
        Result result;

        if (hasArray()) {
            array_t *a = array();
            result.setArray((array_t *)memdup(a, a->byteSize()));
            for (uint32_t i = 0; i < a->count; i++) {
                result.array()->lists[i] = a->lists[i]->duplicate();
            }
        } else if (list) {
            result.list = list->duplicate();
        } else {
            result.list = nil;
        }

        return result;
    }
};          // list_array_tt 类定义结束

//方法集合类的定义
// method_array_t 类的定义 继承自 list_array_tt
class method_array_t : 
    public list_array_tt<method_t, method_list_t> 
{
    typedef list_array_tt<method_t, method_list_t> Super;

 public:
    method_list_t **beginCategoryMethodLists() {
        return beginLists();
    }
    
    method_list_t **endCategoryMethodLists(Class cls);

    method_array_t duplicate() {
        return Super::duplicate<method_array_t>();
    }
};

//属性集合类 property_array_t 继承自 list_array_tt  类
class property_array_t : 
    public list_array_tt<property_t, property_list_t> 
{
    typedef list_array_tt<property_t, property_list_t> Super;

 public:
    property_array_t duplicate() {
        return Super::duplicate<property_array_t>();
    }
};

//协议集合类的定义
class protocol_array_t : 
    public list_array_tt<protocol_ref_t, protocol_list_t> 
{
    typedef list_array_tt<protocol_ref_t, protocol_list_t> Super;

 public:
    protocol_array_t duplicate() {
        return Super::duplicate<protocol_array_t>();
    }
};

//类的读写结构的定义 class_rw_t 之前有 class_ro_t
//class_ro_t 结构体定义 read only ----用来存储 类 在编译期就已经确定的属性、方法以及遵循的协议
struct class_rw_t {
    uint32_t flags;
    uint32_t version;

    const class_ro_t *ro;  // 类的读写结构中有 类的ro（read only）结构的数据 ro的数据是在编译器就已经确定的 包括是不是元类 以及相应的在编译器就已经确定的属性方法协议

    method_array_t methods;
    property_array_t properties;
    protocol_array_t protocols;

    Class firstSubclass;
    Class nextSiblingClass;

    char *demangledName;

    void setFlags(uint32_t set) 
    {
        OSAtomicOr32Barrier(set, &flags);
    }

    void clearFlags(uint32_t clear) 
    {
        OSAtomicXor32Barrier(clear, &flags);
    }

    // set and clear must not overlap
    void changeFlags(uint32_t set, uint32_t clear) 
    {
        assert((set & clear) == 0);

        uint32_t oldf, newf;
        do {
            oldf = flags;
            newf = (oldf | set) & ~clear;
        } while (!OSAtomicCompareAndSwap32Barrier(oldf, newf, (volatile int32_t *)&flags));
    }
};

// class_data_bits_t 结构体的定义 里面存储了类的一些信息 ro 或者是 rw的一些信息
struct class_data_bits_t {

    // Values are the FAST_ flags above.
    uintptr_t bits;
    
    // 只有这个一个成员变量，所有数据都存在这里，包括 rw 的地址和一些 flag
    // 1. 在 realized 之前，bits 存的是 ro 的地址，
    // 2. realized 后，bits 存 rw 的地址，rw 里的存有 ro 的地址
    
private:
    bool getBit(uintptr_t bit)
    {
        return bits & bit;
    }

#if FAST_ALLOC
    static uintptr_t updateFastAlloc(uintptr_t oldBits, uintptr_t change)
    {
        if (change & FAST_ALLOC_MASK) {
            if (((oldBits & FAST_ALLOC_MASK) == FAST_ALLOC_VALUE)  &&  
                ((oldBits >> FAST_SHIFTED_SIZE_SHIFT) != 0)) 
            {
                oldBits |= FAST_ALLOC;
            } else {
                oldBits &= ~FAST_ALLOC;
            }
        }
        return oldBits;
    }
#else
    static uintptr_t updateFastAlloc(uintptr_t oldBits, uintptr_t change) {
        return oldBits;
    }
#endif

    void setBits(uintptr_t set) 
    {
        uintptr_t oldBits;
        uintptr_t newBits;
        do {
            oldBits = LoadExclusive(&bits);
            newBits = updateFastAlloc(oldBits | set, set);
        } while (!StoreReleaseExclusive(&bits, oldBits, newBits));
    }

    void clearBits(uintptr_t clear) 
    {
        uintptr_t oldBits;
        uintptr_t newBits;
        do {
            oldBits = LoadExclusive(&bits);
            newBits = updateFastAlloc(oldBits & ~clear, clear);
        } while (!StoreReleaseExclusive(&bits, oldBits, newBits));
    }

public:

    class_rw_t* data() {
        return (class_rw_t *)(bits & FAST_DATA_MASK);
    }
    void setData(class_rw_t *newData)
    {
        assert(!data()  ||  (newData->flags & (RW_REALIZING | RW_FUTURE)));
        // Set during realization or construction only. No locking needed.
        bits = (bits & ~FAST_DATA_MASK) | (uintptr_t)newData;
    }

    bool hasDefaultRR() {
        return getBit(FAST_HAS_DEFAULT_RR);
    }
    void setHasDefaultRR() {
        setBits(FAST_HAS_DEFAULT_RR);
    }
    void setHasCustomRR() {
        clearBits(FAST_HAS_DEFAULT_RR);
    }

#if FAST_HAS_DEFAULT_AWZ
    bool hasDefaultAWZ() {
        return getBit(FAST_HAS_DEFAULT_AWZ);
    }
    void setHasDefaultAWZ() {
        setBits(FAST_HAS_DEFAULT_AWZ);
    }
    void setHasCustomAWZ() {
        clearBits(FAST_HAS_DEFAULT_AWZ);
    }
#else
    bool hasDefaultAWZ() {
        return data()->flags & RW_HAS_DEFAULT_AWZ;
    }
    void setHasDefaultAWZ() {
        data()->setFlags(RW_HAS_DEFAULT_AWZ);
    }
    void setHasCustomAWZ() {
        data()->clearFlags(RW_HAS_DEFAULT_AWZ);
    }
#endif

#if FAST_HAS_CXX_CTOR
    bool hasCxxCtor() {
        return getBit(FAST_HAS_CXX_CTOR);
    }
    void setHasCxxCtor() {
        setBits(FAST_HAS_CXX_CTOR);
    }
#else
    bool hasCxxCtor() {
        return data()->flags & RW_HAS_CXX_CTOR;
    }
    void setHasCxxCtor() {
        data()->setFlags(RW_HAS_CXX_CTOR);
    }
#endif

#if FAST_HAS_CXX_DTOR
    bool hasCxxDtor() {
        return getBit(FAST_HAS_CXX_DTOR);
    }
    void setHasCxxDtor() {
        setBits(FAST_HAS_CXX_DTOR);
    }
#else
    bool hasCxxDtor() {
        return data()->flags & RW_HAS_CXX_DTOR;
    }
    void setHasCxxDtor() {
        data()->setFlags(RW_HAS_CXX_DTOR);
    }
#endif

#if FAST_REQUIRES_RAW_ISA
    bool requiresRawIsa() {
        return getBit(FAST_REQUIRES_RAW_ISA);
    }
    void setRequiresRawIsa() {
        setBits(FAST_REQUIRES_RAW_ISA);
    }
#else
# if SUPPORT_NONPOINTER_ISA
#   error oops
# endif
    bool requiresRawIsa() {
        return true;
    }
    void setRequiresRawIsa() {
        // nothing
    }
#endif

#if FAST_ALLOC
    size_t fastInstanceSize() 
    {
        assert(bits & FAST_ALLOC);
        return (bits >> FAST_SHIFTED_SIZE_SHIFT) * 16;
    }
    void setFastInstanceSize(size_t newSize) 
    {
        // Set during realization or construction only. No locking needed.
        assert(data()->flags & RW_REALIZING);

        // Round up to 16-byte boundary, then divide to get 16-byte units
        newSize = ((newSize + 15) & ~15) / 16;
        
        uintptr_t newBits = newSize << FAST_SHIFTED_SIZE_SHIFT;
        if ((newBits >> FAST_SHIFTED_SIZE_SHIFT) == newSize) {
            int shift = WORD_BITS - FAST_SHIFTED_SIZE_SHIFT;
            uintptr_t oldBits = (bits << shift) >> shift;
            if ((oldBits & FAST_ALLOC_MASK) == FAST_ALLOC_VALUE) {
                newBits |= FAST_ALLOC;
            }
            bits = oldBits | newBits;
        }
    }

    bool canAllocFast() {
        return bits & FAST_ALLOC;
    }
#else
    size_t fastInstanceSize() {
        abort();
    }
    void setFastInstanceSize(size_t) {
        // nothing
    }
    bool canAllocFast() {
        return false;
    }
#endif

    bool isSwift() {
        return getBit(FAST_IS_SWIFT);
    }

    void setIsSwift() {
        setBits(FAST_IS_SWIFT);
    }
};
//class_data_bits_t 结构体的定义结束了 里面存储了类的一些信息 ro 或者是 rw的一些信息 及其操作方法

// objc_class 结构体的定义

//typedef struct objc_class *Class;

//struct objc_class : objc_object {//objc-runtime-old中也有这个的定义
struct objc_class : objc_object {
    // Class ISA;
    Class superclass;
    cache_t cache;             // formerly cache pointer and vtable
                                //缓存槽 之前定义的结构体 bucket_t 槽
    class_data_bits_t bits;    // class_rw_t * plus custom rr/alloc flags
                                //上面定义的一个结构体 里面存储的是 类的一些信息 ro 或者是 rw的一些信息 及其操作方法
    class_rw_t *data() {
        return bits.data();  //class_rw_t* data() {
    }
    void setData(class_rw_t *newData) {
        bits.setData(newData);
    }

    void setInfo(uint32_t set) {
        assert(isFuture()  ||  isRealized());
        data()->setFlags(set);
    }

    void clearInfo(uint32_t clear) {
        assert(isFuture()  ||  isRealized());
        data()->clearFlags(clear);
    }

    // set and clear must not overlap
    void changeInfo(uint32_t set, uint32_t clear) {
        assert(isFuture()  ||  isRealized());
        assert((set & clear) == 0);
        data()->changeFlags(set, clear);
    }

    bool hasCustomRR() {
        return ! bits.hasDefaultRR();
    }
    void setHasDefaultRR() {
        assert(isInitializing());
        bits.setHasDefaultRR();
    }
    void setHasCustomRR(bool inherited = false);
    void printCustomRR(bool inherited);

    bool hasCustomAWZ() {
        return ! bits.hasDefaultAWZ();
    }
    void setHasDefaultAWZ() {
        assert(isInitializing());
        bits.setHasDefaultAWZ();
    }
    void setHasCustomAWZ(bool inherited = false);
    void printCustomAWZ(bool inherited);

    bool requiresRawIsa() {
        return bits.requiresRawIsa();
    }
    void setRequiresRawIsa(bool inherited = false);
    void printRequiresRawIsa(bool inherited);

    bool canAllocIndexed() {
        assert(!isFuture());
        return !requiresRawIsa();
    }
    bool canAllocFast() {
        assert(!isFuture());
        return bits.canAllocFast();
    }


    bool hasCxxCtor() {
        // addSubclass() propagates this flag from the superclass.
        assert(isRealized());
        return bits.hasCxxCtor();
    }
    void setHasCxxCtor() { 
        bits.setHasCxxCtor();
    }

    bool hasCxxDtor() {
        // addSubclass() propagates this flag from the superclass.
        assert(isRealized());
        return bits.hasCxxDtor();
    }
    void setHasCxxDtor() { 
        bits.setHasCxxDtor();
    }


    bool isSwift() {
        return bits.isSwift();
    }


#if SUPPORT_NONPOINTER_ISA
    // Tracked in non-pointer isas; not tracked otherwise
#else
    bool instancesHaveAssociatedObjects() {
        // this may be an unrealized future class in the CF-bridged case
        assert(isFuture()  ||  isRealized());
        return data()->flags & RW_INSTANCES_HAVE_ASSOCIATED_OBJECTS;
    }

    void setInstancesHaveAssociatedObjects() {
        // this may be an unrealized future class in the CF-bridged case
        assert(isFuture()  ||  isRealized());
        setInfo(RW_INSTANCES_HAVE_ASSOCIATED_OBJECTS);
    }
#endif

    bool shouldGrowCache() {
        return true;
    }

    void setShouldGrowCache(bool) {
        // fixme good or bad for memory use?
    }

    bool shouldFinalizeOnMainThread() {
        // finishInitializing() propagates this flag from the superclass.
        assert(isRealized());
        return data()->flags & RW_FINALIZE_ON_MAIN_THREAD;
    }

    void setShouldFinalizeOnMainThread() {
        assert(isRealized());
        setInfo(RW_FINALIZE_ON_MAIN_THREAD);
    }

    bool isInitializing() {
        return getMeta()->data()->flags & RW_INITIALIZING;
    }

    void setInitializing() {
        assert(!isMetaClass());
        ISA()->setInfo(RW_INITIALIZING);
    }

    bool isInitialized() {
        return getMeta()->data()->flags & RW_INITIALIZED;
    }

    void setInitialized();

    bool isLoadable() {
        assert(isRealized());
        return true;  // any class registered for +load is definitely loadable
    }

    IMP getLoadMethod();

    // Locking: To prevent concurrent realization, hold runtimeLock.
    bool isRealized() {
        return data()->flags & RW_REALIZED;
    }

    // Returns true if this is an unrealized future class.
    // Locking: To prevent concurrent realization, hold runtimeLock.
    bool isFuture() { 
        return data()->flags & RW_FUTURE;
    }

    bool isMetaClass() {
        assert(this);
        assert(isRealized());
        return data()->ro->flags & RO_META;
    }

    // NOT identical to this->ISA when this is a metaclass
    Class getMeta() {
        if (isMetaClass()) return (Class)this;
        else return this->ISA();
    }

    bool isRootClass() {
        return superclass == nil;
    }
    bool isRootMetaclass() {
        return ISA() == (Class)this;
    }

    const char *mangledName() {   //返回重整的名字
        // fixme can't assert locks here
        assert(this);

        if (isRealized()  ||  isFuture()) {
            return data()->ro->name;
        } else {
            return ((const class_ro_t *)data())->name;
        }
    }
    
    const char *demangledName(bool realize = false);
    const char *nameForLogging();

    // May be unaligned depending on class's ivars.
    uint32_t unalignedInstanceSize() {
        assert(isRealized());
        return data()->ro->instanceSize;
    }

    // Class's ivar size rounded up to a pointer-size boundary.
    uint32_t alignedInstanceSize() {
        return word_align(unalignedInstanceSize());
    }

    size_t instanceSize(size_t extraBytes) {
        size_t size = alignedInstanceSize() + extraBytes;
        // CF requires all objects be at least 16 bytes.
        if (size < 16) size = 16;
        return size;
    }

    void setInstanceSize(uint32_t newSize) {
        assert(isRealized());
        if (newSize != data()->ro->instanceSize) {
            assert(data()->flags & RW_COPIED_RO);
            *const_cast<uint32_t *>(&data()->ro->instanceSize) = newSize;
        }
        bits.setFastInstanceSize(newSize);
    }
};
//objc_class 结构体定义结束

//swift_class_t 结构体定义开始 ---Swift的类
struct swift_class_t : objc_class {
    uint32_t flags;
    uint32_t instanceAddressOffset;
    uint32_t instanceSize;
    uint16_t instanceAlignMask;
    uint16_t reserved;

    uint32_t classSize;
    uint32_t classAddressOffset;
    void *description;
    // ...

    void *baseAddress() {
        return (void *)((uint8_t *)this - classAddressOffset);
    }
};

//分类的结构体的定义开始  没有继承自 objc_class
struct category_t {
    const char *name;
    classref_t cls;
    struct method_list_t *instanceMethods;
    struct method_list_t *classMethods;
    struct protocol_list_t *protocols;
    struct property_list_t *instanceProperties;

    method_list_t *methodsForMeta(bool isMeta) {
        if (isMeta) return classMethods;
        else return instanceMethods;
    }

    property_list_t *propertiesForMeta(bool isMeta) {
        if (isMeta) return nil; // classProperties;
        else return instanceProperties;
    }
};
//分类的结构体的定义结束

struct objc_super2 {
    id receiver;
    Class current_class;
};

struct message_ref_t {
    IMP imp;
    SEL sel;
};


extern Method protocol_getMethod(protocol_t *p, SEL sel, bool isRequiredMethod, bool isInstanceMethod, bool recursive);

// 深度遍历 top 类及其子孙类
static inline void
foreach_realized_class_and_subclass_2(Class top, bool (^code)(Class)) 
{
    // runtimeLock.assertWriting();
    assert(top);
    Class cls = top;
    while (1) {
        if (!code(cls)) break;

        if (cls->data()->firstSubclass) {
            cls = cls->data()->firstSubclass;
        } else {
            while (!cls->data()->nextSiblingClass  &&  cls != top) {
                cls = cls->superclass;
            }
            if (cls == top) break;
            cls = cls->data()->nextSiblingClass;
        }
    }
}

static inline void
foreach_realized_class_and_subclass(Class top, void (^code)(Class)) 
{
    foreach_realized_class_and_subclass_2(top, ^bool(Class cls) { 
        code(cls); return true; 
    });
}

#endif
