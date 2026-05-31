/*
 * src/INIParser.cpp
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

#include "INIParser.hpp"

/* STL inclusions. */
#include <fstream>

namespace EmEn::Base
{
	void
	INISection::write (std::ofstream & file) const noexcept
	{
		for ( const auto & [name, variable] : m_variables )
		{
			file << name << " = " << variable.asString() << "\n";
		}
	}

	std::string
	INIParser::parseSectionTitle (std::string_view line) noexcept
	{
		const auto start = line.find_first_of('[');
		const auto end = line.find_last_of(']');

		if ( start != std::string_view::npos && end != std::string_view::npos && end > start )
		{
			return std::string{line.substr(start + 1, end - start - 1)};
		}

		return {};
	}

	INIParser::LineType
	INIParser::getLineType (std::string_view line) noexcept
	{
		/* Classify by the FIRST non-whitespace character: only a line that *starts* with a
		 * marker is a header/comment/section. Any other line containing '=' is a definition,
		 * so a key may legitimately contain '[', '#' or '@' (e.g. "arr[0] = 5") without being
		 * misclassified and silently dropped. */
		const auto firstPosition = line.find_first_not_of(" \t\r\n\f\v");

		if ( firstPosition == std::string_view::npos )
		{
			return LineType::None;
		}

		switch ( line[firstPosition] )
		{
			case '@' :
				return LineType::Headers;

			case '[' :
				return LineType::SectionTitle;

			case '#' :
				return LineType::Comment;

			default :
				break;
		}

		if ( line.find('=') != std::string_view::npos )
		{
			return LineType::Definition;
		}

		return LineType::None;
	}

	INISection &
	INIParser::section (std::string_view label) noexcept
	{
		return m_sections.try_emplace(std::string{label}).first->second;
	}

	bool
	INIParser::read (const std::filesystem::path & filepath) noexcept
	{
		std::ifstream file{filepath};

		if ( !file.is_open() )
		{
			return false;
		}

		std::string line;

		/* This is the default section. */
		auto * currentSection = &this->section("main");

		/* Parse all lines. */
		while ( std::getline(file, line) )
		{
			switch ( INIParser::getLineType(line) )
			{
				case LineType::SectionTitle :
					if ( auto sectionName = INIParser::parseSectionTitle(line); !sectionName.empty() )
					{
						currentSection = &this->section(sectionName);
					}
					break;

				case LineType::Definition :
					if ( auto equalSignPosition = line.find_first_of('='); equalSignPosition != std::string::npos )
					{
						auto key = String::trim(line.substr(0, equalSignPosition));
						auto value = String::trim(line.substr(equalSignPosition + 1));

						currentSection->addVariable(key, INIVariable{value});
					}
					break;

				case LineType::None :
				case LineType::Headers :
				case LineType::Comment :
					break;
			}
		}

		return true;
	}

	bool
	INIParser::write (const std::filesystem::path & filepath) const noexcept
	{
		std::ofstream file{filepath, std::ios::out | std::ios::trunc};

		if ( !file.is_open() )
		{
			return false;
		}

		for ( const auto & [sectionName, section] : m_sections )
		{
			file << "[" << sectionName << "]" "\n";

			for ( const auto & [variableName, variable] : section.variables() )
			{
				file << variableName << " = " << variable.asString() << "\n";
			}

			file << "\n";
		}

		return true;
	}
}
