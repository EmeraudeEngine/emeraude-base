/*
 * src/Testing/test_MD5AnimParser.cpp
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

/* Third-party inclusions. */
#include <gtest/gtest.h>

/* STL inclusions. */
#include <sstream>
#include <string>

/* Local inclusions. */
#include "Animation/MD5AnimParser.hpp"

/* Robustness characterization for the MD5 animation parser (Ave robustus! — A.3, I/O boundary
 * hardening). The parser consumes untrusted .md5anim text; malformed/truncated input must
 * degrade gracefully (empty clip), never crash. Verified meaningfully under ASan/UBSan via
 * ctest — e.g. the empty-joint-name regression below was undefined behaviour (front()/back()
 * on an empty string) before the size>=2 guard. */

namespace EmEn::Base::Animation
{
	namespace
	{
		using Parser = MD5AnimParser< float >;
	}

	TEST(MD5AnimParser, emptyStreamYieldsEmptyClip)
	{
		std::istringstream stream{""};

		const auto clip = Parser::parseStream(stream, "test");

		EXPECT_TRUE(clip.empty());
	}

	TEST(MD5AnimParser, truncatedHierarchyEmptyJointNameDoesNotCrash)
	{
		/* numJoints announces one joint, but the joint line is blank: `hs >> name` leaves
		 * `name` empty, so name.front()/back() used to be UB. The parse must complete. */
		const std::string md5 =
			"numFrames 0\n"
			"numJoints 1\n"
			"frameRate 24\n"
			"numAnimatedComponents 0\n"
			"hierarchy {\n"
			"\n"
			"}\n";

		std::istringstream stream{md5};

		const auto clip = Parser::parseStream(stream, "truncated");

		/* No frames -> no channels; the contract verified under ASan/UBSan is "no crash". */
		EXPECT_TRUE(clip.empty());
	}

	TEST(MD5AnimParser, singleQuoteJointNameDoesNotCrash)
	{
		/* A one-character `"` token: front()==back()=='"' but size<2, so the quote-strip must
		 * be skipped (otherwise substr(1, size-2) underflows the length). */
		const std::string md5 =
			"numFrames 0\n"
			"numJoints 1\n"
			"frameRate 24\n"
			"numAnimatedComponents 0\n"
			"hierarchy {\n"
			"\"\n"
			"}\n";

		std::istringstream stream{md5};

		const auto clip = Parser::parseStream(stream, "weird");

		EXPECT_TRUE(clip.empty());
	}
	/* ⚠️⚠️ REGRESSION PIN (Aug 2026) — the MD5 -> engine conversion must be a ROTATION (det +1),
	 * never a reflection. This parser was a FIFTH conversion site the Y-up migration missed: it
	 * kept (md5.y, -md5.z, md5.x), det -1, while the MESH path in VertexFactory/FileFormatMDx.hpp
	 * had already moved to (md5.y, md5.z, md5.x). A clip then lived in a mirrored frame relative
	 * to the mesh it animated, and NO geometric assertion could see it.
	 * Both halves are pinned here because they must move TOGETHER: converting positions while
	 * leaving orientations on the old mirror renders a skinned MD5 upside down. */
	TEST(MD5AnimParser, conversionIsARotationNotAReflection)
	{
		/* One joint, one frame, flags 0 so the baseframe values are used verbatim. Position
		 * (1, 2, 3) in MD5 space, and an orientation about the MD5 X axis.
		 * ⚠️⚠️ The two halves need DIFFERENT discriminating axes, and getting that wrong makes
		 * half the test vacuous:
		 *  - POSITION: only md5 Z tells the transforms apart (md5 X and md5 Y map identically
		 *    under both), and it is carried here by the pos triplet's third component.
		 *  - ROTATION: md5 Z is exactly the axis that CANNOT tell them apart. The retired
		 *    reflection negates engine Y, and conjugating a rotation by a reflection that negates
		 *    the rotation's own axis leaves it unchanged: S*R(Y,angle)*S^T == R(Y,angle). An
		 *    orientation about md5 Z was verified to pass against the reflection. Rotating about
		 *    md5 X (-> engine Z) instead flips the rotation SENSE, which is what actually broke. */
		const std::string md5 =
			"numFrames 1\n"
			"numJoints 1\n"
			"frameRate 24\n"
			"numAnimatedComponents 0\n"
			"hierarchy {\n"
			"\"root\" -1 0 0\n"
			"}\n"
			"baseframe {\n"
			"( 1 2 3 ) ( 0.5 0 0 )\n"
			"}\n"
			"frame 0 {\n"
			"}\n";

		std::istringstream stream{md5};

		const auto clip = Parser::parseStream(stream, "conversion-pin");

		const auto & channels = clip.channels();

		ASSERT_EQ(channels.size(), 2U);

		const AnimationChannel< float > * translation = nullptr;
		const AnimationChannel< float > * rotation = nullptr;

		for ( const auto & channel : channels )
		{
			if ( channel.target == ChannelTarget::Translation ) { translation = &channel; }
			if ( channel.target == ChannelTarget::Rotation ) { rotation = &channel; }
		}

		ASSERT_NE(translation, nullptr);
		ASSERT_NE(rotation, nullptr);
		ASSERT_EQ(translation->vectorKeyFrames.size(), 1U);
		ASSERT_EQ(rotation->quaternionKeyFrames.size(), 1U);

		/* POSITION: (md5.y, md5.z, md5.x) * IDTechUnitScale = (2, 3, 1) * 0.01.
		 * ⚠️ Y is the pin: the retired reflection produced -0.03F here. */
		const auto & position = translation->vectorKeyFrames[0].value;

		EXPECT_NEAR(position[Math::X], 0.02F, 1e-5F);
		EXPECT_NEAR(position[Math::Y], 0.03F, 1e-5F);
		EXPECT_NEAR(position[Math::Z], 0.01F, 1e-5F);

		/* ROTATION: md5 (0.5, 0, 0) with the parser's negative w is a -60 degree turn about the
		 * md5 X axis, which maps to engine +Z. Probing with a rotated vector rather than reading
		 * the quaternion components keeps this independent of the (q, -q) sign the
		 * matrix-to-quaternion extraction happens to pick.
		 * Engine +X turned -60 degrees about +Z lands at (0.5, -0.866, 0). Under the retired
		 * reflection the SENSE reverses and Y comes out POSITIVE — that reversal is the defect
		 * that used to get blamed on the animation blender. */
		const auto & orientation = rotation->quaternionKeyFrames[0].value;
		const auto probe = orientation.rotatedVector({1.0F, 0.0F, 0.0F});

		EXPECT_NEAR(probe[Math::X], 0.5F, 1e-3F);
		EXPECT_LT(probe[Math::Y], -0.5F);
		EXPECT_NEAR(probe[Math::Z], 0.0F, 1e-3F);
	}
}