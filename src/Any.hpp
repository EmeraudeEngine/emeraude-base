/*
 * src/Any.hpp
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

/* STL inclusions. */
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>

/* Local inclusions. */
#include "Hash/FNV1a.hpp"

namespace EmEn::Base
{
	/**
	 * @brief Returns the compiler's signature of this function for a type: a string that names the type.
	 * @note `__PRETTY_FUNCTION__` (GCC, Clang) / `__FUNCSIG__` (MSVC) spells the template argument, which needs no RTTI
	 * — the technique of EnTT's `type_hash` (Michele Caini, MIT, https://github.com/skypjack/entt), written anew here.
	 * The text differs between compiler families, so its hash is only stable inside ONE toolchain: never persist it
	 * nor send it over the wire.
	 * @tparam type_t The type.
	 * @return std::string_view
	 */
	template< typename type_t >
	[[nodiscard]]
	constexpr
	std::string_view
	typeSignature () noexcept
	{
#if defined(_MSC_VER) && !defined(__clang__)
		return std::string_view{__FUNCSIG__};
#else
		return std::string_view{__PRETTY_FUNCTION__};
#endif
	}

	/**
	 * @brief The identity of a type without RTTI: the FNV-1a hash of its signature, computed at compile time.
	 * @note cv-qualifiers and references are removed first: `const std::shared_ptr< T > &` and `std::shared_ptr< T >`
	 * are the same payload type. It hashes a STRING, not an address, so it is the same in a shared library and in the
	 * executables that each carry their own copy of emeraude-base (a function-address identity is not).
	 * @warning Two types whose signatures collide (FNV-1a 64-bit) would be taken for each other: the accepted risk.
	 * @note consteval: always computed by the compiler, never at run time.
	 * @tparam type_t The type.
	 * @return size_t
	 */
	template< typename type_t >
	[[nodiscard]]
	consteval
	size_t
	typeHashOf () noexcept
	{
		return Hash::FNV1a(typeSignature< std::remove_cvref_t< type_t > >());
	}

	/**
	 * @brief A type-erased value WITHOUT RTTI: what `std::any` is, for a cascade built with `-fno-rtti` / `/GR-`.
	 * @note MSVC refuses `<any>` under `/GR-` (STL1003: `_HAS_STATIC_RTTI` is 0), so the Observer payload cannot be a
	 * `std::any` in an RTTI-free build. Same value semantics: it holds a copy of any copyable type, is copyable and
	 * movable, and is read back with anyCast() (nullptr on a type mismatch, never a throw). The type identity is
	 * typeHashOf().
	 * @note A value of at most SmallBufferSize bytes, nothrow-movable, is stored inline (a `std::shared_ptr` fits: no
	 * allocation, unlike libstdc++'s `std::any`); a larger one on the heap.
	 */
	class Any final
	{
		public:

			/** @brief The inline storage size. */
			static constexpr size_t SmallBufferSize{32};

			/**
			 * @brief Constructs an empty value.
			 */
			Any () noexcept = default;

			/**
			 * @brief Constructs a value holding a copy (or the move) of the argument — implicit, as std::any's.
			 * @tparam value_t The type of the argument (its decayed type is held).
			 * @param value The value.
			 */
			template< typename value_t >
			requires (!std::is_same_v< std::decay_t< value_t >, Any > && std::is_copy_constructible_v< std::decay_t< value_t > >)
			Any (value_t && value) noexcept
			{
				this->construct< std::decay_t< value_t > >(std::forward< value_t >(value));
			}

			/**
			 * @brief Copy constructor.
			 * @param copy A reference to the copied value.
			 */
			Any (const Any & copy) noexcept
			{
				if ( copy.m_manager != nullptr )
				{
					copy.m_manager(Action::Copy, *this, &copy, nullptr);
				}
			}

			/**
			 * @brief Move constructor.
			 * @param move A reference to the moved value (left empty).
			 */
			Any (Any && move) noexcept
			{
				if ( move.m_manager != nullptr )
				{
					move.m_manager(Action::Move, *this, nullptr, &move);
				}
			}

			/**
			 * @brief Copy assignment.
			 * @param copy A reference to the copied value.
			 * @return Any &
			 */
			Any &
			operator= (const Any & copy) noexcept
			{
				if ( this != &copy )
				{
					Any temporary{copy};

					*this = std::move(temporary);
				}

				return *this;
			}

			/**
			 * @brief Move assignment.
			 * @param move A reference to the moved value (left empty).
			 * @return Any &
			 */
			Any &
			operator= (Any && move) noexcept
			{
				if ( this != &move )
				{
					this->reset();

					if ( move.m_manager != nullptr )
					{
						move.m_manager(Action::Move, *this, nullptr, &move);
					}
				}

				return *this;
			}

			/**
			 * @brief Destructs the held value, if any.
			 */
			~Any ()
			{
				this->reset();
			}

			/**
			 * @brief Returns whether a value is held.
			 * @return bool
			 */
			[[nodiscard]]
			bool
			hasValue () const noexcept
			{
				return m_manager != nullptr;
			}

			/**
			 * @brief Returns the typeHashOf() of the held type, 0 when empty.
			 * @return size_t
			 */
			[[nodiscard]]
			size_t
			typeHash () const noexcept
			{
				return m_typeHash;
			}

