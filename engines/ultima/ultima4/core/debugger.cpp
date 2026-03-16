/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ultima/ultima4/core/debugger.h"
#include "ultima/ultima4/core/utils.h"
#include "ultima/ultima4/controllers/alpha_action_controller.h"
#include "ultima/ultima4/controllers/camp_controller.h"
#include "ultima/ultima4/controllers/read_choice_controller.h"
#include "ultima/ultima4/controllers/read_dir_controller.h"
#include "ultima/ultima4/controllers/ztats_controller.h"
#include "ultima/ultima4/game/armor.h"
#include "ultima/ultima4/game/context.h"
#include "ultima/ultima4/game/game.h"
#include "ultima/ultima4/game/item.h"
#include "ultima/ultima4/game/moongate.h"
#include "ultima/ultima4/game/player.h"
#include "ultima/ultima4/game/portal.h"
#include "ultima/ultima4/game/weapon.h"
#include "ultima/ultima4/gfx/screen.h"
#include "ultima/ultima4/map/annotation.h"
#include "ultima/ultima4/map/city.h"
#include "ultima/ultima4/map/mapmgr.h"
#include "ultima/ultima4/views/dungeonview.h"
#include "ultima/ultima4/views/stats.h"
#include "ultima/ultima4/ultima4.h"
#include "common/system.h"

