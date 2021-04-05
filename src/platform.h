// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#ifdef _MSC_VER
#define LYRA_UNUSED
#else
#define LYRA_UNUSED __attribute__((unused))
#endif

#ifdef _MSC_VER
#define LYRA_NO_RETURN __declspec(noreturn)
#else
#define LYRA_NO_RETURN __attribute__((noreturn))
#endif

#ifdef _MSC_VER
#define LYRA_ALWAYS_INLINE __forceinline
#else
#define LYRA_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#ifdef _MSC_VER
#define LYRA_UNREACHABLE __assume(0)
#else
#define LYRA_UNREACHABLE __builtin_unreachable()
#endif

#define LYRA_LIKELY(x) __builtin_expect(!!(x), 1)
#define LYRA_UNLIKELY(x) __builtin_expect(!!(x), 0)

#ifdef _Thread_local
#define LYRA_THREAD_LOCAL _Thread_local
#else
#define LYRA_THREAD_LOCAL
#endif

#if defined(_WIN32)
#ifdef _AUMENT_H
#define LYRA_PUBLIC __declspec(dllimport)
#define LYRA_PRIVATE
#else
#define LYRA_PUBLIC __declspec(dllexport)
#define LYRA_PRIVATE
#endif
#else
#define LYRA_PUBLIC __attribute__((visibility("default")))
#define LYRA_PRIVATE __attribute__((visibility("hidden")))
#endif

#ifdef LYRA_IS_INTERPRETER
#ifdef static_assert
#define LYRA_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define LYRA_STATIC_ASSERT(cond, msg) assert(cond, msg)
#endif
#else
#define LYRA_STATIC_ASSERT(cond, msg)
#endif

#define LYRA_FALLTHROUGH __attribute__((fallthrough))