			/**
			 * @brief Destroys the held value; the object is empty afterwards.
			 */
			void
			reset () noexcept
			{
				if ( m_manager != nullptr )
				{
					m_manager(Action::Destroy, *this, nullptr, nullptr);

					m_manager = nullptr;
					m_object = nullptr;
					m_typeHash = 0;
				}
			}

			/**
			 * @brief Returns the held value when it is of the requested type.
			 * @tparam value_t The requested type (cv and reference ignored).
			 * @return const std::remove_cvref_t< value_t > * The value, or nullptr (empty, or another type).
			 */
			template< typename value_t >
			[[nodiscard]]
			const std::remove_cvref_t< value_t > *
			get () const noexcept
			{
				if ( m_manager == nullptr || m_typeHash != typeHashOf< value_t >() )
				{
					return nullptr;
				}

				return static_cast< const std::remove_cvref_t< value_t > * >(m_object);
			}

			/**
			 * @copydoc get() const
			 */
			template< typename value_t >
			[[nodiscard]]
			std::remove_cvref_t< value_t > *
			get () noexcept
			{
				if ( m_manager == nullptr || m_typeHash != typeHashOf< value_t >() )
				{
					return nullptr;
				}

				return static_cast< std::remove_cvref_t< value_t > * >(m_object);
			}

		private:

			/** @brief What the manager of a held type is asked to do. */
			enum class Action : uint8_t
			{
				Copy,
				Move,
				Destroy
			};

			/** @brief The lifetime operations of one held type, behind one function pointer (no static table: nothing to
			 * initialise). The type IDENTITY is m_typeHash, never this function's address (it differs between the
			 * shared engine and an executable). */
			using Manager = void (*) (Action action, Any & target, const Any * copySource, Any * moveSource) noexcept;

			/**
			 * @brief Returns whether a type is stored inline.
			 * @tparam value_t The held type.
			 * @return bool
			 */
			template< typename value_t >
			[[nodiscard]]
			static
			consteval
			bool
			isInline () noexcept
			{
				return sizeof(value_t) <= SmallBufferSize && alignof(value_t) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v< value_t >;
			}

			/**
			 * @brief The manager of one held type.
			 * @tparam value_t The held type.
			 * @param action What to do.
			 * @param target Copy and Move: the destination (an empty Any). Destroy: the holder of the value.
			 * @param copySource Copy: the source. nullptr otherwise.
			 * @param moveSource Move: the source (left empty). nullptr otherwise.
			 */
			template< typename value_t >
			static
			void
			manage (Action action, Any & target, const Any * copySource, Any * moveSource) noexcept
			{
				switch ( action )
				{
					case Action::Copy :
						target.construct< value_t >(*static_cast< const value_t * >(copySource->m_object));
						break;

					case Action::Move :
						if constexpr ( isInline< value_t >() )
						{
							target.construct< value_t >(std::move(*static_cast< value_t * >(moveSource->m_object)));

							moveSource->reset();
						}
						else
						{
							/* A heap value changes owner: no copy, no allocation. */
							target.m_object = std::exchange(moveSource->m_object, nullptr);
							target.m_manager = std::exchange(moveSource->m_manager, nullptr);
							target.m_typeHash = std::exchange(moveSource->m_typeHash, 0);
						}
						break;

					case Action::Destroy :
						if constexpr ( isInline< value_t >() )
						{
							static_cast< value_t * >(target.m_object)->~value_t();
						}
						else
						{
							delete static_cast< value_t * >(target.m_object);
						}
						break;
				}
			}

			/**
			 * @brief Constructs the held value (the object is empty).
			 * @tparam value_t The held type.
			 * @tparam argument_t The constructor argument's type.
			 * @param argument The constructor argument.
			 */
			template< typename value_t, typename argument_t >
			void
			construct (argument_t && argument) noexcept
			{
				if constexpr ( isInline< value_t >() )
				{
					m_object = ::new (static_cast< void * >(m_buffer.data())) value_t(std::forward< argument_t >(argument));
				}
				else
				{
					m_object = new value_t(std::forward< argument_t >(argument));
				}

				m_manager = &Any::manage< value_t >;
				m_typeHash = typeHashOf< value_t >();
			}

			alignas(std::max_align_t) std::array< std::byte, SmallBufferSize > m_buffer{};
			void * m_object{nullptr};
			Manager m_manager{nullptr};
			size_t m_typeHash{0};
	};

	/**
	 * @brief Reads an Any WITHOUT EVER THROWING: the pointer form of std::any_cast.
	 * @tparam value_t The requested type (cv and reference ignored).
	 * @param any A pointer to the value (nullptr allowed).
	 * @return const std::remove_cvref_t< value_t > * The value, or nullptr (no Any, empty, or another type).
	 */
	template< typename value_t >
	[[nodiscard]]
	const std::remove_cvref_t< value_t > *
	anyCast (const Any * any) noexcept
	{
		return any != nullptr ? any->get< value_t >() : nullptr;
	}

	/**
	 * @copydoc anyCast(const Any *)
	 */
	template< typename value_t >
	[[nodiscard]]
	std::remove_cvref_t< value_t > *
	anyCast (Any * any) noexcept
	{
		return any != nullptr ? any->get< value_t >() : nullptr;
	}
}