namespace Ultima {
namespace Ultima4 {

Debugger *g_debugger;

static bool strToBool(const char *s) {
	return s && tolower(*s) == 't';
}

static int strToInt(const char *s) {
	if (!*s)
		// No string at all
		return 0;
	else if (toupper(s[strlen(s) - 1]) != 'H')
		// Standard decimal string
		return atoi(s);

	// Hexadecimal string
	uint tmp = 0;
	int read = sscanf(s, "%xh", &tmp);
	if (read < 1)
		error("strToInt failed on string \"%s\"", s);
	return (int)tmp;
}

static void splitString(const Common::String &str,
		Common::StringArray &argv) {
	// Clear the vector
	argv.clear();

	bool quoted = false;
	Common::String::const_iterator it;
	int ch;
	Common::String arg;

	for (it = str.begin(); it != str.end(); ++it) {
		ch = *it;

		// Toggle quoted string handling
		if (ch == '\"') {
			quoted = !quoted;
			continue;
		}

		// Handle \\, \", \', \n, \r, \t
		if (ch == '\\') {
			Common::String::const_iterator next = it + 1;
			if (next != str.end()) {
				if (*next == '\\' || *next == '\"' || *next == '\'') {
					ch = *next;
					++it;
				} else if (*next == 'n') {
					ch = '\n';
					++it;
				} else if (*next == 'r') {
					ch = '\r';
					++it;
				} else if (*next == 't') {
					ch = '\t';
					++it;
				} else if (*next == ' ') {
					ch = ' ';
					++it;
				}
			}
		}

		// A space, a tab, line feed, carriage return
		if (!quoted && (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) {
			// If we are not empty then we are at the end of the arg
			// otherwise we will ignore the extra chars
			if (!arg.empty()) {
				argv.push_back(arg);
				arg.clear();
			}

			continue;
		}

		// Add the charater to the string
		arg += ch;
	}

	// Push any arg if it's left
	if (!arg.empty())
		argv.push_back(arg);
}

Debugger::Debugger() : GUI::Debugger() {
	g_debugger = this;
	_collisionOverride = false;
	_disableCombat = false;
	_disableHunger = false;
	_dontEndTurn = false;

	registerCmd("move", WRAP_METHOD(Debugger, cmdMove));
	registerCmd("attack", WRAP_METHOD(Debugger, cmdAttack));
	registerCmd("board", WRAP_METHOD(Debugger, cmdBoard));
	registerCmd("camp", WRAP_METHOD(Debugger, cmdCamp));
	registerCmd("cast", WRAP_METHOD(Debugger, cmdCastSpell));
	registerCmd("spell", WRAP_METHOD(Debugger, cmdCastSpell));
	registerCmd("cure", WRAP_METHOD(Debugger, cmdCure));
	registerCmd("heal", WRAP_METHOD(Debugger, cmdHeal));
	registerCmd("climb", WRAP_METHOD(Debugger, cmdClimb));
	registerCmd("descend", WRAP_METHOD(Debugger, cmdDescend));
	registerCmd("enter", WRAP_METHOD(Debugger, cmdEnter));
	registerCmd("exit", WRAP_METHOD(Debugger, cmdExit));
	registerCmd("fire", WRAP_METHOD(Debugger, cmdFire));
	registerCmd("get", WRAP_METHOD(Debugger, cmdGetChest));
	registerCmd("ignite", WRAP_METHOD(Debugger, cmdIgnite));
	registerCmd("interact", WRAP_METHOD(Debugger, cmdInteract));
	registerCmd("jimmy", WRAP_METHOD(Debugger, cmdJimmy));
	registerCmd("locate", WRAP_METHOD(Debugger, cmdLocate));
	registerCmd("mix", WRAP_METHOD(Debugger, cmdMixReagents));
	registerCmd("open", WRAP_METHOD(Debugger, cmdOpenDoor));
	registerCmd("order", WRAP_METHOD(Debugger, cmdNewOrder));
	registerCmd("party", WRAP_METHOD(Debugger, cmdParty));
	registerCmd("pass", WRAP_METHOD(Debugger, cmdPass));
	registerCmd("peer", WRAP_METHOD(Debugger, cmdPeer));
	registerCmd("quitAndSave", WRAP_METHOD(Debugger, cmdQuitAndSave));
	registerCmd("ready", WRAP_METHOD(Debugger, cmdReadyWeapon));
	registerCmd("search", WRAP_METHOD(Debugger, cmdSearch));
	registerCmd("stats", WRAP_METHOD(Debugger, cmdStats));
	registerCmd("talk", WRAP_METHOD(Debugger, cmdTalk));
	registerCmd("use", WRAP_METHOD(Debugger, cmdUse));
	registerCmd("wear", WRAP_METHOD(Debugger, cmdWearArmor));
	registerCmd("yell", WRAP_METHOD(Debugger, cmdYell));

	registerCmd("speed", WRAP_METHOD(Debugger, cmdSpeed));
	registerCmd("combat_speed", WRAP_METHOD(Debugger, cmdCombatSpeed));

	registerCmd("3d", WRAP_METHOD(Debugger, cmd3d));
	registerCmd("abyss", WRAP_METHOD(Debugger, cmdAbyss));
	registerCmd("collisions", WRAP_METHOD(Debugger, cmdCollisions));
	registerCmd("combat", WRAP_METHOD(Debugger, cmdCombat));
	registerCmd("companions", WRAP_METHOD(Debugger, cmdCompanions));
	registerCmd("destroy", WRAP_METHOD(Debugger, cmdDestroy));
	registerCmd("destroy_creatures", WRAP_METHOD(Debugger, cmdDestroyCreatures));
	registerCmd("dungeon", WRAP_METHOD(Debugger, cmdDungeon));
	registerCmd("equipment", WRAP_METHOD(Debugger, cmdEquipment));
	registerCmd("exit", WRAP_METHOD(Debugger, cmdExit));
	registerCmd("flee", WRAP_METHOD(Debugger, cmdFlee));
	registerCmd("fullstats", WRAP_METHOD(Debugger, cmdFullStats));
	registerCmd("gate", WRAP_METHOD(Debugger, cmdGate));
	registerCmd("goto", WRAP_METHOD(Debugger, cmdGoto));
	registerCmd("hunger", WRAP_METHOD(Debugger, cmdHunger));
	registerCmd("items", WRAP_METHOD(Debugger, cmdItems));
	registerCmd("karma", WRAP_METHOD(Debugger, cmdKarma));
	registerCmd("leave", WRAP_METHOD(Debugger, cmdLeave));
	registerCmd("location", WRAP_METHOD(Debugger, cmdLocation));
	registerCmd("lordbritish", WRAP_METHOD(Debugger, cmdLorddBritish));
	registerCmd("mixtures", WRAP_METHOD(Debugger, cmdMixtures));
	registerCmd("moon", WRAP_METHOD(Debugger, cmdMoon));
	registerCmd("opacity", WRAP_METHOD(Debugger, cmdOpacity));
	registerCmd("overhead", WRAP_METHOD(Debugger, cmdOverhead));
	registerCmd("reagents", WRAP_METHOD(Debugger, cmdReagents));
	registerCmd("summon", WRAP_METHOD(Debugger, cmdSummon));
	registerCmd("torch", WRAP_METHOD(Debugger, cmdTorch));
	registerCmd("transport", WRAP_METHOD(Debugger, cmdTransport));
	registerCmd("triggers", WRAP_METHOD(Debugger, cmdListTriggers));
	registerCmd("up", WRAP_METHOD(Debugger, cmdUp));
	registerCmd("down", WRAP_METHOD(Debugger, cmdDown));
	registerCmd("virtue", WRAP_METHOD(Debugger, cmdVirtue));
	registerCmd("wind", WRAP_METHOD(Debugger, cmdWind));

	// Override base class help with our syntax-aware version
	registerCmd("help", WRAP_METHOD(Debugger, cmdHelp));
}

Debugger::~Debugger() {
	g_debugger = nullptr;
}

void Debugger::print(const char *fmt, ...) {
	// Format the string
	va_list va;
	va_start(va, fmt);
	Common::String str = Common::String::vformat(fmt, va);
	va_end(va);

	printN("%s\n", str.c_str());
}

void Debugger::printN(const char *fmt, ...) {
	// Format the string
	va_list va;
	va_start(va, fmt);
	Common::String str = Common::String::vformat(fmt, va);
	va_end(va);

	if (isDebuggerActive()) {
		// Strip off any color special characters that aren't
		// relevant for showing the text in the debugger
		Common::String s;
		for (const auto &c : str) {
			if (c >= ' ' || c == '\n')
				s += c;
		}

		debugPrintf("%s", s.c_str());
	} else {
		g_screen->screenMessage("%s", str.c_str());
	}
}

void Debugger::prompt() {
	if (isDebuggerActive())
		g_screen->screenPrompt();
}

bool Debugger::handleCommand(int argc, const char **argv, bool &keepRunning) {
	static const char *const DUNGEON_DISALLOWED[] = {
		"attack", "board", "enter", "fire", "jimmy", "locate",
		"open", "talk", "exit", "yell", nullptr
	};
	static const char *const COMBAT_DISALLOWED[] = {
		"board", "climb", "descend", "enter", "exit", "fire", "hole",
		"ignite", "jimmy", "mix", "order", "open", "peer", "quitAndSave",
		"search", "wear", "yell", nullptr
	};

	// Built-in framework commands safe to run before a game is loaded.
	// "exit" and "quit" are handled separately: Ultima4 overrides those
	// registrations with its vehicle-exit game command, so we must call
	// detach() directly to close the console instead of going through
	// the command map.
	static const char *const BUILTIN_CMDS[] = {
		"help", "openlog", "md5", "md5mac", "clear", "cls",
		"exec", "debuglevel", "debugflag_list", "debugflag_enable",
		"debugflag_disable", nullptr
	};

	if (!g_context || !g_context->_location) {
		Common::String cmd(argv[0]);
		if (cmd.equalsIgnoreCase("exit") || cmd.equalsIgnoreCase("quit")) {
			detach();
			keepRunning = false;
			return true;
		}
		for (const char *const *c = BUILTIN_CMDS; *c; ++c) {
			if (cmd.equalsIgnoreCase(*c))
				return GUI::Debugger::handleCommand(argc, argv, keepRunning);
		}
		debugPrintf("No game loaded. Load a game before using game commands.\n");
		keepRunning = true;
		return true;
	}

	if (g_context->_location) {
		int ctx = g_context->_location->_context;
		if (ctx & (CTX_DUNGEON | CTX_COMBAT)) {
			Common::String method = argv[0];
			const char *const *mth = (ctx & CTX_COMBAT) ?
				COMBAT_DISALLOWED : DUNGEON_DISALLOWED;

			for (; *mth; ++mth) {
				if (method.equalsIgnoreCase(*mth)) {
					print("%cNot here!%c", FG_GREY, FG_WHITE);
					g_context->_location->_turnCompleter->finishTurn();
					keepRunning = false;
					return true;
				}
			}
		}
	}

	bool result = GUI::Debugger::handleCommand(argc, argv, keepRunning);

	if (result) {
		Controller *ctl = eventHandler->getController();

		if (g_context)
			g_context->_lastCommandTime = g_system->getMillis();

		if (!isActive() && !_dontEndTurn) {
			GameController *gc = dynamic_cast<GameController *>(ctl);
			CombatController *cc = dynamic_cast<CombatController *>(ctl);

			if (gc)
				gc->finishTurn();
			else if (cc)
				cc->finishTurn();
		} else if (_dontEndTurn) {
			if (ctl == g_game || ctl == g_combat) {
				assert(g_context);
				g_context->_location->_turnCompleter->finishTurn();
			}
		}
	}

	_dontEndTurn = false;
	return result;
}

void Debugger::getChest(int player) {
	Common::String param = Common::String::format("%d", player);
	const char *argv[2] = { "get", param.c_str() };

	cmdGetChest(2, argv);
}

bool Debugger::cmdMove(int argc, const char **argv) {
	Direction dir;

	if (argc == 2) {
		dir = directionFromName(argv[1]);
	} else {
		print("move <direction>");
		return isDebuggerActive();
	}

	Common::Path priorMap = g_context->_location->_map->_fname;
	MoveResult retval = g_context->_location->move(dir, true);

	// horse doubles speed (make sure we're on the same map as the previous move first)
	if (retval & (MOVE_SUCCEEDED | MOVE_SLOWED) &&
		(g_context->_transportContext == TRANSPORT_HORSE) && g_context->_horseSpeed) {
		// to give it a smooth look of movement
		gameUpdateScreen();
		if (priorMap == g_context->_location->_map->_fname)
			g_context->_location->move(dir, false);
	}

	// Let the movement handler decide to end the turn
	bool endTurn = (retval & MOVE_END_TURN);
	if (!endTurn)
		dontEndTurn();

	return false;
}

bool Debugger::cmdAttack(int argc, const char **argv) {
	if (argc < 2 && isDebuggerActive()) {
		print("attack <direction> [distance]");
		return true;
	}

	Direction dir = (argc >= 2) ? directionFromName(argv[1]) : DIR_NONE;
	int range = (argc >= 3) ? strToInt(argv[2]) : -1;

	CombatController *cc = dynamic_cast<CombatController *>(eventHandler->getController());
	GameController *gc = dynamic_cast<GameController *>(eventHandler->getController());

	if (cc)
		cc->attack(dir, range);
	else if (gc)
		gc->attack(dir);

	return isDebuggerActive();
}

bool Debugger::cmdBoard(int argc, const char **argv) {
	if (g_context->_transportContext != TRANSPORT_FOOT) {
		print("Board: %cCan't!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	Object *obj = g_context->_location->_map->objectAt(g_context->_location->_coords);
	if (!obj) {
		print("%cBoard What?%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	const Tile *tile = obj->getTile().getTileType();
	if (tile->isShip()) {
		print("Board Frigate!");
		if (g_context->_lastShip != obj)
			g_context->_party->setShipHull(50);
	} else if (tile->isHorse())
		print("Mount Horse!");
	else if (tile->isBalloon())
		print("Board Balloon!");
	else {
		print("%cBoard What?%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	g_context->_party->setTransport(obj->getTile());
	g_context->_location->_map->removeObject(obj);
	return isDebuggerActive();
}

bool Debugger::cmdCastSpell(int argc, const char **argv) {
	int player = -1;
	if (argc >= 2)
		player = strToInt(argv[1]);

	print("Cast Spell!");
	if (isCombat()) {
		player = getCombatFocus();
	} else if (player == -1) {
		printN("Player: ");
		player = gameGetPlayer(false, true);
	}
	if (player == -1)
		return isDebuggerActive();

	if (player < 0 || player >= g_context->_party->size()) {
		print("Invalid player: %d (party has %d member%s, index 0-%d)",
			player, g_context->_party->size(),
			g_context->_party->size() == 1 ? "" : "s",
			g_context->_party->size() - 1);
		return isDebuggerActive();
	}

	// get the spell to cast
	g_context->_stats->setView(STATS_MIXTURES);
	printN("Spell: ");
#ifdef IOS_ULTIMA4
	// ### Put the iPad thing too.
	U4IOS::IOSCastSpellHelper castSpellController;
#endif
	int spell;
	if (argc == 3) {
		printN("Spell: ");
		if (Common::isAlpha(argv[2][0])) {
			spell = tolower(argv[2][0]) - 'a';
		} else {
			spell = -1;
		}
	} else {
		spell = AlphaActionController::get('z', "Spell: ");
	}

	if (spell == -1) {
		print("");
		return isDebuggerActive();
	}

	print("%s!", g_spells->spellGetName(spell)); // Prints spell name at prompt

	g_context->_stats->setView(STATS_PARTY_OVERVIEW);

	// If we can't really cast this spell, skip the extra parameters
	if (g_spells->spellCheckPrerequisites(spell, player) != CASTERR_NOERROR) {
		gameCastSpell(spell, player, 0);
		return isDebuggerActive();
	}

	// Get the final parameters for the spell
	switch (g_spells->spellGetParamType(spell)) {
	case Spell::PARAM_NONE:
		gameCastSpell(spell, player, 0);
		break;

	case Spell::PARAM_PHASE: {
		printN("To Phase: ");
#ifdef IOS_ULTIMA4
		U4IOS::IOSConversationChoiceHelper choiceController;
		choiceController.fullSizeChoicePanel();
		choiceController.updateGateSpellChoices();
#endif
		int choice = ReadChoiceController::get("12345678 \033\n");
		if (choice < '1' || choice > '8')
			print("None");
		else {
			print("");
			gameCastSpell(spell, player, choice - '1');
		}
		break;
	}

	case Spell::PARAM_PLAYER: {
		printN("Who: ");
		int subject = gameGetPlayer(true, false);
		if (subject != -1)
			gameCastSpell(spell, player, subject);
		break;
	}

	case Spell::PARAM_DIR:
		if (g_context->_location->_context == CTX_DUNGEON)
			gameCastSpell(spell, player, g_ultima->_saveGame->_orientation);
		else {
			printN("Dir: ");
			Direction dir = gameGetDirection();
			if (dir != DIR_NONE)
				gameCastSpell(spell, player, (int)dir);
		}
		break;

	case Spell::PARAM_TYPEDIR: {
		printN("Energy type? ");
#ifdef IOS_ULTIMA4
		U4IOS::IOSConversationChoiceHelper choiceController;
		choiceController.fullSizeChoicePanel();
		choiceController.updateEnergyFieldSpellChoices();
#endif
		EnergyFieldType fieldType = ENERGYFIELD_NONE;
		char key = ReadChoiceController::get("flps \033\n\r");
		switch (key) {
		case 'f':
			fieldType = ENERGYFIELD_FIRE;
			break;
		case 'l':
			fieldType = ENERGYFIELD_LIGHTNING;
			break;
		case 'p':
			fieldType = ENERGYFIELD_POISON;
			break;
		case 's':
			fieldType = ENERGYFIELD_SLEEP;
			break;
		default:
			break;
		}

		if (fieldType != ENERGYFIELD_NONE) {
			print("");

			Direction dir;
			if (g_context->_location->_context == CTX_DUNGEON)
				dir = (Direction)g_ultima->_saveGame->_orientation;
			else {
				printN("Dir: ");
				dir = gameGetDirection();
			}

			if (dir != DIR_NONE) {

				/* Need to pack both dir and fieldType into param */
				int param = fieldType << 4;
				param |= (int)dir;

				gameCastSpell(spell, player, param);
			}
		} else {
			/* Invalid input here = spell failure */
			print("Failed!");

			/*
			 * Confirmed both mixture loss and mp loss in this situation in the
			 * original Ultima IV (at least, in the Amiga version.)
			 */
			 //c->saveGame->_mixtures[castSpell]--;
			g_context->_party->member(player)->adjustMp(
				-g_spells->spellGetRequiredMP(spell));
		}
		break;
	}

	case Spell::PARAM_FROMDIR: {
		printN("From Dir: ");
		Direction dir = gameGetDirection();
		if (dir != DIR_NONE)
			gameCastSpell(spell, player, (int)dir);
		break;
	}
	}

	return false;
}

bool Debugger::cmdCamp(int argc, const char **argv) {
	print("Hole up & Camp!");

	if (!(g_context->_location->_context & (CTX_WORLDMAP | CTX_DUNGEON))) {
		print("%cNot here!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	if (g_context->_transportContext != TRANSPORT_FOOT) {
		print("%cOnly on foot!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	CombatController *cc = new CampController();
	cc->init(nullptr);
	cc->begin();

	return isDebuggerActive();
}

bool Debugger::cmdClimb(int argc, const char **argv) {
	if (!usePortalAt(g_context->_location, g_context->_location->_coords, ACTION_KLIMB)) {
		if (g_context->_transportContext == TRANSPORT_BALLOON) {
				g_ultima->_saveGame->_balloonState = 1;
				g_context->_opacity = 0;
			print("Klimb altitude");
		} else
			print("%cKlimb what?%c", FG_GREY, FG_WHITE);
	}

	return isDebuggerActive();
}

bool Debugger::cmdDescend(int argc, const char **argv) {
	// unload the map for the second level of Lord British's Castle. The reason
	// why is that Lord British's farewell is dependent on the number of party members.
	// Instead of just redoing the dialog, it's a bit severe, but easier to unload the
	// whole level.
	bool cleanMap = (g_context->_party->size() == 1 && g_context->_location->_map->_id == 100);
	if (!usePortalAt(g_context->_location, g_context->_location->_coords, ACTION_DESCEND)) {
		if (g_context->_transportContext == TRANSPORT_BALLOON) {
			print("Land Balloon");
			if (!g_context->_party->isFlying())
				print("%cAlready Landed!%c", FG_GREY, FG_WHITE);
			else if (g_context->_location->_map->tileTypeAt(g_context->_location->_coords, WITH_OBJECTS)->canLandBalloon()) {
				g_ultima->_saveGame->_balloonState = 0;
				g_context->_opacity = 1;
			} else {
				print("%cNot Here!%c", FG_GREY, FG_WHITE);
			}
		} else {
			print("%cDescend what?%c", FG_GREY, FG_WHITE);
		}
	} else {
		if (cleanMap)
			mapMgr->unloadMap(100);
	}

	return isDebuggerActive();
}

bool Debugger::cmdEnter(int argc, const char **argv) {
	if (!usePortalAt(g_context->_location, g_context->_location->_coords, ACTION_ENTER)) {
		if (!g_context->_location->_map->portalAt(g_context->_location->_coords, ACTION_ENTER))
			print("%cEnter what?%c", FG_GREY, FG_WHITE);
	} else {
		dontEndTurn();
	}

	return isDebuggerActive();
}

bool Debugger::cmdExit(int argc, const char **argv) {
	if ((g_context->_transportContext != TRANSPORT_FOOT) && !g_context->_party->isFlying()) {
		Object *obj = g_context->_location->_map->addObject(g_context->_party->getTransport(), g_context->_party->getTransport(), g_context->_location->_coords);
		if (g_context->_transportContext == TRANSPORT_SHIP)
			g_context->_lastShip = obj;

		Tile *avatar = g_context->_location->_map->_tileSet->getByName("avatar");
		assertMsg(avatar, "no avatar tile found in tileset");

		g_context->_party->setTransport(avatar->getId());
		g_context->_horseSpeed = 0;
		print("X-it");
	} else {
		print("%cX-it What?%c", FG_GREY, FG_WHITE);
	}

	return isDebuggerActive();
}

bool Debugger::cmdFire(int argc, const char **argv) {
	if (g_context->_transportContext != TRANSPORT_SHIP) {
		print("%cFire What?%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	printN("Fire Cannon!\nDir: ");
	Direction dir = gameGetDirection();

	if (dir == DIR_NONE)
		return isDebuggerActive();

	// can only fire broadsides
	int broadsidesDirs = dirGetBroadsidesDirs(g_context->_party->getDirection());
	if (!DIR_IN_MASK(dir, broadsidesDirs)) {
		print("%cBroadsides Only!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	// nothing (not even mountains!) can block cannonballs
	Common::Array<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), broadsidesDirs, g_context->_location->_coords,
		1, 3, nullptr, false);
	for (const auto &coords : path) {
		if (fireAt(coords, true))
			return isDebuggerActive();
	}

	return isDebuggerActive();
}

bool Debugger::cmdCure(int argc, const char **argv) {
	if (argc == 2) {
		// Cure a single player by index
		int player = strToInt(argv[1]);
		if (player < 0 || player >= g_context->_party->size()) {
			print("Invalid player: %d (party has %d member%s, index 0-%d)",
				player, g_context->_party->size(),
				g_context->_party->size() == 1 ? "" : "s",
				g_context->_party->size() - 1);
			return isDebuggerActive();
		}
		PartyMember *pm = g_context->_party->member(player);
		if (pm->getStatus() != STAT_POISONED) {
			print("%s is not poisoned", pm->getName().c_str());
		} else {
			pm->heal(HT_CURE);
			print("%s cured", pm->getName().c_str());
		}
	} else {
		// Cure all poisoned party members
		int cured = 0;
		for (int i = 0; i < g_context->_party->size(); ++i) {
			PartyMember *pm = g_context->_party->member(i);
			if (pm->getStatus() == STAT_POISONED) {
				pm->heal(HT_CURE);
				print("%s cured", pm->getName().c_str());
				++cured;
			}
		}
		if (cured == 0)
			print("No one is poisoned");
	}

	return isDebuggerActive();
}

bool Debugger::cmdHeal(int argc, const char **argv) {
	if (argc == 2) {
		// Heal a single player by index
		int player = strToInt(argv[1]);
		if (player < 0 || player >= g_context->_party->size()) {
			print("Invalid player: %d (party has %d member%s, index 0-%d)",
				player, g_context->_party->size(),
				g_context->_party->size() == 1 ? "" : "s",
				g_context->_party->size() - 1);
			return isDebuggerActive();
		}
		PartyMember *pm = g_context->_party->member(player);
		if (pm->getStatus() == STAT_DEAD) {
			print("%s is dead and cannot be healed", pm->getName().c_str());
		} else if (!pm->heal(HT_FULLHEAL)) {
			print("%s is already at full health", pm->getName().c_str());
		} else {
			print("%s fully healed", pm->getName().c_str());
		}
	} else {
		// Heal all living party members
		int healed = 0;
		for (int i = 0; i < g_context->_party->size(); ++i) {
			PartyMember *pm = g_context->_party->member(i);
			if (pm->getStatus() == STAT_DEAD) {
				print("%s is dead and cannot be healed", pm->getName().c_str());
			} else if (pm->heal(HT_FULLHEAL)) {
				print("%s fully healed", pm->getName().c_str());
				++healed;
			}
		}
		if (healed == 0)
			print("Everyone is already at full health");
	}

	return isDebuggerActive();
}

bool Debugger::cmdGetChest(int argc, const char **argv) {
	int player = -1;
	if (argc == 2)
		player = strToInt(argv[1]);
	else if (isCombat())
		player = getCombatFocus();

	print("Get Chest!");

	if (g_context->_party->isFlying()) {
		print("%cDrift only!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	// first check to see if a chest exists at the current location
	// if one exists, prompt the player for the opener, if necessary
	MapCoords coords;
	g_context->_location->getCurrentPosition(&coords);
	const Tile *tile = g_context->_location->_map->tileTypeAt(coords, WITH_GROUND_OBJECTS);

	/* get the object for the chest, if it is indeed an object */
	Object *obj = g_context->_location->_map->objectAt(coords);
	if (obj && !obj->getTile().getTileType()->isChest())
		obj = nullptr;

	if (tile->isChest() || obj) {
		// if a spell was cast to open this chest,
		// player will equal -2, otherwise player
		// will default to -1 or the defult character
		// number if one was earlier specified
		if (player == -1) {
			printN("Who opens? ");
			player = gameGetPlayer(false, true);
		}
		if (player == -1)
			return isDebuggerActive();

		if (player >= 0 && player >= g_context->_party->size()) {
			print("Invalid player: %d (party has %d member%s, index 0-%d)",
				player, g_context->_party->size(),
				g_context->_party->size() == 1 ? "" : "s",
				g_context->_party->size() - 1);
			return isDebuggerActive();
		}

		if (obj)
			g_context->_location->_map->removeObject(obj);
		else {
			TileId newTile = g_context->_location->getReplacementTile(coords, tile);
			g_context->_location->_map->_annotations->add(coords, newTile, false, true);
		}

		// see if the chest is trapped and handle it
		getChestTrapHandler(player);

		print("The Chest Holds: %d Gold", g_context->_party->getChest());

		g_screen->screenPrompt();

		if (isCity(g_context->_location->_map) && obj == nullptr)
			g_context->_party->adjustKarma(KA_STOLE_CHEST);
	} else {
		print("%cNot Here!%c", FG_GREY, FG_WHITE);
	}

	return isDebuggerActive();
}

bool Debugger::cmdIgnite(int argc, const char **argv) {
	print("Ignite torch!");
	if (g_context->_location->_context == CTX_DUNGEON) {
		if (!g_context->_party->lightTorch())
			print("%cNone left!%c", FG_GREY, FG_WHITE);
	} else {
		print("%cNot here!%c", FG_GREY, FG_WHITE);
	}

	return isDebuggerActive();
}

bool Debugger::cmdInteract(int argc, const char **argv) {
	if (!settings._enhancements || !settings._enhancementsOptions._smartEnterKey)
		return isDebuggerActive();

	// Attempt to guess based on the character's surroundings

	if (g_context->_transportContext == TRANSPORT_FOOT) {
		// When on foot, check for boarding
		Object *obj = g_context->_location->_map->objectAt(g_context->_location->_coords);
		if (obj && (obj->getTile().getTileType()->isShip() ||
				obj->getTile().getTileType()->isHorse() ||
				obj->getTile().getTileType()->isBalloon()))
			return cmdBoard(argc, argv);
	} else if (g_context->_transportContext == TRANSPORT_BALLOON) {
		// Climb/Descend Balloon
		if (g_context->_party->isFlying()) {
			return cmdDescend(argc, argv);
		} else {
#ifdef IOS_ULTIMA4
			U4IOS::IOSSuperButtonHelper superHelper;
			key = ReadChoiceController::get("xk \033\n");
#else
			return cmdClimb(argc, argv);
#endif
		}
	} else {
		// For all other transports, exit the transport
		return cmdExit(argc, argv);
	}

	if ((g_context->_location->_map->portalAt(g_context->_location->_coords, ACTION_KLIMB) != nullptr))
		// Climb
		return cmdClimb(argc, argv);
	else if ((g_context->_location->_map->portalAt(g_context->_location->_coords, ACTION_DESCEND) != nullptr))
		// Descend
		return cmdDescend(argc, argv);

	if (g_context->_location->_context == CTX_DUNGEON) {
		Dungeon *dungeon = static_cast<Dungeon *>(g_context->_location->_map);
		bool up = dungeon->ladderUpAt(g_context->_location->_coords);
		bool down = dungeon->ladderDownAt(g_context->_location->_coords);
		if (up && down) {
#ifdef IOS_ULTIMA4
			U4IOS::IOSClimbHelper climbHelper;
			key = ReadChoiceController::get("kd \033\n");
#else
			return cmdClimb(argc, argv);
#endif
		} else if (up) {
			return cmdClimb(argc, argv);
		} else {
			return cmdDescend(argc, argv);
		}
	}

	if (g_context->_location->_map->portalAt(g_context->_location->_coords, ACTION_ENTER) != nullptr)
		// Enter?
		return cmdEnter(argc, argv);

	if (!g_context->_party->isFlying()) {
		// Get Chest?
		MapTile *tile = g_context->_location->_map->tileAt(g_context->_location->_coords, WITH_GROUND_OBJECTS);

		if (tile->getTileType()->isChest())
			return cmdGetChest(argc, argv);
	}

	// Otherwise default to search
	return cmdSearch(argc, argv);
}

bool Debugger::cmdJimmy(int argc, const char **argv) {
	printN("Jimmy: ");
	Direction dir = gameGetDirection();

	if (dir == DIR_NONE)
		return isDebuggerActive();

	Common::Array<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, g_context->_location->_coords,
		1, 1, nullptr, true);
	for (const auto &coords : path) {
		if (jimmyAt(coords))
			return isDebuggerActive();
	}

	print("%cJimmy what?%c", FG_GREY, FG_WHITE);
	return isDebuggerActive();
}

bool Debugger::cmdLocate(int argc, const char **argv) {
	// Normally Locate isn't allowed in combat, but allow for a special
	// debug display if this command is explicitly run in the debugger
	if (isCombat() && isDebuggerActive()) {
		CombatController *cc = static_cast<CombatController *>(eventHandler->getController());
		Coords coords = cc->getCurrentPlayer()->getCoords();
		print("Location: x:%d, y:%d, z:%d", coords.x, coords.y, coords.z);
		dontEndTurn();
	}
	// Otherwise can't use sextant in dungeon or in combat
	else if (g_context->_location->_context & ~(CTX_DUNGEON | CTX_COMBAT)) {
		if (g_ultima->_saveGame->_sextants >= 1)
			print("Locate position\nwith sextant\n Latitude: %c'%c\"\nLongitude: %c'%c\"",
				g_context->_location->_coords.y / 16 + 'A', g_context->_location->_coords.y % 16 + 'A',
				g_context->_location->_coords.x / 16 + 'A', g_context->_location->_coords.x % 16 + 'A');
		else
			print("%cLocate position with what?%c", FG_GREY, FG_WHITE);
	} else {
		print("%cNot here!%c", FG_GREY, FG_WHITE);
	}

	return isDebuggerActive();
}

bool Debugger::cmdMixReagents(int argc, const char **argv) {
	/*  uncomment this line to activate new spell mixing code */
	//   return mixReagentsSuper();
	bool done = false;

	while (!done) {
		print("Mix reagents");
#ifdef IOS_ULTIMA4
		U4IOS::beginMixSpellController();
		return isDebuggerActive(); // Just return, the dialog takes control from here.
#endif

		// Verify that there are reagents remaining in the inventory
		bool found = false;
		for (int i = 0; i < 8; i++) {
			if (g_ultima->_saveGame->_reagents[i] > 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			printN("%cNone Left!%c", FG_GREY, FG_WHITE);
			done = true;
		} else {
			printN("For Spell: ");
			g_context->_stats->setView(STATS_MIXTURES);

			int choice = ReadChoiceController::get("abcdefghijklmnopqrstuvwxyz \033\n\r");
			if (choice == -1 || choice == ' ' || choice == '\033' || choice == '\n' || choice == '\r')
				break;

			int spell = choice - 'a';
			print("\n%s", g_spells->spellGetName(spell));

			// ensure the mixtures for the spell isn't already maxed out
			if (g_ultima->_saveGame->_mixtures[spell] == 99) {
				print("\n%cYou cannot mix any more of that spell!%c", FG_GREY, FG_WHITE);
				break;
			}

			// Reset the reagent spell mix menu by removing
			// the menu highlight from the current item, and
			// hiding reagents that you don't have
			g_context->_stats->resetReagentsMenu();

			g_context->_stats->setView(MIX_REAGENTS);
			if (settings._enhancements && settings._enhancementsOptions._u5SpellMixing)
				done = mixReagentsForSpellU5(spell);
			else
				done = mixReagentsForSpellU4(spell);
		}
	}

	g_context->_stats->setView(STATS_PARTY_OVERVIEW);
	print("");

	return isDebuggerActive();
}

bool Debugger::cmdNewOrder(int argc, const char **argv) {
	printN("New Order!\nExchange # ");

	int player1 = gameGetPlayer(true, false);

	if (player1 == -1)
		return isDebuggerActive();

	if (player1 == 0) {
		print("%s, You must lead!", g_context->_party->member(0)->getName().c_str());
		return isDebuggerActive();
	}

	printN("    with # ");

	int player2 = gameGetPlayer(true, false);

	if (player2 == -1)
		return isDebuggerActive();

	if (player2 == 0) {
		print("%s, You must lead!", g_context->_party->member(0)->getName().c_str());
		return isDebuggerActive();
	}

	if (player1 >= g_context->_party->size()) {
		print("Invalid player: %d (party has %d member%s, index 0-%d)",
			player1, g_context->_party->size(),
			g_context->_party->size() == 1 ? "" : "s",
			g_context->_party->size() - 1);
		return isDebuggerActive();
	}

	if (player2 >= g_context->_party->size()) {
		print("Invalid player: %d (party has %d member%s, index 0-%d)",
			player2, g_context->_party->size(),
			g_context->_party->size() == 1 ? "" : "s",
			g_context->_party->size() - 1);
		return isDebuggerActive();
	}

	if (player1 == player2) {
		print("%cWhat?%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	g_context->_party->swapPlayers(player1, player2);
	return isDebuggerActive();
}

bool Debugger::cmdOpenDoor(int argc, const char **argv) {
	///  XXX: Pressing "o" should close any open door.

	printN("Open: ");

	if (g_context->_party->isFlying()) {
		print("%cNot Here!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	Direction dir = gameGetDirection();

	if (dir == DIR_NONE)
		return isDebuggerActive();

	Common::Array<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, g_context->_location->_coords,
		1, 1, nullptr, true);
	for (const auto &coords : path) {
		if (openAt(coords))
			return isDebuggerActive();
	}

	print("%cNot Here!%c", FG_GREY, FG_WHITE);
	return isDebuggerActive();
}

bool Debugger::cmdParty(int argc, const char **argv) {
	if (settings._enhancements && settings._enhancementsOptions._activePlayer) {
		int player = (argc == 2) ? strToInt(argv[1]) - 1 : -1;
		if (player >= 0 && player >= g_context->_party->size()) {
			print("Invalid player: %d (party has %d member%s, index 1-%d)",
				player + 1, g_context->_party->size(),
				g_context->_party->size() == 1 ? "" : "s",
				g_context->_party->size());
			return isDebuggerActive();
		}
		gameSetActivePlayer(player);
	} else {
		print("%cBad command!%c", FG_GREY, FG_WHITE);
	}

	dontEndTurn();
	return isDebuggerActive();
}

bool Debugger::cmdPass(int argc, const char **argv) {
	print("Pass");
	return isDebuggerActive();
}

bool Debugger::cmdPeer(int argc, const char **argv) {
	bool useGem = (argc != 2) ? true : strToBool(argv[1]);
	peer(useGem);

	return isDebuggerActive();
}

bool Debugger::cmdQuitAndSave(int argc, const char **argv) {
	print("Quit & Save...\n%d moves", g_ultima->_saveGame->_moves);
	if (g_context->_location->_context & CTX_CAN_SAVE_GAME) {
		(void)g_ultima->saveGameDialog();
		g_ultima->quitGame();

		return false;
	} else {
		print("%cNot here!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}
}

bool Debugger::cmdReadyWeapon(int argc, const char **argv) {
	int player = -1;
	if (argc == 2)
		player = strToInt(argv[1]);
	else if (isCombat())
		player = getCombatFocus();

	// get the player if not provided
	if (player == -1) {
		printN("Ready a weapon for: ");
		player = gameGetPlayer(true, false);
		if (player == -1)
			return isDebuggerActive();
	}

	if (player < 0 || player >= g_context->_party->size()) {
		print("Invalid player: %d (party has %d member%s, index 0-%d)",
			player, g_context->_party->size(),
			g_context->_party->size() == 1 ? "" : "s",
			g_context->_party->size() - 1);
		return isDebuggerActive();
	}

	// get the weapon to use
	g_context->_stats->setView(STATS_WEAPONS);
	printN("Weapon: ");
	int weapon = AlphaActionController::get(WEAP_MAX + 'a' - 1, "Weapon: ");
	g_context->_stats->setView(STATS_PARTY_OVERVIEW);
	if (weapon == -1)
		return isDebuggerActive();

	PartyMember *p = g_context->_party->member(player);
	const Weapon *w = g_weapons->get((WeaponType)weapon);

	if (!w) {
		print("");
		return isDebuggerActive();
	}

	switch (p->setWeapon(w)) {
	case EQUIP_SUCCEEDED:
		print("%s", w->getName().c_str());
		break;
	case EQUIP_NONE_LEFT:
		print("%cNone left!%c", FG_GREY, FG_WHITE);
		break;
	case EQUIP_CLASS_RESTRICTED:
	{
		Common::String indef_article;

		switch (tolower(w->getName()[0])) {
		case 'a':
		case 'e':
		case 'i':
		case 'o':
		case 'u':
		case 'y':
			indef_article = "an";
			break;
		default:
			indef_article = "a";
			break;
		}

		print("\n%cA %s may NOT use %s %s%c", FG_GREY, getClassName(p->getClass()),
			indef_article.c_str(), w->getName().c_str(), FG_WHITE);
		break;
	}
	}

	return isDebuggerActive();
}

bool Debugger::cmdSearch(int argc, const char **argv) {
	if (g_context->_location->_context == CTX_DUNGEON) {
		dungeonSearch();
	} else if (g_context->_party->isFlying()) {
		print("Searching...\n%cDrift only!%c", FG_GREY, FG_WHITE);
	} else if (g_context->_location->_map->_id == MAP_SCUMMVM &&
			g_context->_location->_coords == Coords(52, 5, 0)) {
		// Special hack for the ScummVM easter egg map. Searching on
		// the given tile triggers the cheat to allow teleporting
		print("Searching...\nFound teleport point!");
		g_game->exitToParentMap();
		g_music->playMapMusic();

		return cmdGoto(argc, argv);
	} else {
		print("Searching...");

		const ItemLocation *item = g_items->itemAtLocation(g_context->_location->_map, g_context->_location->_coords);
		if (item) {
			if (item->_isItemInInventory && (g_items->*(item->_isItemInInventory))(item->_data)) {
				print("%cNothing Here!%c", FG_GREY, FG_WHITE);
			} else {
				if (item->_name)
					print("You find...\n%s!", item->_name);
				(g_items->*(item->_putItemInInventory))(item->_data);
			}
		} else if (usePortalAt(g_context->_location, g_context->_location->_coords, ACTION_ENTER)) {
			print("");
		} else {
			print("%cNothing Here!%c", FG_GREY, FG_WHITE);
		}
	}

	return isDebuggerActive();
}

bool Debugger::cmdSpeed(int argc, const char **argv) {
	Common::String action = argv[1];
	int oldCycles = settings._gameCyclesPerSecond;

	if (action == "up") {
		if (++settings._gameCyclesPerSecond > MAX_CYCLES_PER_SECOND)
			settings._gameCyclesPerSecond = MAX_CYCLES_PER_SECOND;
	} else if (action == "down") {
		if (--settings._gameCyclesPerSecond == 0)
			settings._gameCyclesPerSecond = 1;
	} else if (action == "normal") {
		settings._gameCyclesPerSecond = DEFAULT_CYCLES_PER_SECOND;
	}

	if (oldCycles != settings._gameCyclesPerSecond) {
		settings._eventTimerGranularity = (1000 / settings._gameCyclesPerSecond);
		eventHandler->getTimer()->reset(settings._eventTimerGranularity);

		if (settings._gameCyclesPerSecond == DEFAULT_CYCLES_PER_SECOND)
			print("Speed: Normal");
		else if (action == "up")
			print("Speed Up (%d)", settings._gameCyclesPerSecond);
		else
			print("Speed Down (%d)", settings._gameCyclesPerSecond);
	} else if (settings._gameCyclesPerSecond == DEFAULT_CYCLES_PER_SECOND) {
		print("Speed: Normal");
	}

	dontEndTurn();
	return isDebuggerActive();
}

bool Debugger::cmdCombatSpeed(int argc, const char **argv) {
	Common::String action = argv[1];
	int oldSpeed = settings._battleSpeed;

	if (action == "up" && ++settings._battleSpeed > MAX_BATTLE_SPEED)
		settings._battleSpeed = MAX_BATTLE_SPEED;
	else if (action == "down" && --settings._battleSpeed == 0)
		settings._battleSpeed = 1;
	else if (action == "normal")
		settings._battleSpeed = DEFAULT_BATTLE_SPEED;

	if (oldSpeed != settings._battleSpeed) {
		if (settings._battleSpeed == DEFAULT_BATTLE_SPEED) {
			print("Battle Speed:\nNormal");
		} else if (action == "up") {
			print("Battle Speed:\nUp (%d)", settings._battleSpeed);
		} else {
			print("Battle Speed:\nDown (%d)", settings._battleSpeed);
		}
	} else if (settings._battleSpeed == DEFAULT_BATTLE_SPEED) {
		print("Battle Speed:\nNormal");
	}

	dontEndTurn();
	return isDebuggerActive();
}

bool Debugger::cmdStats(int argc, const char **argv) {
	int player = -1;
	if (argc == 2)
		player = strToInt(argv[1]);
	else if (isCombat())
		player = getCombatFocus();

	// get the player if not provided
	if (player == -1) {
		printN("Ztats for: ");
		player = gameGetPlayer(true, false);
		if (player == -1)
			return isDebuggerActive();
	} else {
		print("Ztats");
	}

	if (player < 0 || player >= g_context->_party->size()) {
		print("Invalid player: %d (party has %d member%s, index 0-%d)",
			player, g_context->_party->size(),
			g_context->_party->size() == 1 ? "" : "s",
			g_context->_party->size() - 1);
		return isDebuggerActive();
	}

	// Reset the reagent spell mix menu by removing
	// the menu highlight from the current item, and
	// hiding reagents that you don't have
	g_context->_stats->resetReagentsMenu();

	g_context->_stats->setView(StatsView(STATS_CHAR1 + player));
#ifdef IOS_ULTIMA4
	U4IOS::IOSHideActionKeysHelper hideExtraControls;
#endif
	ZtatsController ctrl;
	eventHandler->pushController(&ctrl);
	ctrl.waitFor();

	// Print stats summary to the debug console
	PartyMember *pm = g_context->_party->member(player);
	const char *statusStr;
	switch (pm->getStatus()) {
	case STAT_GOOD:      statusStr = "Good";     break;
	case STAT_POISONED:  statusStr = "Poisoned"; break;
	case STAT_SLEEPING:  statusStr = "Sleeping"; break;
	case STAT_DEAD:      statusStr = "Dead";     break;
	default:             statusStr = "Unknown";  break;
	}
	print("--------------------------------");
	print("%-16s  %s (%s)", pm->getName().c_str(),
		getClassName(pm->getClass()), statusStr);
	print("--------------------------------");
	print("LVL: %-4d          XP: %d",  pm->getRealLevel(), pm->getExp());
	print("STR: %-4d         DEX: %d",  pm->getStr(),       pm->getDex());
	print("INT: %-4d          MP: %d/%d", pm->getInt(),     pm->getMp(), pm->getMaxMp());
	print(" HP: %d/%d",                  pm->getHp(),       pm->getMaxHp());
	print("  W: %s",  pm->getWeapon() ? pm->getWeapon()->getName().c_str() : "none");
	print("  A: %s",  pm->getArmor()  ? pm->getArmor()->getName().c_str()  : "none");
	print("--------------------------------");

	return isDebuggerActive();
}

bool Debugger::cmdTalk(int argc, const char **argv) {
	printN("Talk: ");

	if (g_context->_party->isFlying()) {
		print("%cDrift only!%c", FG_GREY, FG_WHITE);
		return isDebuggerActive();
	}

	Direction dir = gameGetDirection();

	if (dir == DIR_NONE)
		return isDebuggerActive();

	Common::Array<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, g_context->_location->_coords,
		1, 2, &Tile::canTalkOverTile, true);
	for (const auto &coords : path) {
		if (talkAt(coords))
			return isDebuggerActive();
	}

	print("Funny, no response!");
	return isDebuggerActive();
}

bool Debugger::cmdUse(int argc, const char **argv) {
	print("Use which item:");

	if (settings._enhancements) {
		// a little xu4 enhancement: show items in inventory when prompted for an item to use
		g_context->_stats->setView(STATS_ITEMS);
	}
#ifdef IOS_ULTIMA4
	U4IOS::IOSConversationHelper::setIntroString("Use which item?");
#endif
	g_items->itemUse(gameGetInput().c_str());
	return isDebuggerActive();
}

bool Debugger::cmdWearArmor(int argc, const char **argv) {
	int player = -1;
	if (argc == 2)
		player = strToInt(argv[1]);

	// get the player if not provided
	if (player == -1) {
		printN("Wear Armour\nfor: ");
		player = gameGetPlayer(true, false);
		if (player == -1)
			return isDebuggerActive();
	}

	if (player < 0 || player >= g_context->_party->size()) {
		print("Invalid player: %d (party has %d member%s, index 0-%d)",
			player, g_context->_party->size(),
			g_context->_party->size() == 1 ? "" : "s",
			g_context->_party->size() - 1);
		return isDebuggerActive();
	}

	g_context->_stats->setView(STATS_ARMOR);
	printN("Armour: ");
	int armor = AlphaActionController::get(ARMR_MAX + 'a' - 1, "Armour: ");
	g_context->_stats->setView(STATS_PARTY_OVERVIEW);
	if (armor == -1)
		return isDebuggerActive();

	const Armor *a = g_armors->get((ArmorType)armor);
	PartyMember *p = g_context->_party->member(player);

	if (!a) {
		print("");
		return isDebuggerActive();
	}

	switch (p->setArmor(a)) {
	case EQUIP_SUCCEEDED:
		print("%s", a->getName().c_str());
		break;
	case EQUIP_NONE_LEFT:
		print("%cNone left!%c", FG_GREY, FG_WHITE);
		break;
	case EQUIP_CLASS_RESTRICTED:
		print("\n%cA %s may NOT use %s%c", FG_GREY, getClassName(p->getClass()), a->getName().c_str(), FG_WHITE);
		break;
	}

	return isDebuggerActive();
}

bool Debugger::cmdYell(int argc, const char **argv) {
	printN("Yell ");
	if (g_context->_transportContext == TRANSPORT_HORSE) {
		if (g_context->_horseSpeed == 0) {
			print("Giddyup!");
			g_context->_horseSpeed = 1;
		} else {
			print("Whoa!");
			g_context->_horseSpeed = 0;
		}
	} else {
		print("%cWhat?%c", FG_GREY, FG_WHITE);
	}

	return isDebuggerActive();
}


bool Debugger::cmd3d(int argc, const char **argv) {
	if (g_context->_location->_context == CTX_DUNGEON) {
		print("3-D view %s", DungeonViewer.toggle3DDungeonView() ? "on" : "off");
	} else {
		print("Not here");
	}

	return isDebuggerActive();
}

bool Debugger::cmdAbyss(int argc, const char **argv) {
	// first teleport to the abyss
	g_context->_location->_coords.x = 0xe9;
	g_context->_location->_coords.y = 0xe9;
	g_game->setMap(mapMgr->get(MAP_ABYSS), 1, nullptr);

	// then to the final altar
	g_context->_location->_coords.x = 7;
	g_context->_location->_coords.y = 7;
	g_context->_location->_coords.z = 7;
	g_ultima->_saveGame->_orientation = DIR_NORTH;
	g_context->_party->lightTorch(100, false);

	cmdIgnite(0, nullptr);
	return isDebuggerActive();
}

bool Debugger::cmdCollisions(int argc, const char **argv) {
	_collisionOverride = !_collisionOverride;
	print("Collision detection %s",
		_collisionOverride ? "off" : "on");

	return isDebuggerActive();
}

bool Debugger::cmdCompanions(int argc, const char **argv) {
	for (int m = g_ultima->_saveGame->_members; m < 8; m++) {
		if (g_context->_party->canPersonJoin(g_ultima->_saveGame->_players[m]._name, nullptr)) {
			g_context->_party->join(g_ultima->_saveGame->_players[m]._name);
		}
	}

	g_context->_stats->update();
	print("Joined by companions");
	return isDebuggerActive();
}

bool Debugger::cmdCombat(int argc, const char **argv) {
	_disableCombat = !_disableCombat;
	print("Combat encounters %s",
		_disableCombat ? "off" : "on");

	return isDebuggerActive();
}

bool Debugger::cmdDestroy(int argc, const char **argv) {
	Direction dir;

	if (argc == 2) {
		dir = directionFromName(argv[1]);
	} else if (isDebuggerActive()) {
		print("destroy <direction>");
		return isDebuggerActive();
	} else {
		printN("Destroy Object\nDir: ");
		dir = gameGetDirection();
	}

	if (dir == DIR_NONE)
		return isDebuggerActive();

	Common::Array<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir),
		MASK_DIR_ALL, g_context->_location->_coords, 1, 1, nullptr, true);
	for (const auto &coords : path) {
		if (destroyAt(coords)) {
			return false;
		}
	}

	print("%cNothing there!%c", FG_GREY, FG_WHITE);
	return isDebuggerActive();
}

bool Debugger::cmdDestroyCreatures(int argc, const char **argv) {
	gameDestroyAllCreatures();
	dontEndTurn();

	return isDebuggerActive();
}

bool Debugger::cmdDungeon(int argc, const char **argv) {
	if (g_context->_location->_context & CTX_WORLDMAP) {
		if (argc == 2) {
			int dungNum = strToInt(argv[1]);

			if (dungNum >= 1 && dungNum <= 8) {
				g_context->_location->_coords = g_context->_location->_map->_portals[dungNum + 15]->_coords;
				return false;
			} else if (dungNum == 9) {
				g_game->setMap(mapMgr->get(MAP_DECEIT), 1, nullptr);
				g_context->_location->_coords = MapCoords(1, 0, 7);
				g_ultima->_saveGame->_orientation = DIR_SOUTH;
			} else if (dungNum == 10) {
				g_game->setMap(mapMgr->get(MAP_DESPISE), 1, nullptr);
				g_context->_location->_coords = MapCoords(3, 2, 7);
				g_ultima->_saveGame->_orientation = DIR_SOUTH;
			} else if (dungNum == 11) {
				g_game->setMap(mapMgr->get(MAP_DESTARD), 1, nullptr);
				g_context->_location->_coords = MapCoords(7, 6, 7);
				g_ultima->_saveGame->_orientation = DIR_SOUTH;
			} else {
				print("Invalid dungeon");
				return isDebuggerActive();
			}

			return false;
		} else {
			print("dungeon <number>");
		}
	} else {
		print("Not here");
	}

	return isDebuggerActive();
}

bool Debugger::cmdFlee(int argc, const char **argv) {
	if (eventHandler->getController() == g_combat) {
		// End the combat without losing karma
		g_combat->end(false);
	} else {
		print("Bad command");
	}

	return isDebuggerActive();
}

bool Debugger::cmdEquipment(int argc, const char **argv) {
	int i;

	for (i = ARMR_NONE + 1; i < ARMR_MAX; ++i)
		g_ultima->_saveGame->_armor[i] = 8;

	for (i = WEAP_HANDS + 1; i < WEAP_MAX; ++i) {
		const Weapon *weapon = g_weapons->get(static_cast<WeaponType>(i));
		if (weapon->loseWhenUsed() || weapon->loseWhenRanged())
			g_ultima->_saveGame->_weapons[i] = 99;
		else
			g_ultima->_saveGame->_weapons[i] = 8;
	}

	print("All equipment given");
	return isDebuggerActive();
}

bool Debugger::cmdGate(int argc, const char **argv) {
	int gateNum = (argc == 2) ? strToInt(argv[1]) : -1;

	if (!g_context || !g_game || gateNum < 1 || gateNum > 8) {
		print("Gate <1 to 8>");
	} else {
		if (!isDebuggerActive())
			print("Gate %d!", gateNum);

		if (g_context->_location->_map->isWorldMap()) {
			const Coords *moongate = g_moongates->getGateCoordsForPhase(gateNum - 1);
			if (moongate) {
				g_context->_location->_coords = *moongate;
				return false;
			}
		} else {
			print("Not here!");
		}
	}

	return isDebuggerActive();
}

bool Debugger::cmdGoto(int argc, const char **argv) {
	Common::String dest;
	PortalList &portals = g_context->_location->_map->_portals;
	uint p;

	if (argc == 2) {
		dest = argv[1];
	} else if (isDebuggerActive()) {
		print("teleport <destination name>");
		return true;
	} else {
		printN("Goto: ");
		dest = gameGetInput(32);
		print("");
	}

	dest.toLowercase();
	if (dest == "britain")
		dest = "britannia";

	bool found = false;
	p = strToInt(dest.c_str());

	if (p > 0 && p <= portals.size()) {
		g_context->_location->_coords = portals[p - 1]->_coords;
		found = true;
	}

	for (p = 0; p < portals.size() && !found; p++) {
		MapId destid = portals[p]->_destid;
		Common::String destNameLower = mapMgr->get(destid)->getName();
		destNameLower.toLowercase();

		if (destNameLower.find(dest) != Common::String::npos) {
			print("\n%s", mapMgr->get(destid)->getName().c_str());
			g_context->_location->_coords = portals[p]->_coords;
			found = true;
			break;
		}
	}

	if (!found) {
		MapCoords coords = g_context->_location->_map->getLabel(dest);
		if (coords != MapCoords::nowhere()) {
			print("%s", dest.c_str());
			g_context->_location->_coords = coords;
			found = true;
		}
	}

	if (found) {
		return false;
	} else {
		if (isDebuggerActive())
			print("Can't find %s", dest.c_str());
		else
			print("Can't find\n%s", dest.c_str());

		return isDebuggerActive();
	}
}

bool Debugger::cmdLorddBritish(int argc, const char **argv) {
	if (!isDebuggerActive()) {
		print("Help me LB!");
		g_screen->screenPrompt();
	}

	// Help! send me to Lord British
	g_game->setMap(mapMgr->get(100), 1, nullptr);
	g_context->_location->_coords.x = 19;
	g_context->_location->_coords.y = 8;
	g_context->_location->_coords.z = 0;

	return false;
}

bool Debugger::cmdItems(int argc, const char **argv) {
	SaveGame &sg = *g_ultima->_saveGame;
	sg._torches = 99;
	sg._gems = 99;
	sg._keys = 99;
	sg._sextants = 1;
	sg._items = ITEM_SKULL | ITEM_CANDLE | ITEM_BOOK | ITEM_BELL | ITEM_KEY_C | ITEM_KEY_L | ITEM_KEY_T | ITEM_HORN | ITEM_WHEEL;
	sg._stones = 0xff;
	sg._runes = 0xff;
	sg._food = 999900;
	sg._gold = 9999;

	g_context->_stats->update();
	print("All items given");
	return isDebuggerActive();
}

bool Debugger::cmdKarma(int argc, const char **argv) {
	print("Karma!");

	for (int i = 0; i < 8; ++i) {
		Common::String line = Common::String::format("%s:",
			getVirtueName(static_cast<Virtue>(i)));
		while (line.size() < 13)
			line += ' ';

		if (g_ultima->_saveGame->_karma[i] > 0)
			line += Common::String::format("%.2d", g_ultima->_saveGame->_karma[i]);
		else
			line += "--";
		print("%s", line.c_str());
	}

	return isDebuggerActive();
}

bool Debugger::cmdLeave(int argc, const char **argv) {
	if (!g_game->exitToParentMap()) {
		print("Not Here");
	} else {
		g_music->playMapMusic();
		print("Exited");
	}

	return isDebuggerActive();
}

bool Debugger::cmdLocation(int argc, const char **argv) {
	const MapCoords &pos = g_context->_location->_coords;

	if (argc == 3) {
		Coords newPos;

		if (strlen(argv[1]) == 2 && strlen(argv[2]) == 2
				&& Common::isAlpha(argv[1][0]) && Common::isAlpha(argv[1][1])
			&& Common::isAlpha(argv[2][0]) && Common::isAlpha(argv[2][1])
		) {
			newPos.y = (toupper(argv[1][0]) - 'A') * 16 + (toupper(argv[1][1]) - 'A');
			newPos.x = (toupper(argv[2][0]) - 'A') * 16 + (toupper(argv[2][1]) - 'A');
		} else {
			newPos.x = strToInt(argv[1]);
			newPos.y = strToInt(argv[2]);
		}

		if (newPos.x >= 0 && newPos.y >= 0
				&& newPos.x < (int)g_context->_location->_map->_width
				&& newPos.y < (int)g_context->_location->_map->_height) {
			g_context->_location->_coords = newPos;
			return false;
		} else {
			print("Invalid location!");
		}
	} else if (isDebuggerActive()) {
		if (g_context->_location->_map->isWorldMap())
			print("Location: %s x: %d, y: %d",
				"World Map", pos.x, pos.y);
		else
			print("Location: %s x: %d, y: %d, z: %d",
				g_context->_location->_map->getName().c_str(), pos.x, pos.y, pos.z);
	} else {
		if (g_context->_location->_map->isWorldMap())
			print("\nLocation:\n%s\nx: %d\ny: %d", "World Map",
				pos.x, pos.y);
		else
			print("\nLocation:\n%s\nx: %d\ny: %d\nz: %d",
				g_context->_location->_map->getName().c_str(), pos.x, pos.y, pos.z);
	}

	return isDebuggerActive();
}

bool Debugger::cmdMixtures(int argc, const char **argv) {
	for (int i = 0; i < SPELL_MAX; i++)
		g_ultima->_saveGame->_mixtures[i] = 99;

	print("All mixtures given");
	return isDebuggerActive();
}

bool Debugger::cmdOverhead(int argc, const char **argv) {
	if ((g_context->_location->_viewMode == VIEW_NORMAL) || (g_context->_location->_viewMode == VIEW_DUNGEON))
		g_context->_location->_viewMode = VIEW_GEM;
	else if (g_context->_location->_context == CTX_DUNGEON)
		g_context->_location->_viewMode = VIEW_DUNGEON;
	else
		g_context->_location->_viewMode = VIEW_NORMAL;

	print("Toggle view");
	return isDebuggerActive();
}

bool Debugger::cmdMoon(int argc, const char **argv) {
	int moonNum;

	if (argc == 2) {
		moonNum = strToInt(argv[1]);
		if (moonNum < 0 || moonNum > 7) {
			print("Invalid moon");
			return true;
		}
	} else {
		moonNum = (g_ultima->_saveGame->_trammelPhase + 1) & 7;
	}

	while (g_ultima->_saveGame->_trammelPhase != moonNum)
		g_game->updateMoons(true);

	print("Moons advanced");
	return isDebuggerActive();
}

bool Debugger::cmdOpacity(int argc, const char **argv) {
	g_context->_opacity = !g_context->_opacity;
	print("Opacity is %s", g_context->_opacity ? "on" : "off");
	return isDebuggerActive();
}

bool Debugger::cmdReagents(int argc, const char **argv) {
	for (int i = 0; i < REAG_MAX; i++)
		g_ultima->_saveGame->_reagents[i] = 99;

	print("Reagents given");
	return isDebuggerActive();
}

bool Debugger::cmdFullStats(int argc, const char **argv) {
	for (int i = 0; i < g_ultima->_saveGame->_members; i++) {
		g_ultima->_saveGame->_players[i]._str = 50;
		g_ultima->_saveGame->_players[i]._dex = 50;
		g_ultima->_saveGame->_players[i]._intel = 50;

		if (g_ultima->_saveGame->_players[i]._hpMax < 800) {
			g_ultima->_saveGame->_players[i]._xp = 9999;
			g_ultima->_saveGame->_players[i]._hpMax = 800;
			g_ultima->_saveGame->_players[i]._hp = 800;
		}
	}

	g_context->_stats->update();
	print("Full Stats given");
	return isDebuggerActive();
}

bool Debugger::cmdHunger(int argc, const char **argv) {
	_disableHunger = !_disableHunger;
	print("Party hunger %s",
		_disableHunger ? "off" : "on");

	return isDebuggerActive();
}

bool Debugger::cmdSummon(int argc, const char **argv) {
	Common::String creature;

	if (argc == 2) {
		creature = argv[1];
	} else if (isDebuggerActive()) {
		print("summon <creature name>");
		return true;
	} else {
		print("Summon!");
		print("What?");
		creature = gameGetInput();
	}

	summonCreature(creature);
	return isDebuggerActive();
}

bool Debugger::cmdTorch(int argc, const char **argv) {
	print("Torch: %d", g_context->_party->getTorchDuration());
	if (!isDebuggerActive())
		g_screen->screenPrompt();

	return isDebuggerActive();
}

bool Debugger::cmdTransport(int argc, const char **argv) {
	if (!g_context->_location->_map->isWorldMap()) {
		print("Not here!");
		return isDebuggerActive();
	}

	_horse = g_context->_location->_map->_tileSet->getByName("horse")->getId();
	_ship = g_context->_location->_map->_tileSet->getByName("ship")->getId();
	_balloon = g_context->_location->_map->_tileSet->getByName("balloon")->getId();

	MapCoords coords = g_context->_location->_coords;
	MapTile *choice;
	Tile *tile;

	// Get the transport of choice
	char transport;
	if (argc >= 2) {
		transport = argv[1][0];
	} else if (isDebuggerActive()) {
		print("transport <transport name>");
		return isDebuggerActive();
	} else {
		transport = ReadChoiceController::get("shb \033\015");
	}

	switch (transport) {
	case 's':
		choice = &_ship;
		break;
	case 'h':
		choice = &_horse;
		break;
	case 'b':
		choice = &_balloon;
		break;
	default:
		print("Unknown transport");
		return isDebuggerActive();
	}

	tile = g_context->_location->_map->_tileSet->get(choice->getId());
	Direction dir;

	if (argc == 3) {
		dir = directionFromName(argv[2]);
	} else if (isDebuggerActive()) {
		dir = DIR_NONE;
	} else {
		print("%s", tile->getName().c_str());

		// Get the direction in which to create the transport
		ReadDirController readDir;
		eventHandler->pushController(&readDir);

		printN("Dir: ");
		dir = readDir.waitFor();
	}

	coords.move(dir, g_context->_location->_map);

	if (coords != g_context->_location->_coords) {
		bool ok;
		MapTile *ground = g_context->_location->_map->tileAt(coords, WITHOUT_OBJECTS);

		switch (transport) {
		case 's':
			ok = ground->getTileType()->isSailable();
			break;
		case 'h':
			ok = ground->getTileType()->isWalkable();
			break;
		case 'b':
			ok = ground->getTileType()->isWalkable();
			break;
		default:
			ok = false;
			break;
		}

		if (ok) {
			g_context->_location->_map->addObject(*choice, *choice, coords);
			print("%s created!", tile->getName().c_str());
		} else if (!choice) {
			print("Invalid transport!");
		} else {
			print("Can't place %s there!", tile->getName().c_str());
		}
	}

	return isDebuggerActive();
}

bool Debugger::cmdUp(int argc, const char **argv) {
	if ((g_context->_location->_context & CTX_DUNGEON) && (g_context->_location->_coords.z > 0)) {
		g_context->_location->_coords.z--;

		return false;
	} else {
		print("Leaving...");
		g_game->exitToParentMap();
		g_music->playMapMusic();

		return isDebuggerActive();
	}
}

bool Debugger::cmdDown(int argc, const char **argv) {
	if ((g_context->_location->_context & CTX_DUNGEON) && (g_context->_location->_coords.z < 7)) {
		g_context->_location->_coords.z++;
		return false;
	} else {
		print("Not here");
		return isDebuggerActive();
	}
}

bool Debugger::cmdVirtue(int argc, const char **argv) {
	if (argc == 1) {
		for (int i = 0; i < 8; i++)
			g_ultima->_saveGame->_karma[i] = 0;

		g_context->_stats->update();
		print("Full virtues");
	} else {
		int virtue = strToInt(argv[1]);

		if (virtue <= 0 || virtue >= VIRT_MAX) {
			print("Invalid virtue");
		} else {
			print("Improved %s", getVirtueName((Virtue)virtue));

			if (g_ultima->_saveGame->_karma[virtue] == 99)
				g_ultima->_saveGame->_karma[virtue] = 0;
			else if (g_ultima->_saveGame->_karma[virtue] != 0)
				g_ultima->_saveGame->_karma[virtue] += 10;
			if (g_ultima->_saveGame->_karma[virtue] > 99)
				g_ultima->_saveGame->_karma[virtue] = 99;
			g_context->_stats->update();
		}
	}

	return isDebuggerActive();
}

bool Debugger::cmdWind(int argc, const char **argv) {
	Common::String windDir;

	if (argc == 2) {
		windDir = argv[1];
	} else if (isDebuggerActive()) {
		print("wind <direction or 'lock'>");
		return true;
	} else {
		print("Wind Dir ('l' to lock)");
		windDir = gameGetInput();
	}

	windDir.toLowercase();
	if (windDir == "lock" || windDir == "l") {
		g_context->_windLock = !g_context->_windLock;
		print("Wind direction is %slocked",
			g_context->_windLock ? "" : "un");
	} else {
		Direction dir = directionFromName(windDir);

		if (dir == DIR_NONE) {
			print("Unknown direction");
			return isDebuggerActive();
		} else {
			g_context->_windDirection = dir;
		}
	}

	return false;
}

bool Debugger::cmdListTriggers(int argc, const char **argv) {
	CombatMap *map = nullptr;

	if (isCombat() && (map = static_cast<CombatController *>(
			eventHandler->getController())->getMap()) != nullptr
			&& map->isDungeonRoom()) {
		Dungeon *dungeon = dynamic_cast<Dungeon *>(g_context->_location->_prev->_map);
		assert(dungeon);
		Trigger *triggers = dungeon->_rooms[dungeon->_currentRoom]._triggers;
		assert(triggers);
		int i;

		print("Triggers!");

		for (i = 0; i < 4; i++) {
			print("%.1d)xy tile xy xy", i + 1);
			print("  %.1X%.1X  %.3d %.1X%.1X %.1X%.1X",
				triggers[i].x, triggers[i].y,
				triggers[i]._tile,
				triggers[i]._changeX1, triggers[i]._changeY1,
				triggers[i].changeX2, triggers[i].changeY2);
		}
		prompt();
		dontEndTurn();

	} else {
		print("Not here!");
	}

	return isDebuggerActive();
}

bool Debugger::cmdHelp(int argc, const char **argv) {
	struct HelpEntry {
		const char *name;
		const char *syntax;
		const char *desc;
	};
	static const HelpEntry HELP_TABLE[] = {
		// Game actions
		{ "attack",          "attack <direction>",                   "Attack in a direction" },
		{ "board",           "board",                                "Board ship/horse/balloon at current tile" },
		{ "camp",            "camp",                                 "Camp/hole up (world map or dungeon, on foot only)" },
		{ "cast",            "cast [player] [spell_letter]",         "Cast a spell; prompts interactively if args omitted" },
		{ "climb",           "climb",                                "Climb portal or ascend balloon" },
		{ "cure",            "cure [player]",                        "Cure poison for one player (index 0-N) or all if omitted" },
		{ "heal",            "heal [player]",                        "Fully restore HP for one player (index 0-N) or all if omitted; dead members cannot be healed" },
		{ "combat_speed",    "combat_speed <up|down|normal>",        "Adjust combat speed" },
		{ "descend",         "descend",                              "Descend portal or land balloon" },
		{ "enter",           "enter",                                "Enter portal (city, dungeon, etc.)" },
		{ "exit",            "exit",                                 "Exit current transport, placing it on the map" },
		{ "fire",            "fire",                                 "Fire ship cannon (broadside only)" },
		{ "get",             "get [player]",                         "Open chest at current location" },
		{ "ignite",          "ignite",                               "Light torch in dungeon" },
		{ "interact",        "interact",                             "Context-smart action: boards/climbs/enters/searches as appropriate" },
		{ "jimmy",           "jimmy",                                "Pick a lock in specified direction" },
		{ "locate",          "locate",                               "Show sextant coordinates" },
		{ "mix",             "mix",                                  "Open reagent mixing interface" },
		{ "move",            "move <direction>",                     "Move avatar one tile" },
		{ "open",            "open",                                 "Open door in a direction (on foot only)" },
		{ "order",           "order",                                "Swap party member positions" },
		{ "party",           "party [player]",                       "Set active player (enhanced mode only)" },
		{ "pass",            "pass",                                 "Pass turn" },
		{ "peer",            "peer",                                 "Use magic gem for overhead view" },
		{ "quitAndSave",     "quitAndSave",                          "Save and quit" },
		{ "ready",           "ready [player]",                       "Equip weapon for a party member" },
		{ "search",          "search",                               "Search current location for secrets/items" },
		{ "speed",           "speed <up|down|normal>",               "Adjust game speed" },
		{ "spell",           "spell [player] [spell_letter]",        "Alias for cast" },
		{ "stats",           "stats [player]",                       "Show character stat screen; player index 0-7 (0=Avatar), max is party size - 1" },
		{ "talk",            "talk",                                 "Talk to NPC in a direction (searches up to 2 tiles)" },
		{ "use",             "use",                                  "Use an inventory item" },
		{ "wear",            "wear [player]",                        "Equip armor for a party member" },
		{ "yell",            "yell",                                 "Toggle horse speed (giddyup/whoa)" },
		// Cheat / debug
		{ "3d",              "3d",                                   "Toggle 3D dungeon view (dungeon only)" },
		{ "abyss",           "abyss",                                "Teleport to Abyss final altar (7,7,7 floor 7), lights torch" },
		{ "collisions",      "collisions",                           "Toggle collision detection on/off" },
		{ "combat",          "combat",                               "Toggle random combat encounters on/off" },
		{ "companions",      "companions",                           "Recruit all available companions" },
		{ "destroy",         "destroy <direction>",                  "Destroy object/enemy one tile ahead" },
		{ "destroy_creatures","destroy_creatures",                   "Instantly kill all creatures on current map" },
		{ "down",            "down",                                 "Move down one dungeon floor; floors 0-7, fails at floor 7" },
		{ "dungeon",         "dungeon <1-11>",                       "Teleport to dungeon 1-8 (main) or 9-11 (Deceit/Despise/Destard); world map only; enters at floor 7" },
		{ "equipment",       "equipment",                            "Grant 8 of every equipment type, 99 consumable weapons" },
		{ "flee",            "flee",                                 "End combat instantly, no karma penalty" },
		{ "fullstats",       "fullstats",                            "Max all party stats (STR/DEX/INT 50, HP 800, XP 9999)" },
		{ "gate",            "gate <1-8>",                           "Teleport to moongate for phase 1-8; world map only" },
		{ "goto",            "goto <name|portal_num|label>",         "Teleport by location name, portal 1-24, or map label" },
		{ "hunger",          "hunger",                               "Toggle party hunger on/off" },
		{ "items",           "items",                                "Grant all key items: 99 torches/gems/keys, sextant, all stones/runes, 999900 food, 9999 gold" },
		{ "karma",           "karma",                                "Display current karma values for all 8 virtues" },
		{ "leave",           "leave",                                "Exit current location to parent map" },
		{ "location",        "location [y-pair x-pair | y x]",        "Display or set coords; letter-pairs AA-PP map to 0-255 each axis; numeric: world map 0-255, dungeon 0-7" },
		{ "lordbritish",     "lordbritish",                          "Teleport to Lord British's castle (19,8,0)" },
		{ "mixtures",        "mixtures",                             "Grant 99 of all spell mixtures" },
		{ "moon",            "moon [0-7]",                           "Advance to specific moon phase, or next phase if omitted" },
		{ "opacity",         "opacity",                              "Toggle transparency effects" },
		{ "overhead",        "overhead",                             "Toggle between normal/dungeon view and overhead gem view" },
		{ "reagents",        "reagents",                             "Grant 99 of all spell reagents" },
		{ "summon",          "summon <creature_name>",               "Spawn a named creature for combat" },
		{ "teleport",        "teleport <name|portal_num|label>",     "Alias for goto; portal 1-24 on world map" },
		{ "torch",           "torch",                                "Display remaining torch duration" },
		{ "transport",       "transport <s|h|b> [direction]",        "Spawn ship (s), horse (h), or balloon (b) in a direction" },
		{ "triggers",        "triggers",                             "List dungeon room triggers (coordinates + tile replacements); combat only" },
		{ "up",              "up",                                   "Move up one dungeon floor; floors 0-7, exits dungeon at floor 0" },
		{ "virtue",          "virtue [1-8]",                         "No arg: set all virtues to max. With arg: improve that virtue by 10" },
		{ "wind",            "wind <direction|lock|l>",              "Set wind direction or toggle wind lock" },
		{ nullptr, nullptr, nullptr }
	};

	if (argc < 2) {
		static const int SYNTAX_WIDTH = 36;
		static const int DESC_COL = SYNTAX_WIDTH + 1;
		const int descWidth = getCharsPerLine() - DESC_COL;
		const Common::String indent(DESC_COL, ' ');

		debugPrintf("%-*s %s\n", SYNTAX_WIDTH, "Syntax", "Description");
		debugPrintf("%-*s %s\n", SYNTAX_WIDTH, "------", "-----------");

		for (const HelpEntry *e = HELP_TABLE; e->name; ++e) {
			Common::String desc(e->desc);
			bool firstLine = true;

			while (!desc.empty()) {
				Common::String chunk;
				if ((int)desc.size() <= descWidth) {
					chunk = desc;
					desc.clear();
				} else {
					// Find last space within descWidth for a clean word break
					int breakPos = descWidth;
					while (breakPos > 0 && desc[breakPos] != ' ')
						breakPos--;
					if (breakPos == 0)
						breakPos = descWidth; // no space found: hard break
					chunk = desc.substr(0, breakPos);
					// Skip the space itself when advancing
					int skip = breakPos + (breakPos < (int)desc.size() && desc[breakPos] == ' ' ? 1 : 0);
					desc = desc.substr(skip);
				}

				if (firstLine) {
					debugPrintf("%-*s %s\n", SYNTAX_WIDTH, e->syntax, chunk.c_str());
					firstLine = false;
				} else {
					debugPrintf("%s%s\n", indent.c_str(), chunk.c_str());
				}
			}
		}
		return true;
	}

	Common::String target(argv[1]);
	for (const HelpEntry *e = HELP_TABLE; e->name; ++e) {
		if (target.equalsIgnoreCase(e->name)) {
			debugPrintf("Syntax:      %s\n", e->syntax);
			debugPrintf("Description: %s\n", e->desc);
			return true;
		}
	}

	debugPrintf("No help available for '%s'.\n", argv[1]);
	return true;
}

void Debugger::executeCommand(const Common::String &cmd) {
	// Split up the command, and form a const char * array
	Common::StringArray args;
	splitString(cmd, args);

	Common::Array<const char *> argv;
	for (uint idx = 0; idx < args.size(); ++idx)
		argv.push_back(args[idx].c_str());

	// Execute the command
	executeCommand(argv.size(), &argv[0]);
}

void Debugger::executeCommand(int argc, const char **argv) {
	if (argc <= 0)
		return;

	bool keepRunning = false;
	if (!handleCommand(argc, argv, keepRunning)) {
		debugPrintf("Unknown command - %s\n", argv[0]);
		keepRunning = true;
	}

	// If any message occurred, then we need to ensure the debugger is opened if it isn't already
	if (keepRunning)
		attach();
}

} // End of namespace Ultima4
} // End of namespace Ultima
