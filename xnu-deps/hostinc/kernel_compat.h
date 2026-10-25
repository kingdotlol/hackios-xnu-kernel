/*
 * Kernel compatibility header for building XNU on Linux with standard clang.
 * Provides fallback implementations for Apple-specific clang builtins.
 */

#ifndef KERNEL_COMPAT_H
#define KERNEL_COMPAT_H

/* Check if we have the Apple-specific builtins */
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

/* Fallback for __builtin_xnu_type_summary - returns 0 (no type info) */
#if !__has_builtin(__builtin_xnu_type_summary)
#define __builtin_xnu_type_summary(type) 0
#endif

/* Fallback for __builtin_xnu_type_signature - returns 0 */
#if !__has_builtin(__builtin_xnu_type_signature)
#define __builtin_xnu_type_signature(type) 0
#endif

/* Fallback for __builtin_xnu_types_compatible - returns 1 (assume compatible) */
#if !__has_builtin(__builtin_xnu_types_compatible)
#define __builtin_xnu_types_compatible(type1, type2) 1
#endif

#endif /* KERNEL_COMPAT_H */
