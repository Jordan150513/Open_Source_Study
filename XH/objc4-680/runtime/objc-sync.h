/*
 * Copyright (c) 2002, 2006 Apple Inc.  All Rights Reserved.
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

#ifndef __OBJC_SNYC_H_
#define __OBJC_SNYC_H_

#include <objc/objc.h>

/*
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        AXPerson * p = [AXPerson new];
        @synchronized (p) {
            NSLog(@"p is @synchronized");
        }
    }
    return 0;
}
*/
/*
int main(int argc, const char * argv[]) {
    { __AtAutoreleasePool __autoreleasepool;
        AXPerson * p = ((AXPerson *(*)(id, SEL))(void *)objc_msgSend)((id)objc_getClass("AXPerson"), sel_registerName("new"));
        {
            id _rethrow = 0;
            id _sync_obj = (id)p;
            objc_sync_enter(_sync_obj);  // <<=== 看这里
            try {
                struct _SYNC_EXIT {
                    _SYNC_EXIT(id arg) : sync_exit(arg) {} // 构造函数
                    ~_SYNC_EXIT() { // 析构函数，析构的时候会自动释放锁
                        objc_sync_exit(sync_exit); // <<=== 看这里
                    }
                    id sync_exit; // 成员变量
                }
                _sync_exit(_sync_obj);  // 不要被这句骗了，不然你会百思不得其解，
                                        // 这不是一个单独的语句，注意看结构体的右大括号后面并没有分号，
                                        // 所以 _sync_exit(_sync_obj) 是跟在 _SYNC_EXIT 结构体后面的
                                        // 即它声明了一个名为 _sync_exit 的结构体对象
                NSLog((NSString *)&__NSConstantStringImpl__var_folders_cp_sc2q63f937j88dcxp23f471w0000gn_T_main_3c34e3_mi_0);
 
                // 代码块结束后，_sync_exit 对象会被析构，在其析构函数中调用了 objc_sync_exit，来释放锁
 
            } catch (id e) {
                _rethrow = e;
            }
            {
                struct _FIN {
                    _FIN(id reth) : rethrow(reth) {}
                    ~_FIN() {
                        if (rethrow) objc_exception_throw(rethrow);
                    }
                    id rethrow;
                }
                _fin_force_rethow(_rethrow);
            }
        }
    }
    return 0;
}
*/

// 可以看到 @synchronized 的实现主要就是利用 objc_sync_enter 和 objc_sync_exit

/**
 * Begin synchronizing on 'obj'.
 * Allocates recursive pthread_mutex associated with 'obj' if needed.
 *
 * @param obj The object to begin synchronizing on.
 *
 * @return OBJC_SYNC_SUCCESS once lock is acquired.
 */
OBJC_EXPORT  int objc_sync_enter(id obj)
    __OSX_AVAILABLE_STARTING(__MAC_10_3, __IPHONE_2_0);

/**
 * End synchronizing on 'obj'.
 *
 * @param obj The objet to end synchronizing on.
 *
 * @return OBJC_SYNC_SUCCESS or OBJC_SYNC_NOT_OWNING_THREAD_ERROR
 */
OBJC_EXPORT  int objc_sync_exit(id obj)
    __OSX_AVAILABLE_STARTING(__MAC_10_3, __IPHONE_2_0);



// The wait/notify functions have never worked correctly and no longer exist.
OBJC_EXPORT  int objc_sync_wait(id obj, long long milliSecondsMaxWait) 
    UNAVAILABLE_ATTRIBUTE;
OBJC_EXPORT  int objc_sync_notify(id obj) 
    UNAVAILABLE_ATTRIBUTE;
OBJC_EXPORT  int objc_sync_notifyAll(id obj) 
    UNAVAILABLE_ATTRIBUTE;



enum {
	OBJC_SYNC_SUCCESS                 = 0,
	OBJC_SYNC_NOT_OWNING_THREAD_ERROR = -1,
	OBJC_SYNC_TIMED_OUT               = -2,
	OBJC_SYNC_NOT_INITIALIZED         = -3		
};


#endif // __OBJC_SNYC_H_
