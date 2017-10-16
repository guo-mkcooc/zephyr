/*
 * Copyright (c) 2017, Intel Corporation
 *
 * SPDX-License-Identifier: Apache 2.0
 */


#ifndef _ZEPHYR_SYSCALL_HANDLER_H_
#define _ZEPHYR_SYSCALL_HANDLER_H_

#ifdef CONFIG_USERSPACE

#ifndef _ASMLANGUAGE
#include <kernel.h>
#include <misc/printk.h>
#include <nano_internal.h>

extern const _k_syscall_handler_t _k_syscall_table[K_SYSCALL_LIMIT];

/**
 * Ensure a system object is a valid object of the expected type
 *
 * Searches for the object and ensures that it is indeed an object
 * of the expected type, that the caller has the right permissions on it,
 * and that the object has been initialized.
 *
 * This function is intended to be called on the kernel-side system
 * call handlers to validate kernel object pointers passed in from
 * userspace.
 *
 * @param ko Kernel object metadata pointer, or NULL
 * @param otype Expected type of the kernel object, or K_OBJ_ANY if type
 *	  doesn't matter
 * @param init If true, this is for an init function and we will not error
 *	   out if the object is not initialized
 * @return 0 If the object is valid
 *         -EBADF if not a valid object of the specified type
 *         -EPERM If the caller does not have permissions
 *         -EINVAL Object is not initialized
 */
int _k_object_validate(struct _k_object *ko, enum k_objects otype, int init);

/**
 * Dump out error information on failed _k_object_validate() call
 *
 * @param retval Return value from _k_object_validate()
 * @param obj Kernel object we were trying to verify
 * @param ko If retval=-EPERM, struct _k_object * that was looked up, or NULL
 * @param otype Expected type of the kernel object
 */
extern void _dump_object_error(int retval, void *obj, struct _k_object *ko,
			enum k_objects otype);

/**
 * Kernel object validation function
 *
 * Retrieve metadata for a kernel object. This function is implemented in
 * the gperf script footer, see gen_kobject_list.py
 *
 * @param obj Address of kernel object to get metadata
 * @return Kernel object's metadata, or NULL if the parameter wasn't the
 * memory address of a kernel object
 */
extern struct _k_object *_k_object_find(void *obj);

typedef void (*_wordlist_cb_func_t)(struct _k_object *ko, void *context);

/**
 * Iterate over all the kernel object metadata in the system
 *
 * @param func function to run on each struct _k_object
 * @param context Context pointer to pass to each invocation
 */
extern void _k_object_wordlist_foreach(_wordlist_cb_func_t func, void *context);

/**
 * Copy all kernel object permissions from the parent to the child
 *
 * @param parent Parent thread, to get permissions from
 * @param child Child thread, to copy permissions to
 */
extern void _thread_perms_inherit(struct k_thread *parent,
				  struct k_thread *child);

/**
 * Grant a thread permission to a kernel object
 *
 * @param ko Kernel object metadata to update
 * @param thread The thread to grant permission
 */
extern void _thread_perms_set(struct _k_object *ko, struct k_thread *thread);

/**
 * Revoke a thread's permission to a kernel object
 *
 * @param ko Kernel object metadata to update
 * @param thread The thread to grant permission
 */
extern void _thread_perms_clear(struct _k_object *ko, struct k_thread *thread);

/**
 * Grant all current and future threads access to a kernel object
 *
 * @param ko Kernel object metadata to update
 */
extern void _thread_perms_all_set(struct _k_object *ko);

/**
 * @brief Runtime expression check for system call arguments
 *
 * Used in handler functions to perform various runtime checks on arguments,
 * and generate a kernel oops if anything is not expected, printing a custom
 * message.
 *
 * @param expr Boolean expression to verify, a false result will trigger an
 *             oops
 * @param fmt Printf-style format string (followed by appropriate variadic
 *            arguments) to print on verification failure
 */
