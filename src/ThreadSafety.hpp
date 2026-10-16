/*
 * src/ThreadSafety.hpp
 * This file is part of Emeraude-Base
 *
 * Copyright (C) 2010-2026 - Sébastien Léon Claude Christian Bémelmans "LondNoir" <londnoir@gmail.com>
 *
 * Emeraude-Base is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Emeraude-Base is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Emeraude-Base; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Complete project and additional information can be found at :
 * https://github.com/EmeraudeEngine/emeraude-base
 *
 * --- THIS IS AUTOMATICALLY GENERATED, DO NOT CHANGE ---
 */

#pragma once

/**
 * @file ThreadSafety.hpp
 * @brief The Clang thread-safety analysis attributes (decision D12, Ave Robustus II, 2026-10-08): every member a mutex
 * protects is declared EMEN_GUARDED_BY(thatMutex), every function that needs the lock held EMEN_REQUIRES(thatMutex),
 * and Clang's -Wthread-safety (in the paranoid warning set) proves the protocol at compile time.
 * @note The attributes exist on Clang only; elsewhere every macro expands to nothing (GCC and MSVC compile the same code
 * without the check). They only work with a mutex type declared as a capability: use Base::Mutex and Base::MutexLock
 * (Mutex.hpp), never a bare std::mutex for annotated state (libstdc++'s is not annotated).
 * @see https://clang.llvm.org/docs/ThreadSafetyAnalysis.html (the attribute set and its mutex.h reference header).
 */

#if defined(__clang__)
	#define EMEN_THREAD_ANNOTATION(x) __attribute__((x))
#else
	#define EMEN_THREAD_ANNOTATION(x)
#endif

/** @brief Declares a class as a capability (a lockable resource), named for the diagnostics. */
#define EMEN_CAPABILITY(name) EMEN_THREAD_ANNOTATION(capability(name))

/** @brief Declares an RAII class that acquires a capability in its constructor and releases it in its destructor. */
#define EMEN_SCOPED_CAPABILITY EMEN_THREAD_ANNOTATION(scoped_lockable)

/** @brief The member is read and written only while the capability is held. */
#define EMEN_GUARDED_BY(x) EMEN_THREAD_ANNOTATION(guarded_by(x))

/** @brief The data the pointer member points to is accessed only while the capability is held. */
#define EMEN_PT_GUARDED_BY(x) EMEN_THREAD_ANNOTATION(pt_guarded_by(x))

/** @brief The function must be called with the capabilities held (exclusively). */
#define EMEN_REQUIRES(...) EMEN_THREAD_ANNOTATION(requires_capability(__VA_ARGS__))

/** @brief The function must be called with the capabilities held, at least shared (a read under a shared lock). */
#define EMEN_REQUIRES_SHARED(...) EMEN_THREAD_ANNOTATION(requires_shared_capability(__VA_ARGS__))

/** @brief The function must be called WITHOUT the capabilities held (it takes them itself: a re-entry deadlocks). */
#define EMEN_EXCLUDES(...) EMEN_THREAD_ANNOTATION(locks_excluded(__VA_ARGS__))

/** @brief The function acquires the capabilities and returns with them held. */
#define EMEN_ACQUIRE(...) EMEN_THREAD_ANNOTATION(acquire_capability(__VA_ARGS__))

/** @brief The function acquires the capabilities shared (a reader) and returns with them held. */
#define EMEN_ACQUIRE_SHARED(...) EMEN_THREAD_ANNOTATION(acquire_shared_capability(__VA_ARGS__))

/** @brief The function releases the capabilities it held shared. */
#define EMEN_RELEASE_SHARED(...) EMEN_THREAD_ANNOTATION(release_shared_capability(__VA_ARGS__))

/** @brief The function releases the capabilities. */
#define EMEN_RELEASE(...) EMEN_THREAD_ANNOTATION(release_capability(__VA_ARGS__))

/** @brief The function tries to acquire the capabilities and returns the given value on success. */
#define EMEN_TRY_ACQUIRE(...) EMEN_THREAD_ANNOTATION(try_acquire_capability(__VA_ARGS__))

/** @brief The function returns a reference to the given capability (an accessor of a mutex). */
#define EMEN_RETURN_CAPABILITY(x) EMEN_THREAD_ANNOTATION(lock_returned(x))
