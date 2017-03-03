/*
 * Copyright (c) 1999-2001, 2005-2007 Apple Inc.  All Rights Reserved.
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
	Protocol.h
	Copyright 1991-1996 NeXT Software, Inc.
*/

#include "objc-private.h"

#undef id
#undef Class

#include <stdlib.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/ldsyms.h>

#include "Protocol.h"

#if __OBJC2__
@interface __IncompleteProtocol : NSObject @end
@implementation __IncompleteProtocol 
// fixme hack - make __IncompleteProtocol a non-lazy class
+ (void) load { } 
@end
#endif

@implementation Protocol 

#if __OBJC2__
                //使协议是一个非懒加载的类
// fixme hack - make Protocol a non-lazy class
+ (void) load { } 
#endif

// 判断 一个协议（本协议） 是否遵守另外一个协议
- (BOOL) conformsTo: (Protocol *)aProtocolObj
{
    //调用了objc-runtime-new里面的方法，来查询本协议是否遵守这个协议，期间还需要通过递归实现 看self的协议列表里的子协议 是否有遵守这个协议
    return protocol_conformsToProtocol(self, aProtocolObj);
}

// 获取 协议中的 实例方法的描述信息 传过来是一个选择子 一个方法名
- (struct objc_method_description *) descriptionForInstanceMethod:(SEL)aSel
{
#if !__OBJC2__
    return lookup_protocol_method((struct old_protocol *)self, aSel, 
                                  YES/*required*/, YES/*instance*/, 
                                  YES/*recursive*/);
#else
    //调用了 runtime-new.mm 的 方法
    //内部实现 其实就是强转了一下 把SEL获取的 Method（method_t *类型的） 强转成了 struct objc_method_description * 类型 其实都是 char * 类型的东西
    //在强转之前 对 SEL 进行了一个操作 传入 SEL 返回 取得 proto（本协议） 协议中符合指定条件的方法
    return method_getDescription(protocol_getMethod((struct protocol_t *)self, 
                                                     aSel, YES, YES, YES));
#endif
}

- (struct objc_method_description *) descriptionForClassMethod:(SEL)aSel
{
#if !__OBJC2__
    return lookup_protocol_method((struct old_protocol *)self, aSel, 
                                  YES/*required*/, NO/*instance*/, 
                                  YES/*recursive*/);
#else
    return method_getDescription(protocol_getMethod((struct protocol_t *)self, 
                                                    aSel, YES, NO, YES));
#endif
}

// 取得协议的名字，是 demangledName，即正常的名字
- (const char *)name
{
    return protocol_getName(self);
}

- (BOOL)isEqual:other
{
#if __OBJC2__
    // check isKindOf:
    Class cls;
    Class protoClass = objc_getClass("Protocol");
    for (cls = object_getClass(other); cls; cls = cls->superclass) {
        if (cls == protoClass) break;
    }
    if (!cls) return NO;
    // check equality
    return protocol_isEqual(self, other);
#else
    return [other isKindOf:[Protocol class]] && [self conformsTo: other] && [other conformsTo: self];
#endif
}

#if __OBJC2__
- (NSUInteger)hash
{
    return 23;
}
#else
- (unsigned)hash
{
    return 23;
}
#endif

@end
