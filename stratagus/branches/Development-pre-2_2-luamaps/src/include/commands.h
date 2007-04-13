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
/**@name commands.h - The commands header file. */
//
//      (c) Copyright 1998-2004 by Lutz Sammer
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
//      $Id$

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "unit.h"

/*----------------------------------------------------------------------------
--  Declarations
----------------------------------------------------------------------------*/

typedef enum _replay_type_ {
	ReplayNone,          ///< No replay
	ReplaySinglePlayer,  ///< Single player replay
	ReplayMultiPlayer,   ///< Multi player replay
} ReplayType;            ///< Replay types

struct _CL_File_;
struct _unit_;
struct _unit_type_;
struct _upgrade_;

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

extern int CommandLogDisabled;     ///< True, if command log is off
extern ReplayType ReplayGameType;  ///< Replay game type

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

	/// Log commands into file
void CommandLog(const char* action, const struct _unit_* unit, int flush,
	int x, int y, const struct _unit_* dest, const char* value, int num);
	/// Replay user commands from log each cycle, single player games
extern void SinglePlayerReplayEachCycle(void);
	/// Replay user commands from log each cycle, multiplayer games
extern void MultiPlayerReplayEachCycle(void);
	/// Load replay
extern int LoadReplay(char*);
	/// End logging
extern void EndReplayLog(void);
	/// Clean replay
extern void CleanReplayLog(void);
	/// Save the replay list to file
extern void SaveReplayList(struct _CL_File_* file);

/*
**  The send command functions sends a command, if needed over the
**  Network, this is only for user commands. Automatic reactions which
**  are on all computers equal, should use the functions without Send.
*/

	/// Send stop command
extern void SendCommandStopUnit(struct _unit_* unit);
	/// Send stand ground command
extern void SendCommandStandGround(struct _unit_* unit, int flush);
	/// Send follow command
extern void SendCommandFollow(struct _unit_* unit, struct _unit_* dest,
	int flush);
	/// Send move command
extern void SendCommandMove(struct _unit_* unit, int x, int y, int flush);
	/// Send repair command
extern void SendCommandRepair(struct _unit_* unit, int x, int y,
	struct _unit_* dest, int flush);
	/// Send auto repair command
extern void SendCommandAutoRepair(struct _unit_* unit, int on);
	/// Send attack command
extern void SendCommandAttack(struct _unit_* unit, int x, int y,
	struct _unit_* dest, int flush);
	/// Send attack ground command
extern void SendCommandAttackGround(struct _unit_* unit, int x, int y,
	int flush);
	/// Send patrol command
extern void SendCommandPatrol(struct _unit_* unit, int x, int y, int flush);
	/// Send board command
extern void SendCommandBoard(struct _unit_* unit, int x, int y,
	struct _unit_* dest, int flush);
	/// Send unload command
extern void SendCommandUnload(struct _unit_* unit, int x, int y,
	struct _unit_* what, int flush);
	/// Send build building command
extern void SendCommandBuildBuilding(struct _unit_* unit, int x, int y,
	struct _unit_type_* what, int flush);
	/// Send cancel building command
extern void SendCommandDismiss(struct _unit_* unit);
	/// Send harvest location command
extern void SendCommandResourceLoc(struct _unit_* unit, int x, int y,
	int flush);
	/// Send harvest command
extern void SendCommandResource(struct _unit_* unit, struct _unit_* dest,
	int flush);
	/// Send return goods command
extern void SendCommandReturnGoods(struct _unit_* unit, struct _unit_* dest,
	int flush);
	/// Send train command
extern void SendCommandTrainUnit(struct _unit_* unit,
	struct _unit_type_* what, int flush);
	/// Send cancel training command
extern void SendCommandCancelTraining(struct _unit_* unit, int slot,
	const struct _unit_type_* type);
	/// Send upgrade to command
extern void SendCommandUpgradeTo(struct _unit_* unit,
	struct _unit_type_* what, int flush);
	/// Send cancel upgrade to command
extern void SendCommandCancelUpgradeTo(struct _unit_* unit);
	/// Send research command
extern void SendCommandResearch(struct _unit_* unit, struct _upgrade_* what,
	int flush);
	/// Send cancel research command
extern void SendCommandCancelResearch(struct _unit_* unit);
	/// Send spell cast command
extern void SendCommandSpellCast(struct _unit_* unit, int x, int y,
	struct _unit_* dest, int spellid, int flush);
	/// Send auto spell cast command
extern void SendCommandAutoSpellCast(struct _unit_* unit, int spellid,
	int on);
	/// Send diplomacy command
extern void SendCommandDiplomacy(int player, int state, int opponent);
	/// Send shared vision command
extern void SendCommandSharedVision(int player, int state, int opponent);

	/// Parse a command (from network).
extern void ParseCommand(unsigned char type, UnitRef unum, unsigned short x,
	unsigned short y, UnitRef dest);
	/// Parse an extended command (from network).
extern void ParseExtendedCommand(unsigned char type, int status,
	unsigned char arg1, unsigned short arg2, unsigned short arg3,
	unsigned short arg4);

//@}

#endif // !__COMMANDS_H__