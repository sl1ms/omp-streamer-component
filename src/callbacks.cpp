/*
 * Copyright (C) 2017 Incognito
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "main.h"

#include "core.h"
#include "streamer_component_api.h"

bool Streamer_OnPlayerConnect(int playerid)
{
	if (playerid >= 0 && playerid < MAX_PLAYERS)
	{
		auto &players = core->getData()->players;
		if (players.find(playerid) == players.end())
		{
			players.insert(std::make_pair(playerid, Player(playerid)));
		}
	}
	return true;
}

bool Streamer_OnPlayerDisconnect(int playerid, int /*reason*/)
{
	core->getData()->players.erase(playerid);
	return true;
}

bool Streamer_OnPlayerSpawn(int playerid)
{
	auto it = core->getData()->players.find(playerid);
	if (it != core->getData()->players.end())
	{
		it->second.requestingClass = false;
	}
	return true;
}

bool Streamer_OnPlayerRequestClass(int playerid, int /*classid*/)
{
	auto it = core->getData()->players.find(playerid);
	if (it != core->getData()->players.end())
	{
		it->second.requestingClass = true;
	}
	return true;
}

bool Streamer_OnPlayerEnterCheckpoint(int playerid)
{
	auto it = core->getData()->players.find(playerid);
	if (it == core->getData()->players.end()) return true;
	Player &player = it->second;
	if (player.activeCheckpoint == player.visibleCheckpoint) return true;

	int checkpointid = player.visibleCheckpoint;
	player.activeCheckpoint = checkpointid;
	for (auto* h : GetStreamerEventHandlers()) h->onPlayerEnterDynamicCheckpoint(playerid, checkpointid);
	return true;
}

bool Streamer_OnPlayerLeaveCheckpoint(int playerid)
{
	auto it = core->getData()->players.find(playerid);
	if (it == core->getData()->players.end()) return true;
	Player &player = it->second;
	if (player.activeCheckpoint != player.visibleCheckpoint) return true;

	int checkpointid = player.activeCheckpoint;
	player.activeCheckpoint = 0;
	for (auto* h : GetStreamerEventHandlers()) h->onPlayerLeaveDynamicCheckpoint(playerid, checkpointid);
	return true;
}

bool Streamer_OnPlayerEnterRaceCheckpoint(int playerid)
{
	auto it = core->getData()->players.find(playerid);
	if (it == core->getData()->players.end()) return true;
	Player &player = it->second;
	if (player.activeRaceCheckpoint == player.visibleRaceCheckpoint) return true;

	int checkpointid = player.visibleRaceCheckpoint;
	player.activeRaceCheckpoint = checkpointid;
	for (auto* h : GetStreamerEventHandlers()) h->onPlayerEnterDynamicRaceCheckpoint(playerid, checkpointid);
	return true;
}

bool Streamer_OnPlayerLeaveRaceCheckpoint(int playerid)
{
	auto it = core->getData()->players.find(playerid);
	if (it == core->getData()->players.end()) return true;
	Player &player = it->second;
	if (player.activeRaceCheckpoint != player.visibleRaceCheckpoint) return true;

	int checkpointid = player.activeRaceCheckpoint;
	player.activeRaceCheckpoint = 0;
	for (auto* h : GetStreamerEventHandlers()) h->onPlayerLeaveDynamicRaceCheckpoint(playerid, checkpointid);
	return true;
}

bool Streamer_OnPlayerPickUpPickup(int playerid, int pickupid)
{
	for (const auto &entry : core->getData()->internalPickups)
	{
		if (entry.second == pickupid)
		{
			for (auto* h : GetStreamerEventHandlers()) h->onPlayerPickUpDynamicPickup(playerid, entry.first.first);
			break;
		}
	}
	return true;
}

