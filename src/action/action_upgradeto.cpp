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
/**@name action_upgradeto.cpp - The unit upgrading to new action. */
//
//      (c) Copyright 1998-2015 by Lutz Sammer, Jimmy Salmon and Andrettin
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

#include "action/action_upgradeto.h"

#include "ai.h"
#include "animation.h"
#include "iolib.h"
#include "map.h"
#include "player.h"
#include "script.h"
#include "spells.h"
#include "translate.h"
#include "unit.h"
#include "unittype.h"

/// How many resources the player gets back if canceling upgrade
#define CancelUpgradeCostsFactor   100


/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

std::unique_ptr<COrder> COrder::NewActionTransformInto(CUnitType &type) /* override */
{
	auto order = std::make_unique<COrder_TransformInto>();

	order->Type = &type;
	return order;
}

std::unique_ptr<COrder> COrder::NewActionUpgradeTo(CUnit &unit, CUnitType &type, bool instant) /* override */
{
	auto order = std::make_unique<COrder_UpgradeTo>();

	// FIXME: if you give quick an other order, the resources are lost!
	unit.Player->SubUnitType(type);
	order->Type = &type;
	order->Instant = instant;
	return order;
}

/**
**  Transform a unit in another.
**
**  @param unit     unit to transform.
**  @param newtype  new type of the unit.
**
**  @return 0 on error, 1 if nothing happens, 2 else.
*/
static int TransformUnitIntoType(CUnit &unit, const CUnitType &newtype)
{
	bool removed = false;
	const CUnitType &oldtype = *unit.Type;
	if (&oldtype == &newtype) { // nothing to do
		return 1;
	}
	const Vec2i pos = unit.tilePos + oldtype.GetHalfTileSize() - newtype.GetHalfTileSize();
	CUnit *container = unit.Container;

	if (container) {
		MapUnmarkUnitSight(unit);
	} else {
		// shortcut for quicker checking
		if (unit.Type->FieldFlags == newtype.FieldFlags &&
			unit.Type->MovementMask == newtype.MovementMask &&
			unit.Type->TileHeight == newtype.TileHeight &&
			unit.Type->TileWidth == newtype.TileWidth) {
			// the unit is the same size and has the same flags,
			// it can be here
		} else {
			// the new type is sufficiently different, we check
			// by removing the unit and looking at the map fields
			removed = true;
			SaveSelection();
			unit.Remove(nullptr);
			if (!UnitTypeCanBeAt(newtype, pos)) {
				unit.Place(unit.tilePos);
				RestoreSelection();
				return 0;
			}
		}
	}
	CPlayer &player = *unit.Player;
	player.UnitTypesCount[oldtype.Slot]--;
	player.UnitTypesCount[newtype.Slot]++;
	if (unit.Active) {
		player.UnitTypesAiActiveCount[oldtype.Slot]--;
		player.UnitTypesAiActiveCount[newtype.Slot]++;
	}

	player.Demand += newtype.Stats[player.Index].Variables[DEMAND_INDEX].Value - oldtype.Stats[player.Index].Variables[DEMAND_INDEX].Value;
	player.Supply += newtype.Stats[player.Index].Variables[SUPPLY_INDEX].Value - oldtype.Stats[player.Index].Variables[SUPPLY_INDEX].Value;

	// Change resource limit
	for (int i = 0; i < MaxCosts; ++i) {
		if (player.MaxResources[i] != -1) {
			player.MaxResources[i] += newtype.Stats[player.Index].Storing[i] - oldtype.Stats[player.Index].Storing[i];
			player.SetResource(i, player.StoredResources[i], STORE_BUILDING);
		}
	}

	//  adjust Variables with percent.
	const CUnitStats &newstats = newtype.Stats[player.Index];

	for (unsigned int i = 0; i < UnitTypeVar.GetNumberVariable(); ++i) {
		if (i == KILL_INDEX || i == XP_INDEX) {
			unit.Variable[i].Value = unit.Variable[i].Max;
		} else if (unit.Variable[i].Max && unit.Variable[i].Value) {
			unit.Variable[i].Value = newstats.Variables[i].Max *
									 unit.Variable[i].Value / unit.Variable[i].Max;
			unit.Variable[i].Max = std::max(newstats.Variables[i].Max, unit.Variable[i].Max);
			unit.Variable[i].Increase = newstats.Variables[i].Increase;
			unit.Variable[i].IncreaseFrequency = newstats.Variables[i].IncreaseFrequency;
			unit.Variable[i].Enable = newstats.Variables[i].Enable;
		} else {
			unit.Variable[i].Value = newstats.Variables[i].Value;
			unit.Variable[i].Max = newstats.Variables[i].Max;
			unit.Variable[i].Enable = newstats.Variables[i].Enable;
		}
	}

	unit.Type = const_cast<CUnitType *>(&newtype);
	unit.Stats = const_cast<CUnitStats *>(&unit.Type->Stats[player.Index]);

	if (!newtype.CanCastSpell.empty() && unit.AutoCastSpell.empty()) {
		unit.AutoCastSpell.resize(SpellTypeTable.size());
		unit.SpellCoolDownTimers.resize(SpellTypeTable.size());
	}

	UpdateForNewUnit(unit, 1);
	//  Update Possible sight range change
	UpdateUnitSightRange(unit);
	if (!container) {
		if (removed) {
			unit.Place(pos);
			RestoreSelection();
		}
	} else {
		MapMarkUnitSight(unit);
	}
	//
	// Update possible changed buttons.
	//
	if (IsOnlySelected(unit) || &player == ThisPlayer) {
		// could affect the buttons of any selected unit
		SelectedUnitChanged();
	}
	return 1;
}