#define _SYSCALL_VERIFY_MSG(expr, fmt, ...) \
	do { \
		if (!(expr)) { \
			printk("FATAL: syscall %s failed check: " fmt "\n", \
			       __func__, ##__VA_ARGS__); \
			_arch_syscall_oops(ssf); \
		} \
	} while (0)

/**
 * @brief Runtime expression check for system call arguments
 *
 * Used in handler functions to perform various runtime checks on arguments,
 * and generate a kernel oops if anything is not expected.
 *
 * @param expr Boolean expression to verify, a false result will trigger an
 *             oops. A stringified version of this expression will be printed.
 */
#define _SYSCALL_VERIFY(expr) _SYSCALL_VERIFY_MSG(expr, #expr)

#define _SYSCALL_MEMORY(ptr, size, write) \
	_SYSCALL_VERIFY_MSG(!_arch_buffer_validate((void *)ptr, size, write), \
			    "Memory region %p (size %u) %s access denied", \
			    (void *)(ptr), (u32_t)(size), \
			    write ? "write" : "read")

/**
 * @brief Runtime check that a user thread has read permission to a memory area
 *
 * Checks that the particular memory area is readable by the currently running
 * thread if the CPU was in user mode, and generates a kernel oops if it
 * wasn't. Prevents userspace from getting the kernel to read memory the thread
 * does not have access to, or passing in garbage pointers that would
 * crash/pagefault the kernel if dereferenced.
 *
 * @param ptr Memory area to examine
 * @param size Size of the memory area
 * @param write If the thread should be able to write to this memory, not just
 *		read it
 */
#define _SYSCALL_MEMORY_READ(ptr, size) \
	_SYSCALL_MEMORY(ptr, size, 0)

/**
 * @brief Runtime check that a user thread has write permission to a memory area
 *
 * Checks that the particular memory area is readable and writable by the
 * currently running thread if the CPU was in user mode, and generates a kernel
 * oops if it wasn't. Prevents userspace from getting the kernel to read or
 * modify memory the thread does not have access to, or passing in garbage
 * pointers that would crash/pagefault the kernel if dereferenced.
 *
 * @param ptr Memory area to examine
 * @param size Size of the memory area
 * @param write If the thread should be able to write to this memory, not just
 *		read it
 */
#define _SYSCALL_MEMORY_WRITE(ptr, size) \
	_SYSCALL_MEMORY(ptr, size, 1)

#define _SYSCALL_MEMORY_ARRAY(ptr, nmemb, size, write) \
	do { \
		u32_t product; \
		_SYSCALL_VERIFY_MSG(!__builtin_umul_overflow((u32_t)(nmemb), \
							     (u32_t)(size), \
							     &product), \
				    "%ux%u array is too large", \
				    (u32_t)(nmemb), (u32_t)(size)); \
		_SYSCALL_MEMORY(ptr, product, write); \
	} while (0)

/**
 * @brief Validate user thread has read permission for sized array
 *
 * Used when the memory region is expressed in terms of number of elements and
 * each element size, handles any overflow issues with computing the total
 * array bounds. Otherwise see _SYSCALL_MEMORY_READ.
 *
 * @param ptr Memory area to examine
 * @param nmemb Number of elements in the array
 * @param size Size of each array element
 */
#define _SYSCALL_MEMORY_ARRAY_READ(ptr, nmemb, size) \
	_SYSCALL_MEMORY_ARRAY(ptr, nmemb, size, 0)

/**
 * @brief Validate user thread has read/write permission for sized array
 *
 * Used when the memory region is expressed in terms of number of elements and
 * each element size, handles any overflow issues with computing the total
 * array bounds. Otherwise see _SYSCALL_MEMORY_WRITE.
 *
 * @param ptr Memory area to examine
 * @param nmemb Number of elements in the array
 * @param size Size of each array element
 */
#define _SYSCALL_MEMORY_ARRAY_WRITE(ptr, nmemb, size) \
	_SYSCALL_MEMORY_ARRAY(ptr, nmemb, size, 1)

static inline int _obj_validation_check(void *obj, enum k_objects otype,
					int init)
{
	struct _k_object *ko;
	int ret;

	ko = _k_object_find(obj);
	ret = _k_object_validate(ko, otype, init);

#ifdef CONFIG_PRINTK
	if (ret) {
		_dump_object_error(ret, obj, ko, otype);
	}
#endif

	return ret;
}

#define _SYSCALL_IS_OBJ(ptr, type, init) \
	_SYSCALL_VERIFY_MSG(!_obj_validation_check((void *)ptr, type, init), \
			    "object %p access denied", (void *)(ptr))

/**
 * @brief Runtime check kernel object pointer for non-init functions
 *
 * Calls _k_object_validate and triggers a kernel oops if the check files.
 * For use in system call handlers which are not init functions; a check
 * enforcing that an object is initialized* will not occur.
 *
 * @param ptr Untrusted kernel object pointer
 * @param type Expected kernel object type
 */
#define _SYSCALL_OBJ(ptr, type) \
	_SYSCALL_IS_OBJ(ptr, type, 0)

/**
 * @brief Runtime check kernel object pointer for non-init functions
 *
 * See description of _SYSCALL_IS_OBJ. For use in system call handlers which
 * are not init functions; a check enforcing that an object is initialized
 * will not occur.
 *
 * @param ptr Untrusted kernel object pointer
 * @param type Expected kernel object type
 */

#define _SYSCALL_OBJ_INIT(ptr, type) \
	_SYSCALL_IS_OBJ(ptr, type, 1)

/*
 * Handler definition macros
 *
 * All handlers have the same prototype:
 *
 * u32_t _handler_APINAME(u32_t arg1, u32_t arg2, u32_t arg3,
 *			  u32_t arg4, u32_t arg5, u32_t arg6, void *ssf);
 *
 * These make it much simpler to define handlers instead of typing out
 * the bolierplate. The macros ensure that the seventh argument is named
 * "ssf" as this is now referenced by various other _SYSCALL macros.
 *
 * The different variants here simply depend on how many of the 6 arguments
 * passed in are really used.
 */

#define _SYSCALL_HANDLER0(name_) \
	u32_t _handler_ ## name_(u32_t arg1 __unused, \
				 u32_t arg2 __unused, \
				 u32_t arg3 __unused, \
				 u32_t arg4 __unused, \
				 u32_t arg5 __unused, \
				 u32_t arg6 __unused, \
				 void *ssf)