bool Streamer_OnPlayerEditObject(int playerid, bool playerobject, int objectid, int response, float fX, float fY, float fZ, float fRotX, float fRotY, float fRotZ)
{
	if (!playerobject) return false;

	auto playerIt = core->getData()->players.find(playerid);
	if (playerIt == core->getData()->players.end()) return false;

	for (const auto &internal : playerIt->second.internalObjects)
	{
		if (internal.second != objectid) continue;

		int dynObjectId = internal.first;
		if (response == EDIT_RESPONSE_CANCEL || response == EDIT_RESPONSE_FINAL)
		{
			auto objIt = core->getData()->objects.find(dynObjectId);
			if (objIt != core->getData()->objects.end())
			{
				auto &obj = objIt->second;
				if (obj->comparableStreamDistance < STREAMER_STATIC_DISTANCE_CUTOFF
					&& obj->originalComparableStreamDistance > STREAMER_STATIC_DISTANCE_CUTOFF)
				{
					obj->comparableStreamDistance = obj->originalComparableStreamDistance;
					obj->originalComparableStreamDistance = -1.0f;
				}
			}
		}
		for (auto* h : GetStreamerEventHandlers())
		{
			if (h->onPlayerEditDynamicObject(playerid, dynObjectId, response, fX, fY, fZ, fRotX, fRotY, fRotZ)
				== StreamerHandlerResult::Consume) break;
		}
		return true;
	}
	return false;
}

bool Streamer_OnPlayerSelectObject(int playerid, int type, int objectid, int modelid, float x, float y, float z)
{
	if (type != SELECT_OBJECT_PLAYER_OBJECT) return false;

	auto playerIt = core->getData()->players.find(playerid);
	if (playerIt == core->getData()->players.end()) return false;

	for (const auto &internal : playerIt->second.internalObjects)
	{
		if (internal.second != objectid) continue;
		for (auto* h : GetStreamerEventHandlers())
		{
			if (h->onPlayerSelectDynamicObject(playerid, internal.first, modelid, x, y, z)
				== StreamerHandlerResult::Consume) break;
		}
		return true;
	}
	return false;
}

bool Streamer_OnPlayerWeaponShot(int playerid, int weaponid, int hittype, int hitid, float x, float y, float z)
{
	if (hittype != BULLET_HIT_TYPE_PLAYER_OBJECT) return true;

	auto playerIt = core->getData()->players.find(playerid);
	if (playerIt == core->getData()->players.end()) return true;

	for (const auto &internal : playerIt->second.internalObjects)
	{
		if (internal.second != hitid) continue;
		bool allow = true;
		for (auto* h : GetStreamerEventHandlers())
		{
			if (h->onPlayerShootDynamicObject(playerid, weaponid, internal.first, x, y, z)
				== StreamerHandlerResult::Veto) allow = false;
		}
		return allow;
	}
	return true;
}

bool Streamer_OnPlayerGiveDamageActor(int playerid, int actorid, float amount, int weaponid, int bodypart)
{
	for (const auto &entry : core->getData()->internalActors)
	{
		if (entry.second != actorid) continue;
		for (auto* h : GetStreamerEventHandlers())
		{
			if (h->onPlayerGiveDamageDynamicActor(playerid, entry.first.first, amount, weaponid, bodypart)
				== StreamerHandlerResult::Consume) break;
		}
		return true;
	}
	return false;
}

bool Streamer_OnActorStreamIn(int actorid, int forplayerid)
{
	for (const auto &entry : core->getData()->internalActors)
	{
		if (entry.second == actorid)
		{
			for (auto* h : GetStreamerEventHandlers()) h->onDynamicActorStreamIn(entry.first.first, forplayerid);
			break;
		}
	}
	return true;
}

bool Streamer_OnActorStreamOut(int actorid, int forplayerid)
{
	for (const auto &entry : core->getData()->internalActors)
	{
		if (entry.second == actorid)
		{
			for (auto* h : GetStreamerEventHandlers()) h->onDynamicActorStreamOut(entry.first.first, forplayerid);
			break;
		}
	}
	return true;
}