#if 1 // TransFormInto

void COrder_TransformInto::Save(CFile &file, const CUnit &unit) const /* override */
{
	file.printf("{\"action-transform-into\",");
	if (this->Finished) {
		file.printf(" \"finished\", ");
	}
	file.printf(" \"type\", \"%s\"", this->Type->Ident.c_str());
	file.printf("}");
}

bool COrder_TransformInto::ParseSpecificData(lua_State *l,
                                             int &j,
                                             std::string_view value,
                                             const CUnit &unit) /* override */
{
	if (value == "type") {
		++j;
		this->Type = &UnitTypeByIdent(LuaToString(l, -1, j + 1));
	} else {
		return false;
	}
	return true;
}

bool COrder_TransformInto::IsValid() const /* override */
{
	return true;
}


PixelPos COrder_TransformInto::Show(const CViewport &,
                                    const PixelPos &lastScreenPos) const /* override */
{
	return lastScreenPos;
}


void COrder_TransformInto::Execute(CUnit &unit) /* override */
{
	TransformUnitIntoType(unit, *this->Type);
	this->Finished = true;
}

#endif

#if 1  //  COrder_UpgradeTo

void COrder_UpgradeTo::Save(CFile &file, const CUnit &unit) const /* override */
{
	file.printf("{\"action-upgrade-to\",");
	if (this->Finished) {
		file.printf(" \"finished\", ");
	}
	file.printf(" \"type\", \"%s\",", this->Type->Ident.c_str());
	file.printf(" \"ticks\", %d", this->Ticks);
	file.printf("}");
}

bool COrder_UpgradeTo::ParseSpecificData(lua_State *l,
                                         int &j,
                                         std::string_view value,
                                         const CUnit &unit) /* override */
{
	if (value == "type") {
		++j;
		this->Type = &UnitTypeByIdent(LuaToString(l, -1, j + 1));
	} else if (value == "ticks") {
		++j;
		this->Ticks = LuaToNumber(l, -1, j + 1);
	} else {
		return false;
	}
	return true;
}

bool COrder_UpgradeTo::IsValid() const /* override */
{
	return true;
}

PixelPos COrder_UpgradeTo::Show(const CViewport &,
                                const PixelPos &lastScreenPos) const /* override */
{
	return lastScreenPos;
}


static void AnimateActionUpgradeTo(CUnit &unit)
{
	CAnimations &animations = *unit.Type->Animations;

	UnitShowAnimation(unit, !animations.Upgrade.empty() ? &animations.Upgrade : &animations.Still);
}

void COrder_UpgradeTo::Execute(CUnit &unit) /* override */
{
	AnimateActionUpgradeTo(unit);
	if (unit.Wait) {
		unit.Wait--;
		return ;
	}
	CPlayer &player = *unit.Player;
	const CUnitType &newtype = *this->Type;
	const CUnitStats &newstats = newtype.Stats[player.Index];

	this->Ticks += std::max(1, player.SpeedUpgrade / SPEEDUP_FACTOR);
	if (!this->Instant && this->Ticks < newstats.Costs[TimeCost]) {
		unit.Wait = CYCLES_PER_SECOND / 6;
		return ;
	}

	if (unit.Anim.Unbreakable) {
		this->Ticks = this->Instant ? 0 : newstats.Costs[TimeCost];
		return ;
	}

	if (TransformUnitIntoType(unit, newtype) == 0) {
		player.Notify(ColorYellow, unit.tilePos, _("Upgrade to %s canceled"), newtype.Name.c_str());
		// give resources back
		player.AddCosts(newstats.Costs);
		this->Finished = true;
		return ;
	}
	player.Notify(ColorGreen, unit.tilePos, _("Upgrade to %s complete"), unit.Type->Name.c_str());

	//  Warn AI.
	if (player.AiEnabled) {
		AiUpgradeToComplete(unit, newtype);
	}
	this->Finished = true;
}

void COrder_UpgradeTo::Cancel(CUnit &unit) /* override */
{
	CPlayer &player = *unit.Player;

	player.AddCostsFactor(this->Type->Stats[player.Index].Costs, CancelUpgradeCostsFactor);
}

void COrder_UpgradeTo::UpdateUnitVariables(CUnit &unit) const /* override */
{
	Assert(unit.CurrentOrder() == this);

	unit.Variable[UPGRADINGTO_INDEX].Value = this->Ticks;
	unit.Variable[UPGRADINGTO_INDEX].Max = this->Type->Stats[unit.Player->Index].Costs[TimeCost];
}

#endif
//@}
