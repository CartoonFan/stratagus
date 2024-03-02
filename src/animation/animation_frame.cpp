//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name animation_frame.cpp - The animation Frame. */
//
//      (c) Copyright 2012-2015 by Joris Dauphin and Andrettin
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "animation/animation_frame.h"

#include "ui.h"
#include "unit.h"

void CAnimation_Frame::Action(CUnit &unit, int &/*move*/, int /*scale*/) const /* override */
{
	Assert(unit.Anim.CurrAnim);
	Assert((*unit.Anim.CurrAnim)[unit.Anim.Anim].get() == this);
	if (unit.Type->Building && unit.Type->NumDirections == 1 && FancyBuildings && unit.Type->BoolFlag[NORANDOMPLACING_INDEX].value == false && unit.Frame < 0) {
	} else {
		unit.Frame = ParseAnimInt(&unit);
	}
	UnitUpdateHeading(unit);
}

void CAnimation_Frame::Init(std::string_view s, lua_State *) /* override */
{
	this->frame = s;
}

std::optional<int> CAnimation_Frame::GetStillFrame(const CUnitType &type) /* override */
{
	return ParseAnimInt(nullptr) + type.NumDirections / 2;
}

int CAnimation_Frame::ParseAnimInt(const CUnit *unit) const
{
	if (unit == nullptr) {
		return to_number(this->frame);
	} else {
		return ::ParseAnimInt(*unit, this->frame);
	}
}

//@}
