/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/
#pragma once

#include <type_traits>

// To Use This feature, declare your enum as an enum class, and then define
// the template specialisation for FlagTraits to enable the bitmask features
// E.g
// enum class FeatureListFlagBits : uint32_t
// {
// 		None = 0x0,
//		FeatureOne = 0x1,
// 		FeatureTwo = 0x2,
// };
// 
// Then the following definition will create `FeatureListFlags`
// ENABLE_BITMASK_FLAGS(FeatureList);
// 
// Then you can use usual bitmask operators to work with them
// FeatureListFlags f = FeatureListFlagBits::FeatureOne | FeatureListFlagBits::FeatureTwo;

namespace TD
{
template<typename FlagBitsType>
struct FlagTraits
{
	static constexpr bool isBitmask = false;
};

template<typename BitType>
class Flags
{
public:
	using BitsType = BitType;
	using MaskType = typename std::underlying_type<BitType>::type;

	constexpr Flags() noexcept :
		mask(0)
	{
	}

	constexpr Flags(BitType bit) noexcept :
		mask(static_cast<MaskType>(bit))
	{
	}

	constexpr Flags(Flags<BitType> const& rhs) noexcept = default;

	constexpr explicit Flags(MaskType flags) noexcept :
		mask(flags)
	{
	}

	constexpr bool
	operator<(Flags<BitType> const& rhs) const noexcept
	{
		return mask < rhs.mask;
	}

	constexpr bool
	operator<=(Flags<BitType> const& rhs) const noexcept
	{
		return mask <= rhs.mask;
	}

	constexpr bool
	operator>(Flags<BitType> const& rhs) const noexcept
	{
		return mask > rhs.mask;
	}

	constexpr bool
	operator>=(Flags<BitType> const& rhs) const noexcept
	{
		return mask >= rhs.mask;
	}

	constexpr bool
	operator==(Flags<BitType> const& rhs) const noexcept
	{
		return mask == rhs.mask;
	}

	constexpr bool
	operator!=(Flags<BitType> const& rhs) const noexcept
	{
		return mask != rhs.mask;
	}

	// logical operator
	constexpr bool
	operator!() const noexcept
	{
		return !mask;
	}

	// bitwise operators
	constexpr Flags<BitType>
	operator&(Flags<BitType> const& rhs) const noexcept
	{
		return Flags<BitType>(mask & rhs.mask);
	}

	constexpr Flags<BitType>
	operator|(Flags<BitType> const& rhs) const noexcept
	{
		return Flags<BitType>(mask | rhs.mask);
	}

	constexpr Flags<BitType>
	operator^(Flags<BitType> const& rhs) const noexcept
	{
		return Flags<BitType>(mask ^ rhs.mask);
	}

	constexpr Flags<BitType>
	operator~() const noexcept
	{
		return Flags<BitType>(mask ^ FlagTraits<BitType>::allFlags.mask);
	}

	// assignment operators
	Flags<BitType>& operator=(Flags<BitType> const& rhs) noexcept = default;

	constexpr Flags<BitType>&
	operator|=(Flags<BitType> const& rhs) noexcept
	{
		mask |= rhs.mask;
		return *this;
	}

	constexpr Flags<BitType>&
	operator&=(Flags<BitType> const& rhs) noexcept
	{
		mask &= rhs.mask;
		return *this;
	}

	constexpr Flags<BitType>&
	operator^=(Flags<BitType> const& rhs) noexcept
	{
		mask ^= rhs.mask;
		return *this;
	}

	// cast operators
	explicit constexpr
	operator bool() const noexcept
	{
		return !!mask;
	}

	explicit constexpr
	operator MaskType() const noexcept
	{
		return mask;
	}

private:
	MaskType mask;
};

template<typename BitType>
constexpr bool
operator<(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator>(bit);
}

template<typename BitType>
constexpr bool
operator<=(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator>=(bit);
}

template<typename BitType>
constexpr bool
operator>(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator<(bit);
}

template<typename BitType>
constexpr bool
operator>=(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator<=(bit);
}

template<typename BitType>
constexpr bool
operator==(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator==(bit);
}

template<typename BitType>
constexpr bool
operator!=(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator!=(bit);
}

// bitwise operators
template<typename BitType>
constexpr Flags<BitType>
operator&(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator&(bit);
}

template<typename BitType>
constexpr Flags<BitType>
operator|(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator|(bit);
}

template<typename BitType>
constexpr Flags<BitType>
operator^(BitType bit, Flags<BitType> const& flags) noexcept
{
	return flags.operator^(bit);
}

// bitwise operators on BitType
template<typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType>
operator&(BitType lhs, BitType rhs) noexcept
{
	return Flags<BitType>(lhs) & rhs;
}

template<typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType>
operator|(BitType lhs, BitType rhs) noexcept
{
	return Flags<BitType>(lhs) | rhs;
}

template<typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType>
operator^(BitType lhs, BitType rhs) noexcept
{
	return Flags<BitType>(lhs) ^ rhs;
}

template<typename BitType, typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType>
operator~(BitType bit) noexcept
{
	return ~(Flags<BitType>(bit));
}
}  // namespace TD

#define TD_ENABLE_BITMASK_FLAGS(x)			\
using x##Flags = Flags<x##FlagBits>;		\
template<>									\
struct FlagTraits<x##FlagBits>				\
{											\
	static constexpr bool isBitmask = true; \
};											\