#define _SYSCALL_HANDLER1(name_, arg1_) \
	u32_t _handler_ ## name_(u32_t arg1_, \
				 u32_t arg2 __unused, \
				 u32_t arg3 __unused, \
				 u32_t arg4 __unused, \
				 u32_t arg5 __unused, \
				 u32_t arg6 __unused, \
				 void *ssf)

#define _SYSCALL_HANDLER2(name_, arg1_, arg2_) \
	u32_t _handler_ ## name_(u32_t arg1_, \
				 u32_t arg2_, \
				 u32_t arg3 __unused, \
				 u32_t arg4 __unused, \
				 u32_t arg5 __unused, \
				 u32_t arg6 __unused, \
				 void *ssf)

#define _SYSCALL_HANDLER3(name_, arg1_, arg2_, arg3_) \
	u32_t _handler_ ## name_(u32_t arg1_, \
				 u32_t arg2_, \
				 u32_t arg3_, \
				 u32_t arg4 __unused, \
				 u32_t arg5 __unused, \
				 u32_t arg6 __unused, \
				 void *ssf)

#define _SYSCALL_HANDLER4(name_, arg1_, arg2_, arg3_, arg4_) \
	u32_t _handler_ ## name_(u32_t arg1_, \
				 u32_t arg2_, \
				 u32_t arg3_, \
				 u32_t arg4_, \
				 u32_t arg5 __unused, \
				 u32_t arg6 __unused, \
				 void *ssf)

#define _SYSCALL_HANDLER5(name_, arg1_, arg2_, arg3_, arg4_, arg5_) \
	u32_t _handler_ ## name_(u32_t arg1_, \
				 u32_t arg2_, \
				 u32_t arg3_, \
				 u32_t arg4_, \
				 u32_t arg5_, \
				 u32_t arg6 __unused, \
				 void *ssf)

#define _SYSCALL_HANDLER6(name_, arg1_, arg2_, arg3_, arg4_, arg5_, arg6_) \
	u32_t _handler_ ## name_(u32_t arg1_, \
				 u32_t arg2_, \
				 u32_t arg3_, \
				 u32_t arg4_, \
				 u32_t arg5_, \
				 u32_t arg6_, \
				 void *ssf)

/*
 * Helper macros for a very common case: calls which just take one argument
 * which is an initialized kernel object of a specific type. Verify the object
 * and call the implementation.
 */

#define _SYSCALL_HANDLER1_SIMPLE(name_, obj_enum_, obj_type_) \
	_SYSCALL_HANDLER1(name_, arg1) { \
		_SYSCALL_OBJ(arg1, obj_enum_); \
		return (u32_t)_impl_ ## name_((obj_type_)arg1); \
	}

#define _SYSCALL_HANDLER1_SIMPLE_VOID(name_, obj_enum_, obj_type_) \
	_SYSCALL_HANDLER1(name_, arg1) { \
		_SYSCALL_OBJ(arg1, obj_enum_); \
		_impl_ ## name_((obj_type_)arg1); \
		return 0; \
	}

#define _SYSCALL_HANDLER0_SIMPLE(name_) \
	_SYSCALL_HANDLER0(name_) { \
		return (u32_t)_impl_ ## name_(); \
	}

#define _SYSCALL_HANDLER0_SIMPLE_VOID(name_) \
	_SYSCALL_HANDLER0(name_) { \
		_impl_ ## name_(); \
		return 0; \
	}

#endif /* _ASMLANGUAGE */

#endif /* CONFIG_USERSPACE */

#endif /* _ZEPHYR_SYSCALL_H_ */