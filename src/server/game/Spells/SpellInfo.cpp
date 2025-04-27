/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SpellInfo.h"
#include "Chat.h"
#include "ConditionMgr.h"
#include "Corpse.h"
#include "DBCStores.h"
#include "LootMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"

//npcbot
#include "botmgr.h"
#include "botspell.h"
//end npcbot

static const std::unordered_set<uint32> VIP_SPELLS = {
//  月神  自由行动药剂  VIP卡技能------------------
    6724,6615,85411,85412,85413,85414,85415,85416,85417,85418,85419,85420,85421,85422,85423,85424,85425,85426,85427,85428,85429,85430,85431,85432,85433,85434,85435,85436,85437,85438,85439,85440,85441,85442,85443,85444,85445,85446,85447,85448,85449,85450,85451,85452,85453,85454,85455,85456,85457,85458,85459,85460,85461,85462,85463,85464,85465,85466,85467,85468,85469,85470,85471,85472,85473,85474,85475,85476,85477,85478,85479,85480,85481,85482,85483,85484,85485,85486,85487,85488,85489,85490,85491,85492,85493,85494,85550,85551,85552,85553,85554,85555,85556,85557,85558,85559,85811,85812,85813,85814,85815,85816,85817,85818,85819,85820,85821,85822,85823,85824,85825,85826,85827,85828,85829,85830,85831,85832,85833  // 所有VIP技能ID
};

uint32 GetTargetFlagMask(SpellTargetObjectTypes objType)
{
    switch (objType)
    {
        case TARGET_OBJECT_TYPE_DEST:
            return TARGET_FLAG_DEST_LOCATION;
        case TARGET_OBJECT_TYPE_UNIT_AND_DEST:
            return TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_UNIT;
        case TARGET_OBJECT_TYPE_CORPSE_ALLY:
            return TARGET_FLAG_CORPSE_ALLY;
        case TARGET_OBJECT_TYPE_CORPSE_ENEMY:
            return TARGET_FLAG_CORPSE_ENEMY;
        case TARGET_OBJECT_TYPE_CORPSE:
            return TARGET_FLAG_CORPSE_ALLY | TARGET_FLAG_CORPSE_ENEMY;
        case TARGET_OBJECT_TYPE_UNIT:
            return TARGET_FLAG_UNIT;
        case TARGET_OBJECT_TYPE_GOBJ:
            return TARGET_FLAG_GAMEOBJECT;
        case TARGET_OBJECT_TYPE_GOBJ_ITEM:
            return TARGET_FLAG_GAMEOBJECT_ITEM;
        case TARGET_OBJECT_TYPE_ITEM:
            return TARGET_FLAG_ITEM;
        case TARGET_OBJECT_TYPE_SRC:
            return TARGET_FLAG_SOURCE_LOCATION;
        default:
            return TARGET_FLAG_NONE;
    }
}

SpellImplicitTargetInfo::SpellImplicitTargetInfo(uint32 target)
{
    _target = Targets(target);
}

bool SpellImplicitTargetInfo::IsArea() const
{
    return GetSelectionCategory() == TARGET_SELECT_CATEGORY_AREA || GetSelectionCategory() == TARGET_SELECT_CATEGORY_CONE;
}

SpellTargetSelectionCategories SpellImplicitTargetInfo::GetSelectionCategory() const
{
    return _data[_target].SelectionCategory;
}

SpellTargetReferenceTypes SpellImplicitTargetInfo::GetReferenceType() const
{
    return _data[_target].ReferenceType;
}

SpellTargetObjectTypes SpellImplicitTargetInfo::GetObjectType() const
{
    return _data[_target].ObjectType;
}

SpellTargetCheckTypes SpellImplicitTargetInfo::GetCheckType() const
{
    return _data[_target].SelectionCheckType;
}

SpellTargetDirectionTypes SpellImplicitTargetInfo::GetDirectionType() const
{
    return _data[_target].DirectionType;
}

float SpellImplicitTargetInfo::CalcDirectionAngle() const
{
    switch (GetDirectionType())
    {
        case TARGET_DIR_FRONT:
            return 0.0f;
        case TARGET_DIR_BACK:
            return static_cast<float>(M_PI);
        case TARGET_DIR_RIGHT:
            return static_cast<float>(-M_PI / 2);
        case TARGET_DIR_LEFT:
            return static_cast<float>(M_PI / 2);
        case TARGET_DIR_FRONT_RIGHT:
            return static_cast<float>(-M_PI / 4);
        case TARGET_DIR_BACK_RIGHT:
            return static_cast<float>(-3 * M_PI / 4);
        case TARGET_DIR_BACK_LEFT:
            return static_cast<float>(3 * M_PI / 4);
        case TARGET_DIR_FRONT_LEFT:
            return static_cast<float>(M_PI / 4);
        case TARGET_DIR_RANDOM:
            return float(rand_norm()) * static_cast<float>(2 * M_PI);
        default:
            return 0.0f;
    }
}

Targets SpellImplicitTargetInfo::GetTarget() const
{
    return _target;
}

uint32 SpellImplicitTargetInfo::GetExplicitTargetMask(bool& srcSet, bool& dstSet) const
{
    uint32 targetMask = 0;
    if (GetTarget() == TARGET_DEST_TRAJ)
    {
        if (!srcSet)
            targetMask = TARGET_FLAG_SOURCE_LOCATION;
        if (!dstSet)
            targetMask |= TARGET_FLAG_DEST_LOCATION;
    }
    else
    {
        switch (GetReferenceType())
        {
            case TARGET_REFERENCE_TYPE_SRC:
                if (srcSet)
                    break;
                targetMask = TARGET_FLAG_SOURCE_LOCATION;
                break;
            case TARGET_REFERENCE_TYPE_DEST:
                if (dstSet)
                    break;
                targetMask = TARGET_FLAG_DEST_LOCATION;
                break;
            case TARGET_REFERENCE_TYPE_TARGET:
                switch (GetObjectType())
                {
                    case TARGET_OBJECT_TYPE_GOBJ:
                        targetMask = TARGET_FLAG_GAMEOBJECT;
                        break;
                    case TARGET_OBJECT_TYPE_GOBJ_ITEM:
                        targetMask = TARGET_FLAG_GAMEOBJECT_ITEM;
                        break;
                    case TARGET_OBJECT_TYPE_UNIT_AND_DEST:
                    case TARGET_OBJECT_TYPE_UNIT:
                    case TARGET_OBJECT_TYPE_DEST:
                        switch (GetCheckType())
                        {
                            case TARGET_CHECK_ENEMY:
                                targetMask = TARGET_FLAG_UNIT_ENEMY;
                                break;
                            case TARGET_CHECK_ALLY:
                                targetMask = TARGET_FLAG_UNIT_ALLY;
                                break;
                            case TARGET_CHECK_PARTY:
                                targetMask = TARGET_FLAG_UNIT_PARTY;
                                break;
                            case TARGET_CHECK_RAID:
                                targetMask = TARGET_FLAG_UNIT_RAID;
                                break;
                            case TARGET_CHECK_PASSENGER:
                                targetMask = TARGET_FLAG_UNIT_PASSENGER;
                                break;
                            case TARGET_CHECK_RAID_CLASS:
                                [[fallthrough]];
                            default:
                                targetMask = TARGET_FLAG_UNIT;
                                break;
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    switch (GetObjectType())
    {
        case TARGET_OBJECT_TYPE_SRC:
            srcSet = true;
            break;
        case TARGET_OBJECT_TYPE_DEST:
        case TARGET_OBJECT_TYPE_UNIT_AND_DEST:
            dstSet = true;
            break;
        default:
            break;
    }
    return targetMask;
}

std::array<SpellImplicitTargetInfo::StaticData, TOTAL_SPELL_TARGETS> SpellImplicitTargetInfo::_data =
{ {
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        //
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 1 TARGET_UNIT_CASTER
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_ENEMY,    TARGET_DIR_NONE},        // 2 TARGET_UNIT_NEARBY_ENEMY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_ALLY,     TARGET_DIR_NONE},        // 3 TARGET_UNIT_NEARBY_ALLY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_PARTY,    TARGET_DIR_NONE},        // 4 TARGET_UNIT_NEARBY_PARTY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 5 TARGET_UNIT_PET
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_ENEMY,    TARGET_DIR_NONE},        // 6 TARGET_UNIT_TARGET_ENEMY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_SRC,    TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_ENTRY,    TARGET_DIR_NONE},        // 7 TARGET_UNIT_SRC_AREA_ENTRY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_ENTRY,    TARGET_DIR_NONE},        // 8 TARGET_UNIT_DEST_AREA_ENTRY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 9 TARGET_DEST_HOME
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 10
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_SRC,    TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 11 TARGET_UNIT_SRC_AREA_UNK_11
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 12
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 13
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 14
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_SRC,    TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_ENEMY,    TARGET_DIR_NONE},        // 15 TARGET_UNIT_SRC_AREA_ENEMY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_ENEMY,    TARGET_DIR_NONE},        // 16 TARGET_UNIT_DEST_AREA_ENEMY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 17 TARGET_DEST_DB
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 18 TARGET_DEST_CASTER
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 19
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_PARTY,    TARGET_DIR_NONE},        // 20 TARGET_UNIT_CASTER_AREA_PARTY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_ALLY,     TARGET_DIR_NONE},        // 21 TARGET_UNIT_TARGET_ALLY
    {TARGET_OBJECT_TYPE_SRC,  TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 22 TARGET_SRC_CASTER
    {TARGET_OBJECT_TYPE_GOBJ, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 23 TARGET_GAMEOBJECT_TARGET
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_ENEMY,    TARGET_DIR_FRONT},       // 24 TARGET_UNIT_CONE_ENEMY_24
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 25 TARGET_UNIT_TARGET_ANY
    {TARGET_OBJECT_TYPE_GOBJ_ITEM, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT, TARGET_DIR_NONE},        // 26 TARGET_GAMEOBJECT_ITEM_TARGET
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 27 TARGET_UNIT_MASTER
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_ENEMY,    TARGET_DIR_NONE},        // 28 TARGET_DEST_DYNOBJ_ENEMY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_ALLY,     TARGET_DIR_NONE},        // 29 TARGET_DEST_DYNOBJ_ALLY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_SRC,    TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_ALLY,     TARGET_DIR_NONE},        // 30 TARGET_UNIT_SRC_AREA_ALLY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_ALLY,     TARGET_DIR_NONE},        // 31 TARGET_UNIT_DEST_AREA_ALLY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_LEFT},  // 32 TARGET_DEST_CASTER_SUMMON
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_SRC,    TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_PARTY,    TARGET_DIR_NONE},        // 33 TARGET_UNIT_SRC_AREA_PARTY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_PARTY,    TARGET_DIR_NONE},        // 34 TARGET_UNIT_DEST_AREA_PARTY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_PARTY,    TARGET_DIR_NONE},        // 35 TARGET_UNIT_TARGET_PARTY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 36 TARGET_DEST_CASTER_36
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_LAST,   TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_PARTY,    TARGET_DIR_NONE},        // 37 TARGET_UNIT_LASTTARGET_AREA_PARTY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_ENTRY,    TARGET_DIR_NONE},        // 38 TARGET_UNIT_NEARBY_ENTRY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 39 TARGET_DEST_CASTER_FISHING
    {TARGET_OBJECT_TYPE_GOBJ, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_ENTRY,    TARGET_DIR_NONE},        // 40 TARGET_GAMEOBJECT_NEARBY_ENTRY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_RIGHT}, // 41 TARGET_DEST_CASTER_FRONT_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK_RIGHT},  // 42 TARGET_DEST_CASTER_BACK_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK_LEFT},   // 43 TARGET_DEST_CASTER_BACK_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_LEFT},  // 44 TARGET_DEST_CASTER_FRONT_LEFT
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_ALLY,     TARGET_DIR_NONE},        // 45 TARGET_UNIT_TARGET_CHAINHEAL_ALLY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_ENTRY,    TARGET_DIR_NONE},        // 46 TARGET_DEST_NEARBY_ENTRY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT},       // 47 TARGET_DEST_CASTER_FRONT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK},        // 48 TARGET_DEST_CASTER_BACK
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RIGHT},       // 49 TARGET_DEST_CASTER_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_LEFT},        // 50 TARGET_DEST_CASTER_LEFT
    {TARGET_OBJECT_TYPE_GOBJ, TARGET_REFERENCE_TYPE_SRC,    TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 51 TARGET_GAMEOBJECT_SRC_AREA
    {TARGET_OBJECT_TYPE_GOBJ, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 52 TARGET_GAMEOBJECT_DEST_AREA
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_ENEMY,    TARGET_DIR_NONE},        // 53 TARGET_DEST_TARGET_ENEMY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_ENEMY,    TARGET_DIR_FRONT},       // 54 TARGET_UNIT_CONE_ENEMY_54
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 55 TARGET_DEST_CASTER_FRONT_LEAP
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_RAID,     TARGET_DIR_NONE},        // 56 TARGET_UNIT_CASTER_AREA_RAID
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_RAID,     TARGET_DIR_NONE},        // 57 TARGET_UNIT_TARGET_RAID
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_NEARBY,  TARGET_CHECK_RAID,     TARGET_DIR_NONE},        // 58 TARGET_UNIT_NEARBY_RAID
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_ALLY,     TARGET_DIR_FRONT},       // 59 TARGET_UNIT_CONE_ALLY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_ENTRY,    TARGET_DIR_FRONT},       // 60 TARGET_UNIT_CONE_ENTRY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_AREA,    TARGET_CHECK_RAID_CLASS, TARGET_DIR_NONE},      // 61 TARGET_UNIT_TARGET_AREA_RAID_CLASS
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 62 TARGET_UNK_62
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 63 TARGET_DEST_TARGET_ANY
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT},       // 64 TARGET_DEST_TARGET_FRONT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK},        // 65 TARGET_DEST_TARGET_BACK
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RIGHT},       // 66 TARGET_DEST_TARGET_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_LEFT},        // 67 TARGET_DEST_TARGET_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_RIGHT}, // 68 TARGET_DEST_TARGET_FRONT_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK_RIGHT},  // 69 TARGET_DEST_TARGET_BACK_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK_LEFT},   // 70 TARGET_DEST_TARGET_BACK_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_LEFT},  // 71 TARGET_DEST_TARGET_FRONT_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RANDOM},      // 72 TARGET_DEST_CASTER_RANDOM
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RANDOM},      // 73 TARGET_DEST_CASTER_RADIUS
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RANDOM},      // 74 TARGET_DEST_TARGET_RANDOM
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RANDOM},      // 75 TARGET_DEST_TARGET_RADIUS
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CHANNEL, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 76 TARGET_DEST_CHANNEL_TARGET
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CHANNEL, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 77 TARGET_UNIT_CHANNEL_TARGET
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT},       // 78 TARGET_DEST_DEST_FRONT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK},        // 79 TARGET_DEST_DEST_BACK
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RIGHT},       // 80 TARGET_DEST_DEST_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_LEFT},        // 81 TARGET_DEST_DEST_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_RIGHT}, // 82 TARGET_DEST_DEST_FRONT_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK_RIGHT},  // 83 TARGET_DEST_DEST_BACK_RIGHT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_BACK_LEFT},   // 84 TARGET_DEST_DEST_BACK_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT_LEFT},  // 85 TARGET_DEST_DEST_FRONT_LEFT
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RANDOM},      // 86 TARGET_DEST_DEST_RANDOM
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 87 TARGET_DEST_DEST
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 88 TARGET_DEST_DYNOBJ_NONE
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_TRAJ,    TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},           // 89 TARGET_DEST_TRAJ
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 90 TARGET_UNIT_TARGET_MINIPET
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_RANDOM},      // 91 TARGET_DEST_DEST_RADIUS
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 92 TARGET_UNIT_SUMMONER
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_ENEMY,    TARGET_DIR_FRONT},       // 93 TARGET_CORPSE_SRC_AREA_ENEMY
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 94 TARGET_UNIT_VEHICLE
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_TARGET, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_PASSENGER, TARGET_DIR_NONE},       // 95 TARGET_UNIT_TARGET_PASSENGER
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 96 TARGET_UNIT_PASSENGER_0
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 97 TARGET_UNIT_PASSENGER_1
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 98 TARGET_UNIT_PASSENGER_2
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 99 TARGET_UNIT_PASSENGER_3
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 100 TARGET_UNIT_PASSENGER_4
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 101 TARGET_UNIT_PASSENGER_5
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 102 TARGET_UNIT_PASSENGER_6
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_DEFAULT, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 103 TARGET_UNIT_PASSENGER_7
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_ENEMY,    TARGET_DIR_FRONT},       // 104 TARGET_UNIT_CONE_ENEMY_104
    {TARGET_OBJECT_TYPE_UNIT, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 105 TARGET_UNIT_UNK_105
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CHANNEL, TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 106 TARGET_DEST_CHANNEL_CASTER
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_DEST,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 107 TARGET_UNK_DEST_AREA_UNK_107
    {TARGET_OBJECT_TYPE_GOBJ, TARGET_REFERENCE_TYPE_CASTER, TARGET_SELECT_CATEGORY_CONE,    TARGET_CHECK_DEFAULT,  TARGET_DIR_FRONT},       // 108 TARGET_GAMEOBJECT_CONE
    {TARGET_OBJECT_TYPE_NONE, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 109
    {TARGET_OBJECT_TYPE_DEST, TARGET_REFERENCE_TYPE_NONE,   TARGET_SELECT_CATEGORY_NYI,     TARGET_CHECK_DEFAULT,  TARGET_DIR_NONE},        // 110 TARGET_DEST_UNK_110
} };

SpellEffectInfo::SpellEffectInfo(SpellEntry const* spellEntry, SpellInfo const* spellInfo, uint8 effIndex)
{
    _spellInfo = spellInfo;
    _effIndex = effIndex;
    Effect = spellEntry->Effect[effIndex];
    ApplyAuraName = spellEntry->EffectApplyAuraName[effIndex];
    Amplitude = spellEntry->EffectAmplitude[effIndex];
    DieSides = spellEntry->EffectDieSides[effIndex];
    RealPointsPerLevel = spellEntry->EffectRealPointsPerLevel[effIndex];
    BasePoints = spellEntry->EffectBasePoints[effIndex];
    PointsPerComboPoint = spellEntry->EffectPointsPerComboPoint[effIndex];
    ValueMultiplier = spellEntry->EffectValueMultiplier[effIndex];
    DamageMultiplier = spellEntry->EffectDamageMultiplier[effIndex];
    BonusMultiplier = spellEntry->EffectBonusMultiplier[effIndex];
    MiscValue = spellEntry->EffectMiscValue[effIndex];
    MiscValueB = spellEntry->EffectMiscValueB[effIndex];
    Mechanic = Mechanics(spellEntry->EffectMechanic[effIndex]);
    TargetA = SpellImplicitTargetInfo(spellEntry->EffectImplicitTargetA[effIndex]);
    TargetB = SpellImplicitTargetInfo(spellEntry->EffectImplicitTargetB[effIndex]);
    RadiusEntry = spellEntry->EffectRadiusIndex[effIndex] ? sSpellRadiusStore.LookupEntry(spellEntry->EffectRadiusIndex[effIndex]) : nullptr;
    ChainTarget = spellEntry->EffectChainTarget[effIndex];
    ItemType = spellEntry->EffectItemType[effIndex];
    TriggerSpell = spellEntry->EffectTriggerSpell[effIndex];
    SpellClassMask = spellEntry->EffectSpellClassMask[effIndex];
    ImplicitTargetConditions = nullptr;
}

bool SpellEffectInfo::IsEffect() const
{
    return Effect != 0;
}

bool SpellEffectInfo::IsEffect(SpellEffects effectName) const
{
    return Effect == effectName;
}

bool SpellEffectInfo::IsAura() const
{
    return (IsUnitOwnedAuraEffect() || Effect == SPELL_EFFECT_PERSISTENT_AREA_AURA) && ApplyAuraName != 0;
}

bool SpellEffectInfo::IsAura(AuraType aura) const
{
    return IsAura() && ApplyAuraName == uint32(aura);
}

bool SpellEffectInfo::IsTargetingArea() const
{
    return TargetA.IsArea() || TargetB.IsArea();
}

bool SpellEffectInfo::IsAreaAuraEffect() const
{
    if (Effect == SPELL_EFFECT_APPLY_AREA_AURA_PARTY    ||
            Effect == SPELL_EFFECT_APPLY_AREA_AURA_RAID     ||
            Effect == SPELL_EFFECT_APPLY_AREA_AURA_FRIEND   ||
            Effect == SPELL_EFFECT_APPLY_AREA_AURA_ENEMY    ||
            Effect == SPELL_EFFECT_APPLY_AREA_AURA_PET      ||
            Effect == SPELL_EFFECT_APPLY_AREA_AURA_OWNER)
        return true;
    return false;
}

bool SpellEffectInfo::IsFarUnitTargetEffect() const
{
    return (Effect == SPELL_EFFECT_SUMMON_PLAYER)
           || (Effect == SPELL_EFFECT_SUMMON_RAF_FRIEND)
           || (Effect == SPELL_EFFECT_RESURRECT)
           || (Effect == SPELL_EFFECT_RESURRECT_NEW)
           /*|| (Effect == SPELL_EFFECT_SKIN_PLAYER_CORPSE) Xinef: This is not Far Unit Target Effect*/;
}

bool SpellEffectInfo::IsFarDestTargetEffect() const
{
    return Effect == SPELL_EFFECT_TELEPORT_UNITS;
}

bool SpellEffectInfo::IsUnitOwnedAuraEffect() const
{
    return IsAreaAuraEffect() || Effect == SPELL_EFFECT_APPLY_AURA;
}

int32 SpellEffectInfo::CalcValue(Unit const* caster, int32 const* bp, Unit const* /*target*/) const
{
    float basePointsPerLevel = RealPointsPerLevel;
    int32 basePoints = bp ? *bp : BasePoints;
    int32 randomPoints = int32(DieSides);

    bool canusezuoqi = false;
    if (caster)
    {
        //获得玩家当前是否在战场或竞技场中
        if (caster->IsPlayer() && !caster->ToPlayer()->GetSession()->IsBot())
            if (caster->IsInWorld() && caster->isActiveObject())
                if (caster->GetMap() && caster->GetAreaId() && caster->GetAreaId())
                    canusezuoqi = !caster->GetMap()->IsBattlegroundOrArena() && caster->GetAreaId() != 2177 && caster->GetAreaId() != 1741;
    }


    //如果是玩家的宠物
    if (caster)
    {
        if (caster->IsPet() && caster->IsHunterPet())
        {
            if (Unit* owner = caster->GetOwner())
            {
                if (owner->IsPlayer() && !((Player*)owner)->GetSession()->IsBot())
                {
                    //获得主人近战攻击强度整体加成
                    float ownerpower = 0.0f;
                    //如果主人是硬核模式
                    //if (((Player*)owner)->GetSession()->GetPlayer()->GetExtraFlags() & PLAYER_EXTRA_YH_MODEL || ((Player*)owner)->GetSession()->IsBot())//现在允许非硬核模式生效
                    //{
                    ownerpower = ((Player*)owner)->GetSession()->GetPlayer()->GetTotalAttackPowerValue(RANGED_ATTACK);//取远程攻强
                    //攻强除以3.3基本等于面板伤害，远程攻强除以4基本等于面板远程伤害，原设置满级猎人平射是100多，低吼是415，相当3倍平射伤害
                    //根据这个来3/4的远程攻强，是要给低吼增加的仇恨值
                    //335的60猎人宠物低吼默认仇恨是319，远程攻强7873打60木桩平射800，70木桩平射600，80木桩平射500
                    //如果按原版设定，低吼应当是平射的三倍，就是打60木桩仇恨2400，70木桩仇恨1800，80木桩仇恨1500，仇恨是固定的
                    //取最大值2400，就是319*7.5 ,7.5=7873/1050,所以公式改为:basePoints = basePoints + basePoints * (ownerpower / 1050);
                    if (_spellInfo)
                    {
                        if (_spellInfo->SpellIconID && _spellInfo->Id)
                        {
                            //低吼增加
                            if (_spellInfo->SpellIconID == 201)
                            {
                                switch (_spellInfo->Id)
                                {
                                case 2649:
                                case 14916:
                                case 14917:
                                case 14918:
                                case 14919:
                                case 14920:
                                case 14921:
                                case 27047:
                                    //LOG_ERROR("xx", "old basePoints: {} ", basePoints);//测试
                                    //basePoints = basePoints + ownerpower * 0.175;
                                    basePoints = basePoints + basePoints * (ownerpower / 1050);
                                    //LOG_ERROR("xx", "basePoints: {} ", basePoints);//测试
                                    break;
                                }


                            }
                        }
                    }
                    //}

                }
            }

        }

        //如果是SS宠物
        if (caster->IsPet() && caster->IsSummon())
        {
            if (Unit* owner = caster->GetOwner())
            {
                if (owner->IsPlayer() && !((Player*)owner)->GetSession()->IsBot())
                {
                    //获得主人法伤加成
                    float ownerfashang = 0.0f;
                    //如果主人是硬核模式
                    //if (((Player*)owner)->GetSession()->GetPlayer()->GetExtraFlags() & PLAYER_EXTRA_YH_MODEL || ((Player*)owner)->GetSession()->IsBot())//现在允许非硬核模式生效
                    //{
                        //取法伤中的最大值作为伤害加成参考
                    int32 DoneAdvertisedBenefit = (((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY) > ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE)) ? ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY) : ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
                    DoneAdvertisedBenefit = (DoneAdvertisedBenefit > ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_NATURE)) ? DoneAdvertisedBenefit : ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_NATURE);
                    DoneAdvertisedBenefit = (DoneAdvertisedBenefit > ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST)) ? DoneAdvertisedBenefit : ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST);
                    DoneAdvertisedBenefit = (DoneAdvertisedBenefit > ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)) ? DoneAdvertisedBenefit : ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW);
                    DoneAdvertisedBenefit = (DoneAdvertisedBenefit > ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE)) ? DoneAdvertisedBenefit : ((Player*)owner)->GetSession()->GetPlayer()->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE);
                    ownerfashang = float(DoneAdvertisedBenefit);

                    if (_spellInfo)
                    {
                        if (_spellInfo->SpellIconID && _spellInfo->Id)
                        {
                            //增加仇恨
                            switch (_spellInfo->Id)
                            {
                                //魅魔 安抚之吻
                            case 6360:
                            case 7813:
                            case 11784:
                            case 11785:
                            case 27275:
                                //虚空行者 折磨
                            case 3716:
                            case 7809:
                            case 7810:
                            case 7811:
                            case 11774:
                            case 11775:
                            case 27270:
                            case 47984:
                                //虚空行者 受难
                            case 17735:
                            case 17750:
                            case 17751:
                            case 17752:
                            case 27271:
                            case 33701:
                            case 47989:
                            case 47990:
                                //LOG_ERROR("xx", "old basePoints: {} ", basePoints);//测试
                                //basePoints = basePoints + basePoints * (ownerfashang / 1050);
                                basePoints = basePoints + basePoints * (ownerfashang / 500);
                                //LOG_ERROR("xx", "basePoints: {} ", basePoints);//测试
                                break;
                            }

                        }
                    }
                    //}

                }
            }

        }


        // 如果是图腾施法的，找到图腾的主人，如果主人是硬核转生，就提升图腾效果
        if (caster->GetTypeId() == TYPEID_UNIT && caster->IsTotem())
        {
            if (Unit* owner = caster->GetOwner())
            {
                //获得主人当前是否在战场或竞技场中
                bool ownercanusezuoqi = false;
                if (owner->IsInWorld() && owner->isActiveObject())
                    if (owner->IsPlayer() && !owner->ToPlayer()->GetSession()->IsBot())
                        if (owner->GetMap() && owner->GetAreaId() && owner->GetAreaId())
                            ownercanusezuoqi = !owner->GetMap()->IsBattlegroundOrArena() && owner->GetAreaId() != 2177 && owner->GetAreaId() != 1741;

                if (owner->IsPlayer() && ownercanusezuoqi)
                {
                    Player const* pownerPlayer = owner->ToPlayer();
                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "xbasePoints %f", basePoints);//测试，药水效果也可以到这里，比如泰坦合剂
                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "xSpellIconID %u", _spellInfo->SpellIconID);//测试
                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "xSpellid %u", _spellInfo->Id);//测试

                    //如果图腾的主人是硬核模式
                    float ownerAPpower = 0.0f;
                    float ownermeleeAPpower = 0.0f;
                    //if (pownerPlayer->GetExtraFlags() & PLAYER_EXTRA_YH_MODEL || pownerPlayer->GetSession()->IsBot())//现在允许非硬核模式生效
                    //{
                    ownerAPpower = pownerPlayer->GetTotalAttackPowerValue(RANGED_ATTACK);
                    ownermeleeAPpower = pownerPlayer->GetTotalAttackPowerValue(BASE_ATTACK);

                    //获得图腾主人的转生石个数
                    uint32 ownerzsstone = pownerPlayer->GetItemCount(30630, false);
                    //如果是满级机器人，随机转生石的数量
                    if (ownerzsstone == 0 && pownerPlayer->GetSession()->IsBot() && pownerPlayer->GetLevel() == sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
                    {
                        ownerzsstone = urand(0, 10);
                    }

                    switch (pownerPlayer->GetClass())
                    {
                    case CLASS_SHAMAN:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //萨满法力之泉图腾加成
                                if (_spellInfo->SpellIconID == 338 && _spellInfo->Id == 10494)
                                {
                                    if (ownerzsstone > 0)
                                        basePoints = basePoints + basePoints * ownerzsstone * 0.2;
                                }
                                //萨满法力之潮图腾加成
                                if (_spellInfo->SpellIconID == 94 && _spellInfo->Id == 17360)
                                {
                                    if (ownerzsstone > 0)
                                        basePoints = basePoints + basePoints * ownerzsstone * 0.2;
                                }
                                //萨满石肤图腾加成
                                if (_spellInfo->SpellIconID == 690 && _spellInfo->Id == 10405)
                                {
                                    if (ownerzsstone > 0)
                                        basePoints = basePoints + basePoints * ownerzsstone * 0.2;
                                }
                                //萨满风墙图腾加成
                                if (_spellInfo->SpellIconID == 174 && _spellInfo->Id == 15110)
                                {
                                    if (ownerzsstone > 0)
                                        basePoints = basePoints + basePoints * ownerzsstone * 0.2;
                                }
                                //萨满治疗之泉墙图腾加成
                                if (_spellInfo->SpellIconID == 1647 && _spellInfo->Id == 10461)
                                {
                                    if (ownerzsstone > 0)
                                        basePoints = basePoints + basePoints * ownerzsstone * 0.2;

                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "xnewbasePoints %f", basePoints);//测试 
                                }
                            }
                        }
                        break;
                    default:
                        break;
                    }
                    //}


                }

            }
        }



        if (caster->IsPlayer() && !caster->ToPlayer()->GetSession()->IsBot())
        //if (caster->IsPlayer() && canusezuoqi)//机器人也加成
        {
            //调试位置
            //if (caster->GetGUID().GetCounter()== 5238) {
            //    LOG_ERROR("xx", "1basePoints{} ", basePoints);//测试
            //    LOG_ERROR("xx", "1SpellIconID {} ", _spellInfo->SpellIconID);//测试
            //    LOG_ERROR("xx", "1Spellid {} ", _spellInfo->Id);//测试
            //}


            //if (_spellInfo->Id == 7712 || _spellInfo->Id == 7714) {
            //    LOG_ERROR("xx", "break ");//测试，已经骑上马，再次点击马的图标，会下马，打印到了这里
            //    std::abort();
            //}


            //LOG_ERROR("xx", "SpellFamilyFlags {} ", _spellInfo->SpellFamilyFlags[0]);//测试
            //LOG_ERROR("xx", "SpellFamilyName {} ", _spellInfo->SpellFamilyName);//测试

            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "1basePoints %f", basePoints);//测试，药水效果也可以到这里，比如泰坦合剂#####
            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "1SpellIconID %u", _spellInfo->SpellIconID);//测试#####
            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "1Spellid %u", _spellInfo->Id);//测试#####

            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "1GetExtraFlags %u", pPlayer->GetExtraFlags());//测试
            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "1playGetEntry %u", pPlayer->GetGUIDLow());//测试


            //对硬核玩家，以及不同职业的硬核玩家的伤害加成进行区别对待
            //如果玩家是硬核模式
            float APpower = 0.0f;
            float meleeAPpower = 0.0f;
            int32 armorbasePoints = 0;
            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetClass %u", pPlayer->GetGUIDLow());//测试
            //if(( ((Player*)this)->GetSession()->GetPlayer()->GetExtraFlags() & PLAYER_EXTRA_YH_MODEL ))//这种写法会崩，好像人物登录加载时有无法获取GetExtraFlags()的情况
            if (caster->ToPlayer()->GetExtraFlags() & PLAYER_EXTRA_YH_MODEL || caster->ToPlayer()->GetSession()->IsBot())
            {
                //LOG_ERROR("xx", "ApplyAuraName: {} ", ApplyAuraName);//测试
                //判断装备调用的光环技能是否为攻强，远程攻强，法伤，治疗的加成--------------------------
                if (ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER || ApplyAuraName == SPELL_AURA_MOD_RANGED_ATTACK_POWER
                    || ApplyAuraName == SPELL_AURA_MOD_DAMAGE_DONE || ApplyAuraName == SPELL_AURA_MOD_HEALING_DONE)
                {
                    //判断玩家是否装备了橙装戒指，并返回戒指的加成数值，如果有橙装，就动态根据加成数值改动攻强，远程攻强，法伤，治疗加成的百分比
                    uint32 getczring = caster->ToPlayer()->GetCZRingItem();
                    if (getczring > 0)
                    {
                        //LOG_ERROR("xx", "getczring: {} ", getczring);//测试
                        //LOG_ERROR("xx", "basePoints1: {} ", basePoints);//测试
                        basePoints = basePoints + basePoints * float(getczring) / 100.0f;
                        //LOG_ERROR("xx", "basePoints2: {} ", basePoints);//测试
                    }
                }
                //判断装备调用的光环技能是否为攻强，远程攻强，法伤，治疗的加成----------------end---------------


                APpower = caster->ToPlayer()->GetTotalAttackPowerValue(RANGED_ATTACK);
                meleeAPpower = caster->ToPlayer()->GetTotalAttackPowerValue(BASE_ATTACK);
                armorbasePoints = caster->ToPlayer()->GetResistance(SPELL_SCHOOL_NORMAL);

                //获得玩家的转生石个数
                uint16 zsstone = 0;
                //zsstone = caster->ToPlayer()->GetItemCount(70630, false);
                zsstone = caster->ToPlayer()->getZHUANSHENGNUM();
                //LOG_ERROR("xx", "spell-zsstone {}  ", zsstone);//测试

                //如果是机器人，随机转生石的数量
                if (zsstone == 0 && caster->ToPlayer()->GetSession()->IsBot() && caster->ToPlayer()->GetLevel() == sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
                {
                    zsstone = urand(0, 10);
                }
                //LOG_ERROR("xx", "spellrobot-zsstone {}  ", zsstone);//测试


                //所有硬核玩家，对以下技能的效果进行重新加成
                if (_spellInfo)
                {
                    if (_spellInfo->SpellIconID && _spellInfo->Id)
                    {
                        //泰坦合剂增加效果
                        if (_spellInfo->SpellIconID == 1639 && _spellInfo->Id == 17626)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.15;

                        }
                        //多重抗性合剂增加效果
                        if (_spellInfo->SpellIconID == 1641 && _spellInfo->Id == 17629)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //精炼智慧合剂增加效果
                        if (_spellInfo->SpellIconID == 1640 && _spellInfo->Id == 17627)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //超级能量合剂增加效果
                        if (_spellInfo->SpellIconID == 1643 && _spellInfo->Id == 17628)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //大红增加效果
                        if (_spellInfo->SpellIconID == 1 && _spellInfo->Id == 17534)
                        {
                            if (zsstone > 0)
                                if (basePoints == 1049)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //大蓝增加效果
                        if (_spellInfo->SpellIconID == 1 && _spellInfo->Id == 17531)
                        {
                            if (zsstone > 0)
                                if (basePoints == 1349)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }

                        //猫鼬药剂增加效果
                        if (_spellInfo->SpellIconID == 1609 && _spellInfo->Id == 17538)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //东泉火酒增加效果
                        if (_spellInfo->SpellIconID == 1570 && _spellInfo->Id == 17038)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //巨人药剂增加效果
                        if (_spellInfo->SpellIconID == 1337 && _spellInfo->Id == 11405)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //强效奥法药剂增加效果
                        if (_spellInfo->SpellIconID == 1686 && _spellInfo->Id == 17539)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //砂囊口香糖增加效果
                        if (_spellInfo->SpellIconID == 937 && _spellInfo->Id == 10693)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //砂囊口香糖增加效果
                        if (_spellInfo->SpellIconID == 47 && _spellInfo->Id == 10668)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //土狼兴奋剂糖增加效果
                        if (_spellInfo->SpellIconID == 63 && _spellInfo->Id == 10667)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //脑皮层混合饮料增加效果
                        if (_spellInfo->SpellIconID == 32 && _spellInfo->Id == 10692)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //厚甲蝎药粉增加效果
                        if (_spellInfo->SpellIconID == 111 && _spellInfo->Id == 10669)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.1;
                        }
                        //防御水晶增加效果
                        if (_spellInfo->SpellIconID == 1517 && _spellInfo->Id == 15233)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //精神水晶增加效果
                        if (_spellInfo->SpellIconID == 1515 && _spellInfo->Id == 15231)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //尖刺水晶增加效果
                        if (_spellInfo->SpellIconID == 283 && _spellInfo->Id == 15279)
                        {
                            if (zsstone > 0)
                            {
                                if (APpower > meleeAPpower)
                                {
                                    //basePoints = basePoints + basePoints * zsstone * 0.2 + APpower * 0.06;
                                    basePoints = basePoints + basePoints * zsstone * 0.1 + meleeAPpower * 0.05 + (APpower - meleeAPpower) * 0.03;//远程攻强算近战攻强3/5的加成
                                }
                                else
                                    basePoints = basePoints + basePoints * zsstone * 0.2 + meleeAPpower * 0.05;
                            }
                        }
                        //破甲水晶增加效果，int和uint相乘，会出现很大的数字的bug
                        if (_spellInfo->SpellIconID == 1518 && _spellInfo->Id == 15235)
                        {
                            //LOG_ERROR("xx", "basePoints: {} ", basePoints);//测试
                            //LOG_ERROR("xx", "basePointsx: {} ", basePoints * int32(zsstone * 0.2));//测试
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * int32(zsstone) * 0.2;

                            //LOG_ERROR("xx", "basePoints: {} ", basePoints);//测试
                        }
                        //赞扎之魂增加效果
                        if (_spellInfo->SpellIconID == 1315 && _spellInfo->Id == 24382)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //赞扎之光增加效果
                        if (_spellInfo->SpellIconID == 1602 && _spellInfo->Id == 24417)
                        {
                            if (zsstone > 0)
                                if (basePoints == 2)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //赞扎之速增加效果
                        if (_spellInfo->SpellIconID == 1597 && _spellInfo->Id == 24383)
                        {
                            if (zsstone > 0)
                                if (basePoints == 19)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //迅捷之风增加效果
                        if (_spellInfo->SpellIconID == 174 && _spellInfo->Id == 8385)
                        {
                            if (zsstone > 0)
                                if (basePoints == 39)
                                    basePoints = basePoints + basePoints * zsstone * 0.1;
                        }

                        //塔罗牌抗性增加效果
                        if (_spellInfo->SpellIconID == 1595 && _spellInfo->Id == 23769)
                        {
                            if (zsstone > 0)
                                if (basePoints == 24)
                                    basePoints = basePoints + basePoints * zsstone * 0.1;
                        }

                        //野猪之皮增加效果
                        if (_spellInfo->SpellIconID == 53 && _spellInfo->Id == 16610)
                        {
                            if (zsstone > 0)
                            {
                                if (basePoints != 24)
                                {
                                    if (APpower > meleeAPpower)
                                    {
                                        //basePoints = basePoints + basePoints * zsstone * 0.2 + APpower * 0.05 + armorbasePoints * 0.025;
                                        basePoints = basePoints + basePoints * zsstone * 0.1 + meleeAPpower * 0.05 + (APpower - meleeAPpower) * 0.03;//远程攻强算近战攻强3/5的加成
                                    }
                                    else
                                        basePoints = basePoints + basePoints * zsstone * 0.2 + meleeAPpower * 0.05 + armorbasePoints * 0.025;
                                }
                            }
                            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2basePoints %f", basePoints);//测试
                            //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2meleeAPpower %f", meleeAPpower);//测试
                        }
                        //沙漠肉丸子增加效果
                        if (_spellInfo->SpellIconID == 59 && _spellInfo->Id == 24799)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //魂能之击增加效果
                        if (_spellInfo->SpellIconID == 1538 && _spellInfo->Id == 16329)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //魂能之力增加效果
                        if (_spellInfo->SpellIconID == 1533 && _spellInfo->Id == 16323)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //魂能之诈增加效果
                        if (_spellInfo->SpellIconID == 1537 && _spellInfo->Id == 16327)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //强效石盾药剂增加效果
                        if (_spellInfo->SpellIconID == 1316 && _spellInfo->Id == 17540)
                        {
                            if (zsstone > 0)
                                basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //卓越巫师之油增加效果
                        if (_spellInfo->SpellIconID == 1 && _spellInfo->Id == 25113)
                        {
                            if (zsstone > 0)
                                if (basePoints == 35)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //卓越法力之油增加效果
                        if (_spellInfo->SpellIconID == 1 && _spellInfo->Id == 25116)
                        {
                            if (zsstone > 0)
                                if (basePoints == 12)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //牛皮糖增加效果
                        if (_spellInfo->SpellIconID == 1926 && _spellInfo->Id == 29334)
                        {
                            if (zsstone > 0)
                                if (basePoints == 43)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //仲夏腊肠增加效果
                        if (_spellInfo->SpellIconID == 1925 && _spellInfo->Id == 29333)
                        {
                            if (zsstone > 0)
                                if (basePoints == 29)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }

                        //火焰节之韧增加效果
                        if (_spellInfo->SpellIconID == 1923 && _spellInfo->Id == 29235)
                        {
                            if (zsstone > 0)
                                if (basePoints == 29)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        /*
                        //彩带之舞增加效果。硬核没必要加了，取消
                        if (_spellInfo->SpellIconID == 1919 && _spellInfo->Id == 29175)
                        {
                            if (zsstone > 0)
                                if (basePoints == 9)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        //稻草人的祈祷增加效果。硬核没必要加了，取消
                        if (_spellInfo->SpellIconID == 1816 && _spellInfo->Id == 24705)
                        {
                            if (zsstone > 0)
                                if (basePoints == 9)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;
                        }
                        */
                        //醉酒增加效果
                        if (_spellInfo->SpellIconID == 232 && _spellInfo->Id == 25947)
                        {
                            //LOG_ERROR("xx", "basePoints {}- {} ", basePoints, _spellInfo->Id);//测试
                            if (zsstone > 0)
                                if (basePoints == 49)
                                    basePoints = basePoints + basePoints * zsstone * 0.2;

                            //LOG_ERROR("xx", "xbasePoints {}- {} ", basePoints, _spellInfo->Id);//测试
                        }

                        /*
                        //风剑，雷霆之怒技能增加效果
                        if (_spellInfo->SpellIconID == 220 && _spellInfo->Id == 21992)
                        {
                            //if (zsstone > 0)
                            //	if (basePoints == 299)
                            //		basePoints = basePoints + basePoints * zsstone * 0.2;

                            if (zsstone > 0)
                            {
                                if (basePoints == 299)
                                    basePoints = basePoints + meleeAPpower * 0.1;//按3000攻强和转生的加成做成持平
                            }


                        }

                        //橙锤，萨弗拉斯 炎魔拉格纳罗斯之手技能增加效果
                        if (_spellInfo->SpellIconID == 183 && _spellInfo->Id == 21162)
                        {
                            if (zsstone > 0)
                            {
                                if (basePoints == 272)
                                    basePoints = basePoints + meleeAPpower * 0.1;
                                if (basePoints == 14)
                                    basePoints = basePoints + basePoints * zsstone * 0.1;
                                if (basePoints == 2)
                                    basePoints = basePoints + basePoints * zsstone * 0.1;
                            }


                        }
                        */

                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2basePoints %f", basePoints);//测试，药水效果也可以到这里，比如泰坦合剂
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2SpellIconID %u", _spellInfo->SpellIconID);//测试
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2Spellid %u", _spellInfo->Id);//测试
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2GetExtraFlags %u", pPlayer->GetExtraFlags());//测试
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "2playGetEntry %u", pPlayer->GetGUIDLow());//测试

                    }
                }

                //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "tttt");//测试
                //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "xxxxx %u", ((Player*)this)->GetSession()->GetPlayer()->GetExtraFlags());//测试，会崩
                //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetClass %u", pPlayer->GetClass());//测试
                //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "GetClass %u", pPlayer->GetGUIDLow());//测试
                if (caster->ToPlayer()->GetClass())
                {
                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "spellId %u", _spellInfo->Id);//测试，光环战斗回血，不会到这里

                    switch (caster->ToPlayer()->GetClass())
                    {
                    case CLASS_PRIEST:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //牧师神圣新星
                                //if (_spellInfo->Id == 27801)
                                if (_spellInfo->SpellIconID == 1874 && (_spellInfo->SpellFamilyFlags[0] & 0x400000))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.15;
                                }
                            }
                        }
                        break;
                    case CLASS_PALADIN:
                        if (_spellInfo)
                        {
                            /*
                            //这个版本骑士太强了，奉献不再加强
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //圣骑士的奉献
                                //if (_spellInfo->Id == 20924)
                                if (_spellInfo->SpellIconID == 51 && (_spellInfo->SpellFamilyFlags[0] & 0x20))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                            }
                            */
                        }
                        break;
                    case CLASS_MAGE:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //法师魔爆术
                                //if (_spellInfo->Id == 10202)
                                if (_spellInfo->SpellIconID == 122 && (_spellInfo->SpellFamilyFlags[0] & 0x1000))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.15;
                                }
                            }
                        }
                        break;
                    case CLASS_WARLOCK:
                        break;
                    case CLASS_SHAMAN:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //萨满闪电之盾加成26363
                                /*
                                if (_spellInfo->SpellIconID == 19 && _spellInfo->Id == 10432)
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                */
                                if (_spellInfo->SpellIconID == 62 && (_spellInfo->SpellFamilyFlags[0] & 0x400))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //萨满法力之泉图腾加成10497
                                if (_spellInfo->SpellIconID == 338 && (_spellInfo->SpellFamilyFlags[0] & 0x80000))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //萨满法力之潮图腾加成16190
                                if (_spellInfo->SpellIconID == 94 && (_spellInfo->SpellFamilyFlags[0] & 0x20000000))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //萨满石肤图腾加成10408
                                if (_spellInfo->SpellIconID == 690 && (_spellInfo->SpellFamilyFlags[0] & 0x20000000))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                ////萨满风墙图腾加成15112 没有找到
                                //if (_spellInfo->SpellIconID == 174 && _spellInfo->Id == 15112)
                                //{
                                //    if (zsstone > 0)
                                //        basePoints = basePoints + basePoints * zsstone * 0.1;
                                //}
                            }
                        }
                        break;
                    case CLASS_WARRIOR:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //战士撕裂加成11574
                                if (_spellInfo->SpellIconID == 245 && (_spellInfo->SpellFamilyFlags[0] & 0x20))
                                {
                                    if (basePoints != 0)
                                        basePoints = basePoints + meleeAPpower * 0.05;
                                }
                                //战士斩杀加成20662
                                if (_spellInfo->SpellIconID == 1648 && (_spellInfo->SpellFamilyFlags[0] & 0x20000000))
                                {
                                    if (basePoints != 0)
                                        basePoints = basePoints + meleeAPpower * 0.24;
                                }
                                //战士拳击加成6554
                                if (_spellInfo->SpellIconID == 756 && (_spellInfo->SpellFamilyFlags[0] & 0x8))
                                {
                                    if (basePoints != 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //战士复仇加成25288
                                if (_spellInfo->SpellIconID == 562 && (_spellInfo->SpellFamilyFlags[0] & 0x400))
                                {
                                    if (basePoints != 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //战士盾击加成1672
                                if (_spellInfo->SpellIconID == 280 && (_spellInfo->SpellFamilyFlags[0] & 0x800))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //战士盾牌猛击加成23925
                                if (_spellInfo->SpellIconID == 413 && (_spellInfo->Id == 23925 || _spellInfo->Id == 25258 || _spellInfo->Id == 30356 || _spellInfo->Id == 47487 || _spellInfo->Id == 47488))
                                {
                                    if (basePoints != 0)
                                        basePoints = basePoints + meleeAPpower * 0.15;
                                }

                                //战士致死打击加成21553
                                if (_spellInfo->SpellIconID == 564 && (_spellInfo->SpellFamilyFlags[0] & 0x2000000))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }

                                //战士惩戒痛击加成20560
                                if (_spellInfo->SpellIconID == 1477 && (_spellInfo->SpellFamilyFlags[0] & 0x8000000))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //战士雷霆一击加成11581
                                if (_spellInfo->SpellIconID == 199 && (_spellInfo->SpellFamilyFlags[0] & 0x80))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.05;
                                }
                                /*
                                //新版本嗜血技能改动了，先不再加强
                                //战士嗜血加成
                                if (_spellInfo->SpellIconID == 38 && _spellInfo->Id == 23881)
                                {
                                        if (basePoints == 49)
                                            basePoints = basePoints + meleeAPpower * 0.08;
                                }
                                */
                                //战士嗜血回血加成，实际无加血提高效果，百分比1改为2是有效的，但加血的值在这里没有打印出来
                                //if (_spellInfo->SpellIconID == 38 && _spellInfo->Id == 23885)
                                //{
                                //    LOG_ERROR("xx", "ne2wbasePoints {} ", basePoints);//测试
                                //    if (basePoints == 1)
                                //        if (zsstone > 0)
                                //            basePoints = basePoints + basePoints * zsstone * 0.1;
                                //    LOG_ERROR("xx", "newbasePoints {} ", basePoints);//测试
                                //}
                                //换一个被23885触发的技能23880，也无效
                                //if (_spellInfo->SpellIconID == 38 && _spellInfo->Id == 23880)
                                //{
                                //    LOG_ERROR("xx", "ne2wbasePoints {} ", basePoints);//测试
                                //    if (basePoints == 29)
                                //        if (zsstone > 0)
                                //            basePoints = basePoints + basePoints * zsstone * 0.1;
                                //    LOG_ERROR("xx", "newbasePoints {} ", basePoints);//测试
                                //}

                                //战士顺劈斩加成20569
                                if (_spellInfo->SpellIconID == 277 && (_spellInfo->SpellFamilyFlags[0] & 0x400000))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }

                                //战士英勇打击加成25286
                                if (_spellInfo->SpellIconID == 856 && (_spellInfo->SpellFamilyFlags[0] & 0x40))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //战士猛击加成11605
                                if (_spellInfo->SpellIconID == 559 && (_spellInfo->SpellFamilyFlags[0] & 0x200000))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //战士战斗怒吼加成25289
                                if (_spellInfo->SpellIconID == 456 && (_spellInfo->SpellFamilyFlags[0] & 0x10000))
                                {
                                    if (zsstone > 0)
                                        if (basePoints > 0)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                            }
                        }
                        break;
                    case CLASS_ROGUE:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //盗贼背刺加成11281
                                if (_spellInfo->SpellIconID == 243 && (_spellInfo->SpellFamilyFlags[0] & 0x800004))
                                {
                                    if (zsstone > 0)
                                        if (basePoints != 149)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //盗贼脚踢加成1769
                                if (_spellInfo->SpellIconID == 246 && (_spellInfo->SpellFamilyFlags[0] & 0x10))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //盗贼凿击加成11286
                                if (_spellInfo->SpellIconID == 245 && (_spellInfo->SpellFamilyFlags[0] & 0x8))
                                {
                                    if (basePoints > 0)
                                        basePoints = basePoints + meleeAPpower * 0.1;
                                }
                                //盗贼伏击加成11269
                                if (_spellInfo->SpellIconID == 856 && (_spellInfo->SpellFamilyFlags[0] & 0x800200))
                                {
                                    if (zsstone > 0)
                                        if (basePoints != 274)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //盗贼速效毒药11337
                                //if (_spellInfo->Id == 11337)
                                if (_spellInfo->SpellIconID == 247 && (_spellInfo->SpellFamilyFlags[0] & 0x2000))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                            }
                        }
                        break;
                    case CLASS_DRUID:
                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID && _spellInfo->Id)
                            {
                                //德鲁伊巨熊形态的攻强、生命、护甲加成9635
                                if (_spellInfo->SpellIconID == 107 && _spellInfo->Id == 9635)
                                {
                                    if (zsstone > 0)
                                        if (basePoints == 119)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //德鲁伊猎豹形态的攻强加成3025
                                if (_spellInfo->SpellIconID == 493 && _spellInfo->Id == 3025)
                                {
                                    if (zsstone > 0)
                                        if (basePoints == 39)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //德鲁伊猛虎之怒的伤害加成9846
                                if (_spellInfo->SpellIconID == 1181 && (_spellInfo->Id == 9846 || _spellInfo->Id == 50212 || _spellInfo->Id == 50213))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.15;
                                }
                                //德鲁伊撕碎的伤害加成9830
                                //if (_spellInfo->SpellIconID == 147 && _spellInfo->Id == 9830)
                                if (_spellInfo->SpellIconID == 147 && (_spellInfo->SpellFamilyFlags[0] & 0x8000))
                                {
                                    if (zsstone > 0)
                                        if (basePoints != 224)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                /*
                                //新版本已加强过了
                                //德鲁伊突袭的伤害加成
                                if (_spellInfo->SpellIconID == 495 && _spellInfo->Id == 9826)
                                {
                                    if (zsstone > 0)
                                         basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                */
                                //德鲁伊毁灭的伤害加成9867
                                //if (_spellInfo->SpellIconID == 1531 && _spellInfo->Id == 9867)
                                if (_spellInfo->SpellIconID == 1531 && (_spellInfo->SpellFamilyFlags[0] & 0x10000))
                                {
                                    if (zsstone > 0)
                                        if (basePoints != 384)
                                            basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //德鲁伊横扫伤害加成9908
                                if (_spellInfo->SpellIconID == 1562 && (_spellInfo->Id == 9908 || _spellInfo->Id == 26997 || _spellInfo->Id == 48561 || _spellInfo->Id == 48562))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //德鲁伊荆棘术9910
                                //if (_spellInfo->Id == 9910)
                                if (_spellInfo->SpellIconID == 53 && (_spellInfo->SpellFamilyFlags[0] & 0x100))
                                {
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;
                                }
                                //德鲁伊重殴（锤击）的伤害加成
                                if (_spellInfo->SpellIconID == 261 && (_spellInfo->SpellFamilyFlags[0] & 0x800))
                                {
                                    //LOG_ERROR("xx", "basePointsb {}  ", basePoints);//测试
                                    if (zsstone > 0)
                                        basePoints = basePoints + basePoints * zsstone * 0.1;

                                    //LOG_ERROR("xx", "basePointsa {}  ", basePoints);//测试
                                }

                            }
                        }
                        break;
                    case CLASS_HUNTER:
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "SpellID %u", _spellInfo->Id);//测试
                        //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "SpellIconID %u", _spellInfo->SpellIconID);//测试

                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID)
                            {
                                //uint32 spellid = _spellInfo->Id;
                                //auto spellCheck = [spellid](SpellEntry const & spellEntry) -> bool { return (spellEntry.IsFitToFamily<SPELLFAMILY_HUNTER, CF_HUNTER_ARCANE_SHOT, CF_HUNTER_MULTI_SHOT, CF_HUNTER_VOLLEY, CF_HUNTER_AIMED_SHOT>() ); };
                                //if ((bool&)spellCheck)
                                //瞄准射击0x20000
                                if (_spellInfo->SpellIconID == 1629 && (_spellInfo->SpellFamilyFlags[0] & 0x20000))
                                {
                                    //参考战士的嗜血，45%的攻强作为伤害加成
                                    //335的瞄准射击有个debuff，降低50%的治疗，这个不能再加成，否则就bug了
                                    if (basePoints != -51)
                                        basePoints = basePoints + APpower * 0.12;
                                }
                                //奥术射击0x800
                                else if (_spellInfo->SpellIconID == 218 && (_spellInfo->SpellFamilyFlags[0] & 0x800))
                                {
                                    basePoints = basePoints + APpower * 0.026;
                                }
                                //多重射击0x1000
                                else if (_spellInfo->SpellIconID == 85 && (_spellInfo->SpellFamilyFlags[0] & 0x1000))
                                {
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                                    //参考战士的嗜血，45%的攻强作为伤害加成
                                    basePoints = basePoints + APpower * 0.068;
                                }
                                //乱射0x2000
                                else if (_spellInfo->SpellIconID == 126 && (_spellInfo->SpellFamilyFlags[0] & 0x2000))
                                {
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                                    //参考战士的嗜血，45%的攻强作为伤害加成，先测试了0.018，适当加到0.02
                                    basePoints = basePoints + APpower * 0.023;
                                }
                                //猫鼬撕咬0x2
                                else if (_spellInfo->SpellIconID == 257 && (_spellInfo->SpellFamilyFlags[0] & 0x2))
                                {
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                                    //参考战士的嗜血，45%的攻强作为伤害加成
                                    basePoints = basePoints + APpower * 0.26;
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "SpellID %u", _spellInfo->Id);//测试
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "SpellIconID %u", _spellInfo->SpellIconID);//测试
                                }
                                //猛禽一击0x2
                                else if (_spellInfo->SpellIconID == 26 && (_spellInfo->SpellFamilyFlags[0] & 0x2))
                                {
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                                    //参考战士的嗜血，45%的攻强作为伤害加成
                                    basePoints = basePoints + APpower * 0.1;
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "APpower %f", APpower);//测试
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "SpellID %u", _spellInfo->Id);//测试
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "SpellIconID %u", _spellInfo->SpellIconID);//测试
                                }
                                //治疗宠物 SpellFamilyFlags就是表里的SpellClassMask_1
                                else if (_spellInfo->SpellIconID == 267 && (_spellInfo->SpellFamilyFlags[0] & 0x800000))
                                {
                                    basePoints = basePoints + APpower * 0.05;
                                }

                            }
                        }

                        break;
                    case CLASS_DEATH_KNIGHT://DK死亡骑士加强硬核

                        if (_spellInfo)
                        {
                            if (_spellInfo->SpellIconID)
                            {
                                //冰冷触摸 45477
                                if (_spellInfo->SpellIconID == 2721 && (_spellInfo->SpellFamilyFlags[0] & 0x2))
                                {
                                    basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //冰霜打击 
                                else if (_spellInfo->SpellIconID == 2740)
                                {
                                    switch (_spellInfo->Id)
                                    {
                                    case 49143:
                                    case 51416:
                                    case 51417:
                                    case 51418:
                                    case 51419:
                                    case 55268:

                                        if (basePoints != 54)
                                            basePoints = basePoints + meleeAPpower * 0.06;
                                        break;
                                    }
                                }
                                //凛风冲击 
                                else if (_spellInfo->SpellIconID == 2131)
                                {
                                    switch (_spellInfo->Id)
                                    {
                                    case 49184:
                                    case 51409:
                                    case 51410:
                                    case 51411:
                                        if (basePoints != 149)
                                            basePoints = basePoints + meleeAPpower * 0.06;
                                        break;
                                    }
                                }

                                //暗影打击 49917
                                if (_spellInfo->SpellIconID == 2719 && (_spellInfo->SpellFamilyFlags[0] & 0x1))
                                {
                                    if (basePoints != 49)
                                        basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //凋零缠绕 47541
                                if (_spellInfo->SpellIconID == 88 && (_spellInfo->SpellFamilyFlags[0] & 0x2000))
                                {
                                    basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //枯萎凋零 43265
                                if (_spellInfo->SpellIconID == 118 && (_spellInfo->SpellFamilyFlags[0] & 0x20))
                                {
                                    basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //灵界打击 49998
                                if (_spellInfo->SpellIconID == 2751 && (_spellInfo->SpellFamilyFlags[0] & 0x10))
                                {
                                    if (basePoints != 74)
                                        basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //血液沸腾 48721
                                if (_spellInfo->SpellIconID == 2725 && (_spellInfo->SpellFamilyFlags[0] & 0x40000))
                                {
                                    basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //鲜血打击 49926
                                if (_spellInfo->SpellIconID == 2624 && (_spellInfo->SpellFamilyFlags[0] & 0x400000))
                                {
                                    if (basePoints != 39)
                                        basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //心脏打击 55050
                                if (_spellInfo->SpellIconID == 3145 && (_spellInfo->SpellFamilyFlags[0] & 0x1000000))
                                {
                                    if (basePoints != 49)
                                        basePoints = basePoints + meleeAPpower * 0.06;
                                }
                                //天灾打击 
                                else if (_spellInfo->SpellIconID == 3143)
                                {
                                    switch (_spellInfo->Id)
                                    {
                                    case 55090:
                                    case 55265:
                                    case 55270:
                                    case 55271:
                                        if (basePoints != 69 && basePoints != 11)
                                            basePoints = basePoints + meleeAPpower * 0.06;
                                        break;
                                    }
                                }
                                //邪爆 
                                else if (_spellInfo->SpellIconID == 1737)
                                {
                                    switch (_spellInfo->Id)
                                    {
                                    case 51325:
                                    case 51326:
                                    case 51327:
                                    case 51328:
                                        if (basePoints != 50443)
                                            basePoints = basePoints + meleeAPpower * 0.06;
                                        break;
                                    }
                                }

                            }
                        }
                        break;
                    default:
                        break;
                    }
                }


            }

        }



    }

    // base amount modification based on spell lvl vs caster lvl
    // xinef: added basePointsPerLevel check
    if (caster && basePointsPerLevel != 0.0f)
    {
        int32 level = int32(caster->GetLevel());
        if (level > int32(_spellInfo->MaxLevel) && _spellInfo->MaxLevel > 0)
            level = int32(_spellInfo->MaxLevel);
        else if (level < int32(_spellInfo->BaseLevel))
            level = int32(_spellInfo->BaseLevel);

        // xinef: if base level is greater than spell level, reduce by base level (eg. pilgrims foods)
        level -= int32(std::max(_spellInfo->BaseLevel, _spellInfo->SpellLevel));
        basePoints += int32(level * basePointsPerLevel);
    }

    // roll in a range <1;EffectDieSides> as of patch 3.3.3
    switch (randomPoints)
    {
        case 0:
            break;
        case 1:
            basePoints += 1;
            break;                     // range 1..1
        default:
            // range can have positive (1..rand) and negative (rand..1) values, so order its for irand
            int32 randvalue = (randomPoints >= 1)
                              ? irand(1, randomPoints)
                              : irand(randomPoints, 1);

            basePoints += randvalue;
            break;
    }

    float value = float(basePoints);

    // random damage
    if (caster)
    {
        //npcbot: Life Burst heal tempfix 2013
        float pointsPerComboPoint = PointsPerComboPoint;
        if (_spellInfo->Id == 57143 && _effIndex == EFFECT_1)
        {
            basePoints = 2500;
            value = float(basePoints);
            pointsPerComboPoint = 2500.f;
        }
        //npcbot: bonus amount from combo points and specific mods
        if (caster->IsNPCBot())
        {
            if (uint8 comboPoints = caster->ToCreature()->GetCreatureComboPoints())
                value += pointsPerComboPoint * comboPoints;
        }
        //npcbot: bonus amount from combo points (vehicle)
        else if (caster->IsVehicle() && caster->GetTypeId() == TYPEID_UNIT && caster->GetCharmerGUID().IsCreature() &&
            PointsPerComboPoint)
        {
            Unit const* bot = caster->GetCharmer();
            if (bot && bot->ToCreature()->IsNPCBot())
                if (uint8 comboPoints = bot->ToCreature()->GetCreatureComboPoints())
                    value += pointsPerComboPoint * comboPoints;
        }
        else
        //end npcbot
        // bonus amount from combo points
        if (uint8 comboPoints = caster->GetComboPoints())
        {
            value += PointsPerComboPoint * comboPoints;
        }

        value = caster->ApplyEffectModifiers(_spellInfo, _effIndex, value);

        // amount multiplication based on caster's level
        if (!caster->IsControlledByPlayer() &&
                _spellInfo->SpellLevel && _spellInfo->SpellLevel != caster->GetLevel() &&
                !basePointsPerLevel && _spellInfo->HasAttribute(SPELL_ATTR0_SCALES_WITH_CREATURE_LEVEL))
        {
            bool canEffectScale = false;
            switch (Effect)
            {
                case SPELL_EFFECT_SCHOOL_DAMAGE:
                case SPELL_EFFECT_DUMMY:
                case SPELL_EFFECT_POWER_DRAIN:
                case SPELL_EFFECT_HEALTH_LEECH:
                case SPELL_EFFECT_HEAL:
                case SPELL_EFFECT_WEAPON_DAMAGE:
                case SPELL_EFFECT_POWER_BURN:
                case SPELL_EFFECT_SCRIPT_EFFECT:
                case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                case SPELL_EFFECT_FORCE_CAST_WITH_VALUE:
                case SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE:
                case SPELL_EFFECT_TRIGGER_MISSILE_SPELL_WITH_VALUE:
                    canEffectScale = true;
                    break;
                default:
                    break;
            }

            switch (ApplyAuraName)
            {
                case SPELL_AURA_PERIODIC_DAMAGE:
                case SPELL_AURA_DUMMY:
                case SPELL_AURA_PERIODIC_HEAL:
                case SPELL_AURA_DAMAGE_SHIELD:
                case SPELL_AURA_PROC_TRIGGER_DAMAGE:
                case SPELL_AURA_PERIODIC_LEECH:
                case SPELL_AURA_PERIODIC_MANA_LEECH:
                case SPELL_AURA_SCHOOL_ABSORB:
                case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
                    canEffectScale = true;
                    break;
                default:
                    break;
            }

            if ((sSpellMgr->GetSpellInfo(_spellInfo->Effects[_effIndex].TriggerSpell) && sSpellMgr->GetSpellInfo(_spellInfo->Effects[_effIndex].TriggerSpell)->HasAttribute(SPELL_ATTR0_SCALES_WITH_CREATURE_LEVEL)) && _spellInfo->HasAttribute(SPELL_ATTR0_SCALES_WITH_CREATURE_LEVEL))
                canEffectScale = false;

            if (canEffectScale)
            {
                CreatureTemplate const* cInfo = caster->ToCreature()->GetCreatureTemplate();

                CreatureBaseStats const* pCBS = sObjectMgr->GetCreatureBaseStats(caster->GetLevel(), caster->getClass());
                float CBSPowerCreature = pCBS->BaseDamage[cInfo->expansion];
                CreatureBaseStats const* spellCBS = sObjectMgr->GetCreatureBaseStats(_spellInfo->SpellLevel, caster->getClass());
                float CBSPowerSpell = spellCBS->BaseDamage[cInfo->expansion];
                value *= CBSPowerCreature / CBSPowerSpell;
            }
        }
    }

    return int32(value);
}

int32 SpellEffectInfo::CalcBaseValue(int32 value) const
{
    if (DieSides == 0)
        return value;
    else
        return value - 1;
}

float SpellEffectInfo::CalcValueMultiplier(Unit* caster, Spell* spell) const
{
    float multiplier = ValueMultiplier;
    if (Player* modOwner = (caster ? caster->GetSpellModOwner() : nullptr))
        modOwner->ApplySpellMod(_spellInfo->Id, SPELLMOD_VALUE_MULTIPLIER, multiplier, spell);

    //npcbot - apply bot spell effect value mult mods
    if (caster && caster->IsNPCBot())
        BotMgr::ApplyBotEffectValueMultiplierMods(caster->ToCreature(), _spellInfo, SpellEffIndex(_effIndex), multiplier);
    //end npcbot

    return multiplier;
}

float SpellEffectInfo::CalcDamageMultiplier(Unit* caster, Spell* spell) const
{
    float multiplier = DamageMultiplier;
    if (Player* modOwner = (caster ? caster->GetSpellModOwner() : nullptr))
        modOwner->ApplySpellMod(_spellInfo->Id, SPELLMOD_DAMAGE_MULTIPLIER, multiplier, spell);
    return multiplier;
}

bool SpellEffectInfo::HasRadius() const
{
    return RadiusEntry != nullptr;
}

float SpellEffectInfo::CalcRadius(Unit* caster, Spell* spell) const
{
    if (!HasRadius())
        return 0.0f;

    float radius = RadiusEntry->RadiusMin;
    if (caster)
    {
        radius += RadiusEntry->RadiusPerLevel * caster->GetLevel();
        radius = std::min(radius, RadiusEntry->RadiusMax);
        if (Player* modOwner = caster->GetSpellModOwner())
            modOwner->ApplySpellMod(_spellInfo->Id, SPELLMOD_RADIUS, radius, spell);

        //npcbot - apply bot spell radius mods
        if (caster->IsNPCBotOrPet())
            caster->ToCreature()->ApplyCreatureSpellRadiusMods(_spellInfo, radius);
        //end npcbot
    }

    return radius;
}

uint32 SpellEffectInfo::GetProvidedTargetMask() const
{
    return GetTargetFlagMask(TargetA.GetObjectType()) | GetTargetFlagMask(TargetB.GetObjectType());
}

uint32 SpellEffectInfo::GetMissingTargetMask(bool srcSet /*= false*/, bool dstSet /*= false*/, uint32 mask /*=0*/) const
{
    uint32 effImplicitTargetMask = GetTargetFlagMask(GetUsedTargetObjectType());
    uint32 providedTargetMask = GetTargetFlagMask(TargetA.GetObjectType()) | GetTargetFlagMask(TargetB.GetObjectType()) | mask;

    // remove all flags covered by effect target mask
    if (providedTargetMask & TARGET_FLAG_UNIT_MASK)
        effImplicitTargetMask &= ~(TARGET_FLAG_UNIT_MASK);
    if (providedTargetMask & TARGET_FLAG_CORPSE_MASK)
        effImplicitTargetMask &= ~(TARGET_FLAG_UNIT_MASK | TARGET_FLAG_CORPSE_MASK);
    if (providedTargetMask & TARGET_FLAG_GAMEOBJECT_ITEM)
        effImplicitTargetMask &= ~(TARGET_FLAG_GAMEOBJECT_ITEM | TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_ITEM);
    if (providedTargetMask & TARGET_FLAG_GAMEOBJECT)
        effImplicitTargetMask &= ~(TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_GAMEOBJECT_ITEM);
    if (providedTargetMask & TARGET_FLAG_ITEM)
        effImplicitTargetMask &= ~(TARGET_FLAG_ITEM | TARGET_FLAG_GAMEOBJECT_ITEM);
    if (dstSet || providedTargetMask & TARGET_FLAG_DEST_LOCATION)
        effImplicitTargetMask &= ~(TARGET_FLAG_DEST_LOCATION);
    if (srcSet || providedTargetMask & TARGET_FLAG_SOURCE_LOCATION)
        effImplicitTargetMask &= ~(TARGET_FLAG_SOURCE_LOCATION);

    return effImplicitTargetMask;
}

SpellEffectImplicitTargetTypes SpellEffectInfo::GetImplicitTargetType() const
{
    return _data[Effect].ImplicitTargetType;
}

SpellTargetObjectTypes SpellEffectInfo::GetUsedTargetObjectType() const
{
    return _data[Effect].UsedTargetObjectType;
}

std::array<SpellEffectInfo::StaticData, TOTAL_SPELL_EFFECTS> SpellEffectInfo::_data =
{ {
    // implicit target type           used target object type
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 0
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 1 SPELL_EFFECT_INSTAKILL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 2 SPELL_EFFECT_SCHOOL_DAMAGE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 3 SPELL_EFFECT_DUMMY
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 4 SPELL_EFFECT_PORTAL_TELEPORT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 5 SPELL_EFFECT_TELEPORT_UNITS
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 6 SPELL_EFFECT_APPLY_AURA
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 7 SPELL_EFFECT_ENVIRONMENTAL_DAMAGE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 8 SPELL_EFFECT_POWER_DRAIN
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 9 SPELL_EFFECT_HEALTH_LEECH
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 10 SPELL_EFFECT_HEAL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 11 SPELL_EFFECT_BIND
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 12 SPELL_EFFECT_PORTAL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 13 SPELL_EFFECT_RITUAL_BASE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 14 SPELL_EFFECT_RITUAL_SPECIALIZE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 15 SPELL_EFFECT_RITUAL_ACTIVATE_PORTAL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 16 SPELL_EFFECT_QUEST_COMPLETE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 17 SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_CORPSE_ALLY}, // 18 SPELL_EFFECT_RESURRECT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 19 SPELL_EFFECT_ADD_EXTRA_ATTACKS
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 20 SPELL_EFFECT_DODGE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 21 SPELL_EFFECT_EVADE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 22 SPELL_EFFECT_PARRY
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 23 SPELL_EFFECT_BLOCK
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 24 SPELL_EFFECT_CREATE_ITEM
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 25 SPELL_EFFECT_WEAPON
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 26 SPELL_EFFECT_DEFENSE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 27 SPELL_EFFECT_PERSISTENT_AREA_AURA
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 28 SPELL_EFFECT_SUMMON
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 29 SPELL_EFFECT_LEAP
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 30 SPELL_EFFECT_ENERGIZE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 31 SPELL_EFFECT_WEAPON_PERCENT_DAMAGE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 32 SPELL_EFFECT_TRIGGER_MISSILE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_GOBJ_ITEM}, // 33 SPELL_EFFECT_OPEN_LOCK
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 34 SPELL_EFFECT_SUMMON_CHANGE_ITEM
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 35 SPELL_EFFECT_APPLY_AREA_AURA_PARTY
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 36 SPELL_EFFECT_LEARN_SPELL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 37 SPELL_EFFECT_SPELL_DEFENSE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 38 SPELL_EFFECT_DISPEL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 39 SPELL_EFFECT_LANGUAGE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 40 SPELL_EFFECT_DUAL_WIELD
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 41 SPELL_EFFECT_JUMP
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_DEST}, // 42 SPELL_EFFECT_JUMP_DEST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 43 SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 44 SPELL_EFFECT_SKILL_STEP
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 45 SPELL_EFFECT_ADD_HONOR
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 46 SPELL_EFFECT_SPAWN
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 47 SPELL_EFFECT_TRADE_SKILL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 48 SPELL_EFFECT_STEALTH
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 49 SPELL_EFFECT_DETECT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 50 SPELL_EFFECT_TRANS_DOOR
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 51 SPELL_EFFECT_FORCE_CRITICAL_HIT
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 52 SPELL_EFFECT_GUARANTEE_HIT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 53 SPELL_EFFECT_ENCHANT_ITEM
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 54 SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 55 SPELL_EFFECT_TAMECREATURE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 56 SPELL_EFFECT_SUMMON_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 57 SPELL_EFFECT_LEARN_PET_SPELL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 58 SPELL_EFFECT_WEAPON_DAMAGE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 59 SPELL_EFFECT_CREATE_RANDOM_ITEM
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 60 SPELL_EFFECT_PROFICIENCY
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 61 SPELL_EFFECT_SEND_EVENT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 62 SPELL_EFFECT_POWER_BURN
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 63 SPELL_EFFECT_THREAT
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 64 SPELL_EFFECT_TRIGGER_SPELL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 65 SPELL_EFFECT_APPLY_AREA_AURA_RAID
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 66 SPELL_EFFECT_CREATE_MANA_GEM
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 67 SPELL_EFFECT_HEAL_MAX_HEALTH
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 68 SPELL_EFFECT_INTERRUPT_CAST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 69 SPELL_EFFECT_DISTRACT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 70 SPELL_EFFECT_PULL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 71 SPELL_EFFECT_PICKPOCKET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 72 SPELL_EFFECT_ADD_FARSIGHT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 73 SPELL_EFFECT_UNTRAIN_TALENTS
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 74 SPELL_EFFECT_APPLY_GLYPH
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 75 SPELL_EFFECT_HEAL_MECHANICAL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 76 SPELL_EFFECT_SUMMON_OBJECT_WILD
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 77 SPELL_EFFECT_SCRIPT_EFFECT
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 78 SPELL_EFFECT_ATTACK
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 79 SPELL_EFFECT_SANCTUARY
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 80 SPELL_EFFECT_ADD_COMBO_POINTS
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 81 SPELL_EFFECT_CREATE_HOUSE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 82 SPELL_EFFECT_BIND_SIGHT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 83 SPELL_EFFECT_DUEL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 84 SPELL_EFFECT_STUCK
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 85 SPELL_EFFECT_SUMMON_PLAYER
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_GOBJ}, // 86 SPELL_EFFECT_ACTIVATE_OBJECT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_GOBJ}, // 87 SPELL_EFFECT_GAMEOBJECT_DAMAGE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_GOBJ}, // 88 SPELL_EFFECT_GAMEOBJECT_REPAIR
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_GOBJ}, // 89 SPELL_EFFECT_GAMEOBJECT_SET_DESTRUCTION_STATE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 90 SPELL_EFFECT_KILL_CREDIT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 91 SPELL_EFFECT_THREAT_ALL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 92 SPELL_EFFECT_ENCHANT_HELD_ITEM
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 93 SPELL_EFFECT_FORCE_DESELECT
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 94 SPELL_EFFECT_SELF_RESURRECT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 95 SPELL_EFFECT_SKINNING
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 96 SPELL_EFFECT_CHARGE
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 97 SPELL_EFFECT_CAST_BUTTON
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 98 SPELL_EFFECT_KNOCK_BACK
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 99 SPELL_EFFECT_DISENCHANT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 100 SPELL_EFFECT_INEBRIATE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 101 SPELL_EFFECT_FEED_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 102 SPELL_EFFECT_DISMISS_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 103 SPELL_EFFECT_REPUTATION
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 104 SPELL_EFFECT_SUMMON_OBJECT_SLOT1
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 105 SPELL_EFFECT_SUMMON_OBJECT_SLOT2
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 106 SPELL_EFFECT_SUMMON_OBJECT_SLOT3
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 107 SPELL_EFFECT_SUMMON_OBJECT_SLOT4
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 108 SPELL_EFFECT_DISPEL_MECHANIC
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 109 SPELL_EFFECT_RESURRECT_PET
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 110 SPELL_EFFECT_DESTROY_ALL_TOTEMS
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 111 SPELL_EFFECT_DURABILITY_DAMAGE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 112 SPELL_EFFECT_112
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_CORPSE_ALLY}, // 113 SPELL_EFFECT_RESURRECT_NEW
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 114 SPELL_EFFECT_ATTACK_ME
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 115 SPELL_EFFECT_DURABILITY_DAMAGE_PCT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_CORPSE_ENEMY}, // 116 SPELL_EFFECT_SKIN_PLAYER_CORPSE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 117 SPELL_EFFECT_SPIRIT_HEAL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 118 SPELL_EFFECT_SKILL
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 119 SPELL_EFFECT_APPLY_AREA_AURA_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 120 SPELL_EFFECT_TELEPORT_GRAVEYARD
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 121 SPELL_EFFECT_NORMALIZED_WEAPON_DMG
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 122 SPELL_EFFECT_122
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 123 SPELL_EFFECT_SEND_TAXI
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 124 SPELL_EFFECT_PULL_TOWARDS
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 125 SPELL_EFFECT_MODIFY_THREAT_PERCENT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 126 SPELL_EFFECT_STEAL_BENEFICIAL_BUFF
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 127 SPELL_EFFECT_PROSPECTING
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 128 SPELL_EFFECT_APPLY_AREA_AURA_FRIEND
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 129 SPELL_EFFECT_APPLY_AREA_AURA_ENEMY
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 130 SPELL_EFFECT_REDIRECT_THREAT
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 131 SPELL_EFFECT_PLAY_SOUND
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 132 SPELL_EFFECT_PLAY_MUSIC
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 133 SPELL_EFFECT_UNLEARN_SPECIALIZATION
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 134 SPELL_EFFECT_KILL_CREDIT2
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 135 SPELL_EFFECT_CALL_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 136 SPELL_EFFECT_HEAL_PCT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 137 SPELL_EFFECT_ENERGIZE_PCT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 138 SPELL_EFFECT_LEAP_BACK
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 139 SPELL_EFFECT_CLEAR_QUEST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 140 SPELL_EFFECT_FORCE_CAST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 141 SPELL_EFFECT_FORCE_CAST_WITH_VALUE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 142 SPELL_EFFECT_TRIGGER_SPELL_WITH_VALUE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 143 SPELL_EFFECT_APPLY_AREA_AURA_OWNER
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 144 SPELL_EFFECT_KNOCK_BACK_DEST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT_AND_DEST}, // 145 SPELL_EFFECT_PULL_TOWARDS_DEST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 146 SPELL_EFFECT_ACTIVATE_RUNE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 147 SPELL_EFFECT_QUEST_FAIL
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 148 SPELL_EFFECT_TRIGGER_MISSILE_SPELL_WITH_VALUE
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_DEST}, // 149 SPELL_EFFECT_CHARGE_DEST
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 150 SPELL_EFFECT_QUEST_START
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 151 SPELL_EFFECT_TRIGGER_SPELL_2
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_NONE}, // 152 SPELL_EFFECT_SUMMON_RAF_FRIEND
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 153 SPELL_EFFECT_CREATE_TAMED_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 154 SPELL_EFFECT_DISCOVER_TAXI
    {EFFECT_IMPLICIT_TARGET_NONE,     TARGET_OBJECT_TYPE_UNIT}, // 155 SPELL_EFFECT_TITAN_GRIP
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 156 SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 157 SPELL_EFFECT_CREATE_ITEM_2
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_ITEM}, // 158 SPELL_EFFECT_MILLING
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 159 SPELL_EFFECT_ALLOW_RENAME_PET
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 160 SPELL_EFFECT_FORCE_CAST_2
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 161 SPELL_EFFECT_TALENT_SPEC_COUNT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 162 SPELL_EFFECT_TALENT_SPEC_SELECT
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 163 SPELL_EFFECT_163
    {EFFECT_IMPLICIT_TARGET_EXPLICIT, TARGET_OBJECT_TYPE_UNIT}, // 164 SPELL_EFFECT_REMOVE_AURA
} };

SpellInfo::SpellInfo(SpellEntry const* spellEntry)
{
    Id = spellEntry->Id;
    CategoryEntry = spellEntry->Category ? sSpellCategoryStore.LookupEntry(spellEntry->Category) : nullptr;
    Dispel = spellEntry->Dispel;
    Mechanic = spellEntry->Mechanic;
    Attributes = spellEntry->Attributes;
    AttributesEx = spellEntry->AttributesEx;
    AttributesEx2 = spellEntry->AttributesEx2;
    AttributesEx3 = spellEntry->AttributesEx3;
    AttributesEx4 = spellEntry->AttributesEx4;
    AttributesEx5 = spellEntry->AttributesEx5;
    AttributesEx6 = spellEntry->AttributesEx6;
    AttributesEx7 = spellEntry->AttributesEx7;
    AttributesCu = 0;
    Stances = spellEntry->Stances;
    StancesNot = spellEntry->StancesNot;
    Targets = spellEntry->Targets;
    TargetCreatureType = spellEntry->TargetCreatureType;
    RequiresSpellFocus = spellEntry->RequiresSpellFocus;
    FacingCasterFlags = spellEntry->FacingCasterFlags;
    CasterAuraState = spellEntry->CasterAuraState;
    TargetAuraState = spellEntry->TargetAuraState;
    CasterAuraStateNot = spellEntry->CasterAuraStateNot;
    TargetAuraStateNot = spellEntry->TargetAuraStateNot;
    CasterAuraSpell = spellEntry->CasterAuraSpell;
    TargetAuraSpell = spellEntry->TargetAuraSpell;
    ExcludeCasterAuraSpell = spellEntry->ExcludeCasterAuraSpell;
    ExcludeTargetAuraSpell = spellEntry->ExcludeTargetAuraSpell;
    CastTimeEntry = spellEntry->CastingTimeIndex ? sSpellCastTimesStore.LookupEntry(spellEntry->CastingTimeIndex) : nullptr;
    RecoveryTime = spellEntry->RecoveryTime;
    CategoryRecoveryTime = spellEntry->CategoryRecoveryTime;
    StartRecoveryCategory = spellEntry->StartRecoveryCategory;
    StartRecoveryTime = spellEntry->StartRecoveryTime;
    InterruptFlags = spellEntry->InterruptFlags;
    AuraInterruptFlags = spellEntry->AuraInterruptFlags;
    ChannelInterruptFlags = spellEntry->ChannelInterruptFlags;
    ProcFlags = spellEntry->ProcFlags;
    ProcChance = spellEntry->ProcChance;
    ProcCharges = spellEntry->ProcCharges;
    MaxLevel = spellEntry->MaxLevel;
    BaseLevel = spellEntry->BaseLevel;
    SpellLevel = spellEntry->SpellLevel;
    DurationEntry = spellEntry->DurationIndex ? sSpellDurationStore.LookupEntry(spellEntry->DurationIndex) : nullptr;
    PowerType = spellEntry->PowerType;
    ManaCost = spellEntry->ManaCost;
    ManaCostPerlevel = spellEntry->ManaCostPerlevel;
    ManaPerSecond = spellEntry->ManaPerSecond;
    ManaPerSecondPerLevel = spellEntry->ManaPerSecondPerLevel;
    ManaCostPercentage = spellEntry->ManaCostPercentage;
    RuneCostID = spellEntry->RuneCostID;
    RangeEntry = spellEntry->RangeIndex ? sSpellRangeStore.LookupEntry(spellEntry->RangeIndex) : nullptr;
    Speed = spellEntry->Speed;
    StackAmount = spellEntry->StackAmount;
    Totem = spellEntry->Totem;
    Reagent = spellEntry->Reagent;
    ReagentCount = spellEntry->ReagentCount;
    EquippedItemClass = spellEntry->EquippedItemClass;
    EquippedItemSubClassMask = spellEntry->EquippedItemSubClassMask;
    EquippedItemInventoryTypeMask = spellEntry->EquippedItemInventoryTypeMask;
    TotemCategory = spellEntry->TotemCategory;
    SpellVisual = spellEntry->SpellVisual;
    SpellIconID = spellEntry->SpellIconID;
    ActiveIconID = spellEntry->ActiveIconID;
    SpellPriority = spellEntry->SpellPriority;
    SpellName = spellEntry->SpellName;
    Rank = spellEntry->Rank;
    MaxTargetLevel = spellEntry->MaxTargetLevel;
    MaxAffectedTargets = spellEntry->MaxAffectedTargets;
    SpellFamilyName = spellEntry->SpellFamilyName;
    SpellFamilyFlags = spellEntry->SpellFamilyFlags;
    DmgClass = spellEntry->DmgClass;
    PreventionType = spellEntry->PreventionType;
    AreaGroupId = spellEntry->AreaGroupId;
    SchoolMask = spellEntry->SchoolMask;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        Effects[i] = SpellEffectInfo(spellEntry, this, i);

    ChainEntry = nullptr;
    ExplicitTargetMask = 0;

    // Mine
    _isStackableWithRanks = false;
    _isSpellValid = true;
    _isCritCapable = false;
    _requireCooldownInfo = false;
}

SpellInfo::~SpellInfo()
{
    _UnloadImplicitTargetConditionLists();
}

SpellInfo const* SpellInfo::TryGetSpellInfoOverride(WorldObject const* caster) const
{
    SpellInfo const* spellInfoOverride = (caster && caster->IsNPCBotOrPet()) ? GetBotSpellInfoOverride(Id) : nullptr;
    return spellInfoOverride ? spellInfoOverride : this;
}

uint32 SpellInfo::GetCategory() const
{
    return CategoryEntry ? CategoryEntry->Id : 0;
}

bool SpellInfo::HasEffect(SpellEffects effect) const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsEffect(effect))
            return true;
    return false;
}

bool SpellInfo::HasEffectMechanic(Mechanics mechanic) const
{
    for (auto const& effect : Effects)
        if (effect.Mechanic == mechanic)
            return true;

    return false;
}

bool SpellInfo::HasAura(AuraType aura) const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsAura(aura))
            return true;
    return false;
}

bool SpellInfo::HasAnyAura() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsAura())
            return true;
    return false;
}

bool SpellInfo::HasAreaAuraEffect() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsAreaAuraEffect())
            return true;
    return false;
}

bool SpellInfo::IsExplicitDiscovery() const
{
    return ((Effects[0].Effect == SPELL_EFFECT_CREATE_RANDOM_ITEM
             || Effects[0].Effect == SPELL_EFFECT_CREATE_ITEM_2)
            && Effects[1].Effect == SPELL_EFFECT_SCRIPT_EFFECT)
           || Id == 64323;
}

bool SpellInfo::IsLootCrafting() const
{
    return (Effects[0].Effect == SPELL_EFFECT_CREATE_RANDOM_ITEM ||
            // different random cards from Inscription (121==Virtuoso Inking Set category) r without explicit item
            (Effects[0].Effect == SPELL_EFFECT_CREATE_ITEM_2 &&
             ((TotemCategory[0] != 0 || (Totem[0] != 0 && SpellIconID == 1)) || Effects[0].ItemType == 0)));
}

bool SpellInfo::IsQuestTame() const
{
    return Effects[0].Effect == SPELL_EFFECT_THREAT && Effects[1].Effect == SPELL_EFFECT_APPLY_AURA && Effects[1].ApplyAuraName == SPELL_AURA_DUMMY;
}

bool SpellInfo::IsProfessionOrRiding() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].Effect == SPELL_EFFECT_SKILL)
        {
            uint32 skill = Effects[i].MiscValue;

            if (IsProfessionOrRidingSkill(skill))
                return true;
        }
    }
    return false;
}

bool SpellInfo::IsProfession() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].Effect == SPELL_EFFECT_SKILL)
        {
            uint32 skill = Effects[i].MiscValue;

            if (IsProfessionSkill(skill))
                return true;
        }
    }
    return false;
}

bool SpellInfo::IsPrimaryProfession() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].Effect == SPELL_EFFECT_SKILL)
        {
            uint32 skill = Effects[i].MiscValue;

            if (IsPrimaryProfessionSkill(skill))
                return true;
        }
    }
    return false;
}

bool SpellInfo::IsPrimaryProfessionFirstRank() const
{
    return IsPrimaryProfession() && GetRank() == 1;
}

bool SpellInfo::IsAbilityLearnedWithProfession() const
{
    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(Id);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* pAbility = _spell_idx->second;
        if (!pAbility || pAbility->AcquireMethod != SKILL_LINE_ABILITY_LEARNED_ON_SKILL_VALUE)
            continue;

        if (pAbility->MinSkillLineRank > 0)
            return true;
    }

    return false;
}

bool SpellInfo::IsAbilityOfSkillType(uint32 skillType) const
{
    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(Id);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
        if (_spell_idx->second->SkillLine == uint32(skillType))
            return true;

    return false;
}

bool SpellInfo::IsAffectingArea() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsEffect() && (Effects[i].IsTargetingArea() || Effects[i].IsEffect(SPELL_EFFECT_PERSISTENT_AREA_AURA) || Effects[i].IsAreaAuraEffect()))
            return true;
    return false;
}

// checks if spell targets are selected from area, doesn't include spell effects in check (like area wide auras for example)
bool SpellInfo::IsTargetingArea() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsEffect() && Effects[i].IsTargetingArea())
            return true;
    return false;
}

bool SpellInfo::NeedsExplicitUnitTarget() const
{
    return GetExplicitTargetMask() & TARGET_FLAG_UNIT_MASK;
}

bool SpellInfo::NeedsToBeTriggeredByCaster(SpellInfo const* triggeringSpell, uint8 effIndex) const
{
    if (NeedsExplicitUnitTarget())
        return true;

    // pussywizard:
    if (effIndex < MAX_SPELL_EFFECTS && (triggeringSpell->Effects[effIndex].TargetA.GetCheckType() == TARGET_CHECK_ENTRY || triggeringSpell->Effects[effIndex].TargetB.GetCheckType() == TARGET_CHECK_ENTRY))
    {
        // xinef:
        if (Id == 60563)
            return true;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (Effects[i].IsEffect() && (Effects[i].TargetA.GetCheckType() == TARGET_CHECK_ENTRY || Effects[i].TargetB.GetCheckType() == TARGET_CHECK_ENTRY))
                return true;
    }

    /*
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].IsEffect())
        {
            if (Effects[i].TargetA.GetSelectionCategory() == TARGET_SELECT_CATEGORY_CHANNEL
                || Effects[i].TargetB.GetSelectionCategory() == TARGET_SELECT_CATEGORY_CHANNEL)
                return true;
        }
    }
    */

    if (triggeringSpell->IsChanneled())
    {
        uint32 mask = 0;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (Effects[i].TargetA.GetTarget() != TARGET_UNIT_CASTER && Effects[i].TargetA.GetTarget() != TARGET_DEST_CASTER)
                mask |= Effects[i].GetProvidedTargetMask();
        }

        if (mask & TARGET_FLAG_UNIT_MASK)
            return true;
    }

    return false;
}

bool SpellInfo::IsChannelCategorySpell() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].TargetA.GetSelectionCategory() == TARGET_SELECT_CATEGORY_CHANNEL || Effects[i].TargetB.GetSelectionCategory() == TARGET_SELECT_CATEGORY_CHANNEL)
            return true;
    return false;
}
bool SpellInfo::IsSelfCast() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].Effect && Effects[i].TargetA.GetTarget() != TARGET_UNIT_CASTER)
            return false;
    return true;
}

bool SpellInfo::IsPassive() const
{
    return HasAttribute(SPELL_ATTR0_PASSIVE);
}

bool SpellInfo::IsAutocastable() const
{
    if (HasAttribute(SPELL_ATTR0_PASSIVE))
        return false;
    if (HasAttribute(SPELL_ATTR1_NO_AUTOCAST_AI))
        return false;
    return true;
}

bool SpellInfo::ComputeIsStackableWithRanks() const
{
    if (IsPassive())
        return false;
    if (PowerType != POWER_MANA && PowerType != POWER_HEALTH)
        return false;
    if (IsProfessionOrRiding())
        return false;

    if (IsAbilityLearnedWithProfession())
        return false;

    // All stance spells. if any better way, change it.
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (SpellFamilyName)
        {
            case SPELLFAMILY_PALADIN:
                // Paladin aura Spell
                if (Effects[i].Effect == SPELL_EFFECT_APPLY_AREA_AURA_RAID)
                    return false;
                break;
            case SPELLFAMILY_DRUID:
                // Druid form Spell
                if (Effects[i].Effect == SPELL_EFFECT_APPLY_AURA &&
                        Effects[i].ApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT)
                    return false;
                break;
        }
    }
    return true;
}

bool SpellInfo::IsStackableWithRanks() const
{
    return _isStackableWithRanks;
}

void SpellInfo::SetStackableWithRanks(bool val)
{
    _isStackableWithRanks = val;
}

bool SpellInfo::ComputeIsCritCapable() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (Effects[i].Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_HEAL:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_HEAL_MECHANICAL:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_HEAL_PCT:
                return true;
        }
    }
    return false;
}

bool SpellInfo::IsCritCapable() const
{
    return _isCritCapable;
}

bool SpellInfo::RequireCooldownInfo() const
{
    return _requireCooldownInfo;
}

void SpellInfo::SetCritCapable(bool val)
{
    _isCritCapable = val;
}

bool SpellInfo::IsSpellValid() const
{
    return _isSpellValid;
}

void SpellInfo::SetSpellValid(bool val)
{
    _isSpellValid = val;
}

bool SpellInfo::IsPassiveStackableWithRanks() const
{
    return IsPassive() && !HasEffect(SPELL_EFFECT_APPLY_AURA);
}

bool SpellInfo::IsMultiSlotAura() const
{
    return IsPassive() || Id == 40075; // No other way to make 40075 have more than 1 copy of aura
}

bool SpellInfo::IsCooldownStartedOnEvent() const
{
    return HasAttribute(SPELL_ATTR0_COOLDOWN_ON_EVENT) || (CategoryEntry && CategoryEntry->Flags & SPELL_CATEGORY_FLAG_COOLDOWN_STARTS_ON_EVENT);
}

bool SpellInfo::IsDeathPersistent() const
{
    return AttributesEx3 & SPELL_ATTR3_ALLOW_AURA_WHILE_DEAD;
}

bool SpellInfo::IsRequiringDeadTarget() const
{
    return AttributesEx3 & SPELL_ATTR3_ONLY_ON_GHOSTS;
}

bool SpellInfo::IsAllowingDeadTarget() const
{
    return AttributesEx2 & SPELL_ATTR2_ALLOW_DEAD_TARGET || Targets & (TARGET_FLAG_CORPSE_ALLY | TARGET_FLAG_CORPSE_ENEMY | TARGET_FLAG_UNIT_DEAD);
}

bool SpellInfo::CanBeUsedInCombat() const
{
    return !(HasAttribute(SPELL_ATTR0_NOT_IN_COMBAT_ONLY_PEACEFUL));
}

bool SpellInfo::IsPositive() const
{
    return !(AttributesCu & SPELL_ATTR0_CU_NEGATIVE) || (AttributesCu & SPELL_ATTR0_CU_POSITIVE);
}

bool SpellInfo::IsPositiveEffect(uint8 effIndex) const
{
    switch (effIndex)
    {
        default:
        case 0:
            return !(AttributesCu & SPELL_ATTR0_CU_NEGATIVE_EFF0) || (AttributesCu & SPELL_ATTR0_CU_POSITIVE_EFF0);
        case 1:
            return !(AttributesCu & SPELL_ATTR0_CU_NEGATIVE_EFF1) || (AttributesCu & SPELL_ATTR0_CU_POSITIVE_EFF1);
        case 2:
            return !(AttributesCu & SPELL_ATTR0_CU_NEGATIVE_EFF2) || (AttributesCu & SPELL_ATTR0_CU_POSITIVE_EFF2);
    }
}

bool SpellInfo::IsChanneled() const
{
    return (AttributesEx & (SPELL_ATTR1_IS_CHANNELED | SPELL_ATTR1_IS_SELF_CHANNELED));
}

bool SpellInfo::IsActionAllowedChannel() const
{
    return IsChanneled() && HasAttribute(SPELL_ATTR5_ALLOW_ACTION_DURING_CHANNEL);
}

bool SpellInfo::NeedsComboPoints() const
{
    return (AttributesEx & (SPELL_ATTR1_FINISHING_MOVE_DAMAGE | SPELL_ATTR1_FINISHING_MOVE_DURATION));
}

bool SpellInfo::IsBreakingStealth() const
{
    return !(AttributesEx & SPELL_ATTR1_ALLOW_WHILE_STEALTHED);
}

bool SpellInfo::IsRangedWeaponSpell() const
{
    return (SpellFamilyName == SPELLFAMILY_HUNTER && !(SpellFamilyFlags[1] & 0x10000000)) // for 53352, cannot find better way
           || (EquippedItemSubClassMask & ITEM_SUBCLASS_MASK_WEAPON_RANGED)
           || (HasAttribute(SPELL_ATTR0_USES_RANGED_SLOT)); // Xinef: added
}

bool SpellInfo::IsAutoRepeatRangedSpell() const
{
    return AttributesEx2 & SPELL_ATTR2_AUTO_REPEAT;
}

bool SpellInfo::IsAffectedBySpellMods() const
{
    return !(AttributesEx3 & SPELL_ATTR3_IGNORE_CASTER_MODIFIERS);
}

bool SpellInfo::IsAffectedBySpellMod(SpellModifier const* mod) const
{
    // xinef: dont check duration mod
    if (mod->op != SPELLMOD_DURATION)
        if (!IsAffectedBySpellMods())
            return false;

    SpellInfo const* affectSpell = sSpellMgr->GetSpellInfo(mod->spellId);

    if (!affectSpell)
    {
        return false;
    }

    if (!sScriptMgr->OnIsAffectedBySpellModCheck(affectSpell, this, mod))
    {
        return true;
    }

    // False if affect_spell == nullptr or spellFamily not equal
    if (affectSpell->SpellFamilyName != SpellFamilyName)
        return false;

    // true
    if (mod->mask & SpellFamilyFlags)
        return true;

    return false;
}

bool SpellInfo::CanPierceImmuneAura(SpellInfo const* aura) const
{
    // aura can't be pierced
    if (!aura || aura->HasAttribute(SPELL_ATTR0_NO_IMMUNITIES))
    {
        return false;
    }

    // these spells pierce all avalible spells (Resurrection Sickness for example)
    if (HasAttribute(SPELL_ATTR0_NO_IMMUNITIES))
        return true;

    // these spells (Cyclone for example) can pierce all...
    if (HasAttribute(SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS) || HasAttribute(SPELL_ATTR2_NO_SCHOOL_IMMUNITIES))
    {
        if (aura->Mechanic != MECHANIC_IMMUNE_SHIELD &&
               aura->Mechanic != MECHANIC_INVULNERABILITY &&
               aura->Mechanic != MECHANIC_BANISH)
        {
            return true;
        }

    }

    return false;
}

bool SpellInfo::CanDispelAura(SpellInfo const* aura) const
{
    // Xinef: Passive auras cannot be dispelled
    if (aura->IsPassive())
        return false;

    // These auras (like Divine Shield) can't be dispelled
    if (aura->HasAttribute(SPELL_ATTR0_NO_IMMUNITIES))
        return false;

    // These spells (like Mass Dispel) can dispell all auras
    if (HasAttribute(SPELL_ATTR0_NO_IMMUNITIES))
        return true;

    // These auras (Cyclone for example) are not dispelable
    if ((aura->HasAttribute(SPELL_ATTR1_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS) && aura->Mechanic != MECHANIC_NONE)
        || aura->HasAttribute(SPELL_ATTR2_NO_SCHOOL_IMMUNITIES))
    {
        return false;
    }

    return true;
}

bool SpellInfo::IsSingleTarget() const
{
    // all other single target spells have if it has AttributesEx5
    if (AttributesEx5 & SPELL_ATTR5_LIMIT_N)
        return true;

    return false;
}

bool SpellInfo::IsAuraExclusiveBySpecificWith(SpellInfo const* spellInfo) const
{
    SpellSpecificType spellSpec1 = GetSpellSpecific();
    SpellSpecificType spellSpec2 = spellInfo->GetSpellSpecific();
    switch (spellSpec1)
    {
        case SPELL_SPECIFIC_TRACKER:
        case SPELL_SPECIFIC_WARLOCK_ARMOR:
        case SPELL_SPECIFIC_MAGE_ARMOR:
        case SPELL_SPECIFIC_ELEMENTAL_SHIELD:
        case SPELL_SPECIFIC_MAGE_POLYMORPH:
        case SPELL_SPECIFIC_PRESENCE:
        case SPELL_SPECIFIC_CHARM:
        case SPELL_SPECIFIC_SCROLL:
        case SPELL_SPECIFIC_MAGE_ARCANE_BRILLANCE:
        case SPELL_SPECIFIC_PRIEST_DIVINE_SPIRIT:
            return spellSpec1 == spellSpec2;
        case SPELL_SPECIFIC_FOOD:
            return spellSpec2 == SPELL_SPECIFIC_FOOD
                   || spellSpec2 == SPELL_SPECIFIC_FOOD_AND_DRINK;
        case SPELL_SPECIFIC_DRINK:
            return spellSpec2 == SPELL_SPECIFIC_DRINK
                   || spellSpec2 == SPELL_SPECIFIC_FOOD_AND_DRINK;
        case SPELL_SPECIFIC_FOOD_AND_DRINK:
            return spellSpec2 == SPELL_SPECIFIC_FOOD
                   || spellSpec2 == SPELL_SPECIFIC_DRINK
                   || spellSpec2 == SPELL_SPECIFIC_FOOD_AND_DRINK;
        default:
            return false;
    }
}

bool SpellInfo::IsAuraExclusiveBySpecificPerCasterWith(SpellInfo const* spellInfo) const
{
    SpellSpecificType spellSpec = GetSpellSpecific();
    switch (spellSpec)
    {
        case SPELL_SPECIFIC_SEAL:
        case SPELL_SPECIFIC_HAND:
        case SPELL_SPECIFIC_AURA:
        case SPELL_SPECIFIC_STING:
        case SPELL_SPECIFIC_CURSE:
        case SPELL_SPECIFIC_ASPECT:
        case SPELL_SPECIFIC_JUDGEMENT:
        case SPELL_SPECIFIC_WARLOCK_CORRUPTION:
            return spellSpec == spellInfo->GetSpellSpecific();
        default:
            return false;
    }
}

SpellCastResult SpellInfo::CheckShapeshift(uint32 form) const
{
    // talents that learn spells can have stance requirements that need ignore
    // (this requirement only for client-side stance show in talent description)
    if (GetTalentSpellCost(Id) > 0 &&
            (Effects[0].Effect == SPELL_EFFECT_LEARN_SPELL || Effects[1].Effect == SPELL_EFFECT_LEARN_SPELL || Effects[2].Effect == SPELL_EFFECT_LEARN_SPELL))
        return SPELL_CAST_OK;

    uint32 stanceMask = (form ? 1 << (form - 1) : 0);

    if (stanceMask & StancesNot)                 // can explicitly not be casted in this stance
        return SPELL_FAILED_NOT_SHAPESHIFT;

    if (stanceMask & Stances)                    // can explicitly be casted in this stance
        return SPELL_CAST_OK;

    bool actAsShifted = false;
    SpellShapeshiftFormEntry const* shapeInfo = nullptr;
    if (form > 0)
    {
        shapeInfo = sSpellShapeshiftFormStore.LookupEntry(form);
        if (!shapeInfo)
        {
            LOG_ERROR("spells", "GetErrorAtShapeshiftedCast: unknown shapeshift {}", form);
            return SPELL_CAST_OK;
        }
        actAsShifted = !(shapeInfo->flags1 & SHAPESHIFT_FLAG_STANCE);            // shapeshift acts as normal form for spells
    }

    if (actAsShifted)
    {
        if (HasAttribute(SPELL_ATTR0_NOT_SHAPESHIFTED)) // not while shapeshifted
            return SPELL_FAILED_NOT_SHAPESHIFT;
        else if (Stances != 0)                   // needs other shapeshift
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }
    else
    {
        // needs shapeshift
        if (!(AttributesEx2 & SPELL_ATTR2_ALLOW_WHILE_NOT_SHAPESHIFTED) && Stances != 0)
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }

    // Check if stance disables cast of not-stance spells
    // Example: cannot cast any other spells in zombie or ghoul form
    /// @todo: Find a way to disable use of these spells clientside
    if (shapeInfo && (shapeInfo->flags1 & SHAPESHIFT_FLAG_CAN_ONLY_CAST_SHAPESHIFT_SPELLS))
    {
        if (!(stanceMask & Stances))
            return SPELL_FAILED_ONLY_SHAPESHIFT;
    }

    return SPELL_CAST_OK;
}

SpellCastResult SpellInfo::CheckLocation(uint32 map_id, uint32 zone_id, uint32 area_id, Player* player /*= nullptr*/, bool strict /*= true*/) const
{

    MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);

    // normal case
    if (AreaGroupId > 0)
    {
        bool found = false;
        AreaGroupEntry const* groupEntry = sAreaGroupStore.LookupEntry(AreaGroupId);
        while (groupEntry)
        {
            for (uint8 i = 0; i < MAX_GROUP_AREA_IDS; ++i)
                if (groupEntry->AreaId[i] == zone_id || groupEntry->AreaId[i] == area_id)
                    found = true;
            if (found || !groupEntry->nextGroup)
                break;
            // Try search in next group
            groupEntry = sAreaGroupStore.LookupEntry(groupEntry->nextGroup);
        }

        if (!found)
            return SPELL_FAILED_INCORRECT_AREA;
    }

    // continent limitation (virtual continent)
    if (HasAttribute(SPELL_ATTR4_ONLY_FLYING_AREAS) && (area_id || zone_id))
    {
        AreaTableEntry const* areaEntry = sAreaTableStore.LookupEntry(area_id);
        if (!areaEntry)
        {
            areaEntry = sAreaTableStore.LookupEntry(zone_id);
        }

        if (!areaEntry || !areaEntry->IsFlyable() || (strict && (areaEntry->flags & AREA_FLAG_NO_FLY_ZONE) != 0) || !player->canFlyInZone(map_id, zone_id, this))
        {
            return SPELL_FAILED_INCORRECT_AREA;
        }
    }

    // raid instance limitation
    if (HasAttribute(SPELL_ATTR6_NOT_IN_RAID_INSTANCES))
    {
        if (!mapEntry || mapEntry->IsRaid())
            return SPELL_FAILED_NOT_IN_RAID_INSTANCE;
    }

    // DB base check (if non empty then must fit at least single for allow)
    SpellAreaMapBounds saBounds = sSpellMgr->GetSpellAreaMapBounds(Id);
    if (saBounds.first != saBounds.second)
    {
        for (SpellAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            if (itr->second.IsFitToRequirements(player, zone_id, area_id))
                return SPELL_CAST_OK;
        }
        return SPELL_FAILED_INCORRECT_AREA;
    }

    // bg spell checks
    switch (Id)
    {
        case 23333:                                         // Warsong Flag
        case 23335:                                         // Silverwing Flag
            return map_id == 489 && player && player->InBattleground() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
        case 34976:                                         // Netherstorm Flag
            return map_id == 566 && player && player->InBattleground() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
        case 2584:                                          // Waiting to Resurrect
        case 22011:                                         // Spirit Heal Channel
        case 22012:                                         // Spirit Heal
        case 42792:                                         // Recently Dropped Flag
        case 43681:                                         // Inactive
        case 44535:                                         // Spirit Heal (mana)
            {
                if (!mapEntry)
                    return SPELL_FAILED_INCORRECT_AREA;

                return zone_id == 4197 || (mapEntry->IsBattleground() && player && player->InBattleground()) ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
            }
        case 32724:                                         // Gold Team (Alliance)
        case 32725:                                         // Green Team (Alliance)
        case 35774:                                         // Gold Team (Horde)
        case 35775:                                         // Green Team (Horde)
            {
                if (!mapEntry)
                    return SPELL_FAILED_INCORRECT_AREA;

                return mapEntry->IsBattleArena() && player && player->InBattleground() ? SPELL_CAST_OK : SPELL_FAILED_REQUIRES_AREA;
            }
    }
    if (VIP_SPELLS.count(Id))
    {
        // 统一处理逻辑
        if (!mapEntry)
            return SPELL_FAILED_INCORRECT_AREA;

        if (player && (mapEntry->IsBattlegroundOrArena() || area_id == 2177 || area_id == 1741))
            return SPELL_FAILED_INCORRECT_AREA;

        return SPELL_CAST_OK;
    }

    return SPELL_CAST_OK;
}

bool SpellInfo::IsStrongerAuraActive(Unit const* caster, Unit const* target) const
{
    if (!target)
        return false;

    // xinef: check spell group
    uint32 groupId = sSpellMgr->GetSpellGroup(Id);
    if (!groupId)
        return false;

    SpellGroupSpecialFlags sFlag = sSpellMgr->GetSpellGroupSpecialFlags(Id);
    if (sFlag & SPELL_GROUP_SPECIAL_FLAG_SKIP_STRONGER_CHECK)
        return false;

    for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // xinef: Skip Empty effects
        if (!Effects[i].IsEffect())
            continue;

        // xinef: if non-aura effect is preset - return false
        if (!Effects[i].IsAura())
            return false;

        // xinef: aura is periodic - return false
        if (Effects[i].Amplitude)
            return false;

        // xinef: exclude dummy auras
        if (Effects[i].ApplyAuraName == SPELL_AURA_DUMMY)
            return false;
    }

    for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // xinef: skip non-aura efects
        if (!Effects[i].IsAura())
            return false;

        Unit::AuraEffectList const& auraList = target->GetAuraEffectsByType((AuraType)Effects[i].ApplyAuraName);
        for (Unit::AuraEffectList::const_iterator iter = auraList.begin(); iter != auraList.end(); ++iter)
        {
            // xinef: aura is not groupped or in different group
            uint32 auraGroup = (*iter)->GetAuraGroup();
            if (!auraGroup || auraGroup != groupId)
                continue;

            if (IsRankOf((*iter)->GetSpellInfo()) && (sFlag & SPELL_GROUP_SPECIAL_FLAG_SKIP_STRONGER_SAME_SPELL))
            {
                continue;
            }

            // xinef: check priority before effect mask
            if (sFlag >= SPELL_GROUP_SPECIAL_FLAG_PRIORITY1 && sFlag <= SPELL_GROUP_SPECIAL_FLAG_PRIORITY4)
            {
                SpellGroupSpecialFlags sFlagCurr = sSpellMgr->GetSpellGroupSpecialFlags((*iter)->GetId());
                if (sFlagCurr >= SPELL_GROUP_SPECIAL_FLAG_PRIORITY1 && sFlagCurr <= SPELL_GROUP_SPECIAL_FLAG_PRIORITY4 && sFlagCurr < sFlag)
                {
                    return true;
                }
            }

            // xinef: check aura effect equal auras only, some auras have different effects on different ranks - check rank also
            if (!IsAuraEffectEqual((*iter)->GetSpellInfo()) && !IsRankOf((*iter)->GetSpellInfo()))
                continue;

            // xinef: misc value mismatches
            // xinef: commented, checked above
            //if (Effects[i].MiscValue != (*iter)->GetMiscValue())
            //  continue;

            // xinef: should not happen, or effect is not active - stronger one is present
            AuraApplication* aurApp = (*iter)->GetBase()->GetApplicationOfTarget(target->GetGUID());
            if (!aurApp || !aurApp->IsActive((*iter)->GetEffIndex()))
                continue;

            // xinef: assume that all spells are either positive or negative, otherwise they should not be in one group
            // xinef: take custom values into account

            int32 basePoints = Effects[i].BasePoints;
            int32 duration = GetMaxDuration();

            // xinef: should have the same id, can be different if spell is triggered
            // xinef: have to fix spell mods for triggered spell, turn off current spellmodtakingspell for preparing and restore after
            if (Player const* player = caster->GetSpellModOwner())
                if (player->m_spellModTakingSpell && player->m_spellModTakingSpell->m_spellInfo->Id == Id)
                    basePoints = player->m_spellModTakingSpell->GetSpellValue()->EffectBasePoints[i];

            int32 curValue = std::abs(Effects[i].CalcValue(caster, &basePoints));
            int32 auraValue = (sFlag & SPELL_GROUP_SPECIAL_FLAG_BASE_AMOUNT_CHECK) ?
                              std::abs((*iter)->GetSpellInfo()->Effects[(*iter)->GetEffIndex()].CalcValue((*iter)->GetCaster())) :
                              std::abs((*iter)->GetAmount());

            // xinef: for same spells, divide amount by stack amount
            if (Id == (*iter)->GetId())
                auraValue /= (*iter)->GetBase()->GetStackAmount();

            if (curValue < auraValue)
                return true;

            // xinef: little hack, if current spell is the same as aura spell, asume it is not stronger
            // xinef: if values are the same, duration mods should be taken into account but they are almost always passive
            if (curValue == auraValue)
            {
                if (Id == (*iter)->GetId())
                    continue;
                if (!(*iter)->GetBase()->IsPassive() && duration < (*iter)->GetBase()->GetDuration())
                    return true;
            }
        }
    }

    return false;
}

bool SpellInfo::IsAuraEffectEqual(SpellInfo const* otherSpellInfo) const
{
    uint8 matchCount = 0;
    uint8 auraCount = 0;
    for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
    {
        // xinef: skip non-aura efects
        if (!Effects[i].IsAura())
            continue;

        bool effectFound = false;
        for (uint8 j = EFFECT_0; j < MAX_SPELL_EFFECTS; ++j)
        {
            if (!otherSpellInfo->Effects[j].IsAura())
                continue;

            if (Effects[i].ApplyAuraName != otherSpellInfo->Effects[j].ApplyAuraName || Effects[i].MiscValue != otherSpellInfo->Effects[j].MiscValue)
                continue;

            effectFound = true;
            break;
        }

        if (!effectFound)
            return false;
        ++matchCount;
    }

    for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].IsAura())
            ++auraCount;
        if (otherSpellInfo->Effects[i].IsAura())
            ++auraCount;
    }

    return matchCount * 2 == auraCount;
}

bool SpellInfo::ValidateAttribute6SpellDamageMods(Unit const* caster, const AuraEffect* auraEffect, bool isDot) const
{
    // Xinef: no attribute
    if (!(AttributesEx6 & SPELL_ATTR6_IGNORE_CASTER_DAMAGE_MODIFIERS))
        return true;

    // Xinef we have a hook to decide which auras should profit to the spell, by default no profits
    // Xinef: Scourge Strike - Trigger
    if (Id == 70890 && auraEffect)
    {
        SpellInfo const* auraInfo = auraEffect->GetSpellInfo();
        return auraInfo->SpellIconID == 3086 ||
               (auraInfo->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT && (auraInfo->SpellFamilyFlags & flag96(8388608, 64, 16) || auraInfo->SpellIconID == 235 || auraInfo->SpellIconID == 154));
    }
    // Xinef: Necrosis, no profits for done and taken
    else if (Id == 51460)
        return false;

    // Xinef: Do not affect dots and auras obtained from items, only affected by self casted auras
    return !isDot && auraEffect && (auraEffect->GetAmount() < 0 || (auraEffect->GetCasterGUID() == caster->GetGUID() && auraEffect->GetSpellInfo()->SpellFamilyName == SpellFamilyName)) && !auraEffect->GetBase()->GetCastItemGUID();
}

SpellCastResult SpellInfo::CheckTarget(Unit const* caster, WorldObject const* target, bool implicit) const
{
    if (AttributesEx & SPELL_ATTR1_EXCLUDE_CASTER && caster == target)
        return SPELL_FAILED_BAD_TARGETS;

    // check visibility - ignore stealth for implicit (area) targets
    if (!(AttributesEx6 & SPELL_ATTR6_IGNORE_PHASE_SHIFT) && !caster->CanSeeOrDetect(target, implicit))
        return SPELL_FAILED_BAD_TARGETS;

    Unit const* unitTarget = target->ToUnit();

    // creature/player specific target checks
    if (unitTarget)
    {
        // xinef: spells cannot be cast if player is in fake combat also
        if (AttributesEx & SPELL_ATTR1_ONLY_PEACEFUL_TARGETS && (unitTarget->IsInCombat() || unitTarget->IsPetInCombat()))
            return SPELL_FAILED_TARGET_AFFECTING_COMBAT;

        // only spells with SPELL_ATTR3_ONLY_ON_GHOSTS can target ghosts
        if (IsRequiringDeadTarget())
        {
            if (!unitTarget->HasGhostAura())
                return SPELL_FAILED_TARGET_NOT_GHOST;
            if (!IsDeathPersistent() && !IsAllowingDeadTarget())
                return SPELL_FAILED_BAD_TARGETS;
        }

        if (caster != unitTarget)
        {
            if (caster->IsPlayer())
            {
                // Do not allow these spells to target creatures not tapped by us (Banish, Polymorph, many quest spells)
                if (AttributesEx2 & SPELL_ATTR2_CANNOT_CAST_ON_TAPPED)
                    if (Creature const* targetCreature = unitTarget->ToCreature())
                        if (targetCreature->hasLootRecipient() && !targetCreature->isTappedBy(caster->ToPlayer()))
                            return SPELL_FAILED_CANT_CAST_ON_TAPPED;

                if (HasAttribute(SPELL_ATTR0_CU_PICKPOCKET))
                {
                    Creature const* targetCreature = unitTarget->ToCreature();
                    if (!targetCreature)
                        return SPELL_FAILED_BAD_TARGETS;

                    if (!LootTemplates_Pickpocketing.HaveLootFor(targetCreature->GetCreatureTemplate()->pickpocketLootId))
                        return SPELL_FAILED_TARGET_NO_POCKETS;
                }

                // Not allow disarm unarmed player
                if (Mechanic == MECHANIC_DISARM)
                {
                    bool valid = false;
                    for (uint8 i = BASE_ATTACK; i < MAX_ATTACK; ++i)
                    {
                        AuraType disarmAuraType = SPELL_AURA_MOD_DISARM;
                        switch (i)
                        {
                            case OFF_ATTACK:
                                disarmAuraType = SPELL_AURA_MOD_DISARM_OFFHAND;
                                break;
                            case RANGED_ATTACK:
                                disarmAuraType = SPELL_AURA_MOD_DISARM_RANGED;
                                break;
                        }

                        if (HasAura(disarmAuraType))
                        {
                            if (Player const* player = unitTarget->ToPlayer())
                            {
                                if (player->GetWeaponForAttack(WeaponAttackType(BASE_ATTACK + i), true))
                                {
                                    valid = true;
                                    break;
                                }
                            }
                            else if (unitTarget->GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + i))
                            {
                                valid = true;
                                break;
                            }
                        }
                    }

                    if (!valid)
                    {
                        return SPELL_FAILED_TARGET_NO_WEAPONS;
                    }
                }
            }
        }
    }
    // corpse specific target checks
    else if (Corpse const* corpseTarget = target->ToCorpse())
    {
        // cannot target bare bones
        if (corpseTarget->GetType() == CORPSE_BONES)
            return SPELL_FAILED_BAD_TARGETS;
        // we have to use owner for some checks (aura preventing resurrection for example)
        if (Player* owner = ObjectAccessor::FindPlayer(corpseTarget->GetOwnerGUID()))
            unitTarget = owner;
        // we're not interested in corpses without owner
        else
            return SPELL_FAILED_BAD_TARGETS;
    }
    // other types of objects - always valid
    else return SPELL_CAST_OK;

    // corpseOwner and unit specific target checks
    if (AttributesEx3 & SPELL_ATTR3_ONLY_ON_PLAYER && !unitTarget->ToPlayer())
        //npcbot: allow to target bots
        if (!unitTarget->IsNPCBot())
        //end npcbot
        return SPELL_FAILED_TARGET_NOT_PLAYER;

    if (!IsAllowingDeadTarget() && !unitTarget->IsAlive())
        return SPELL_FAILED_TARGETS_DEAD;

    // check this flag only for implicit targets (chain and area), allow to explicitly target units for spells like Shield of Righteousness
    if (implicit && AttributesEx6 & SPELL_ATTR6_DO_NOT_CHAIN_TO_CROWD_CONTROLLED_TARGETS && unitTarget->HasUnitState(UNIT_STATE_CONTROLLED))
        return SPELL_FAILED_BAD_TARGETS;

    // checked in Unit::IsValidAttack/AssistTarget, shouldn't be checked for ENTRY targets
    //if (!(AttributesEx6 & SPELL_ATTR6_CAN_TARGET_UNTARGETABLE) && target->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
    //    return SPELL_FAILED_BAD_TARGETS;

    //if (!(AttributesEx6 & SPELL_ATTR6_NO_AURA_LOG)

    if (!CheckTargetCreatureType(unitTarget))
    {
        if (target->IsPlayer())
            return SPELL_FAILED_TARGET_IS_PLAYER;
        else
            return SPELL_FAILED_BAD_TARGETS;
    }

    // check GM mode and GM invisibility - only for player casts (npc casts are controlled by AI) and negative spells
    if (unitTarget != caster && (caster->IsControlledByPlayer() || !IsPositive()) && unitTarget->IsPlayer())
    {
        if (!unitTarget->ToPlayer()->IsVisible())
            return SPELL_FAILED_BM_OR_INVISGOD;

        if (unitTarget->ToPlayer()->IsGameMaster())
            return SPELL_FAILED_BM_OR_INVISGOD;
    }

    // not allow casting on flying player
    if (unitTarget->IsInFlight() && !HasAttribute(SPELL_ATTR0_CU_ALLOW_INFLIGHT_TARGET))
        return SPELL_FAILED_BAD_TARGETS;

    /* TARGET_UNIT_MASTER gets blocked here for passengers, because the whole idea of this check is to
    not allow passengers to be implicitly hit by spells, however this target type should be an exception,
    if this is left it kills spells that award kill credit from vehicle to master (few spells),
    the use of these 2 covers passenger target check, logically, if vehicle cast this to master it should always hit
    him, because it would be it's passenger, there's no such case where this gets to fail legitimacy, this problem
    cannot be solved from within the check in other way since target type cannot be called for the spell currently
    Spell examples: [ID - 52864 Devour Water, ID - 52862 Devour Wind, ID - 49370 Wyrmrest Defender: Destabilize Azure Dragonshrine Effect] */
    if (!caster->IsVehicle() && caster->GetCharmerOrOwner() != target)
    {
        if (TargetAuraState && !unitTarget->HasAuraState(AuraStateType(TargetAuraState), this, caster))
            return SPELL_FAILED_TARGET_AURASTATE;

        if (TargetAuraStateNot && unitTarget->HasAuraState(AuraStateType(TargetAuraStateNot), this, caster))
            return SPELL_FAILED_TARGET_AURASTATE;
    }

    if (TargetAuraSpell && !unitTarget->HasAura(sSpellMgr->GetSpellIdForDifficulty(TargetAuraSpell, caster)))
        return SPELL_FAILED_TARGET_AURASTATE;

    if (ExcludeTargetAuraSpell && unitTarget->HasAura(sSpellMgr->GetSpellIdForDifficulty(ExcludeTargetAuraSpell, caster)))
        return SPELL_FAILED_TARGET_AURASTATE;

    if (unitTarget->HasPreventResurectionAura() && !HasAttribute(SPELL_ATTR7_BYPASS_NO_RESURRECTION_AURA))
        if (HasEffect(SPELL_EFFECT_SELF_RESURRECT) || HasEffect(SPELL_EFFECT_RESURRECT) || HasEffect(SPELL_EFFECT_RESURRECT_NEW))
            return SPELL_FAILED_TARGET_CANNOT_BE_RESURRECTED;

    // xinef: check if stronger aura is active
    if (IsStrongerAuraActive(caster, unitTarget))
        return SPELL_FAILED_AURA_BOUNCED;

    return SPELL_CAST_OK;
}

SpellCastResult SpellInfo::CheckExplicitTarget(Unit const* caster, WorldObject const* target, Item const* itemTarget) const
{
    uint32 neededTargets = GetExplicitTargetMask();
    if (!target)
    {
        if (neededTargets & (TARGET_FLAG_UNIT_MASK | TARGET_FLAG_GAMEOBJECT_MASK | TARGET_FLAG_CORPSE_MASK))
            if (!(neededTargets & TARGET_FLAG_GAMEOBJECT_ITEM) || !itemTarget)
                return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    if (Unit const* unitTarget = target->ToUnit())
    {
        if (neededTargets & (TARGET_FLAG_UNIT_ENEMY | TARGET_FLAG_UNIT_ALLY | TARGET_FLAG_UNIT_RAID | TARGET_FLAG_UNIT_PARTY | TARGET_FLAG_UNIT_MINIPET | TARGET_FLAG_UNIT_PASSENGER))
        {
            if (neededTargets & TARGET_FLAG_UNIT_ENEMY)
                if (caster->_IsValidAttackTarget(unitTarget, this))
                    return SPELL_CAST_OK;
            if (neededTargets & TARGET_FLAG_UNIT_ALLY
                    || (neededTargets & TARGET_FLAG_UNIT_PARTY && caster->IsInPartyWith(unitTarget))
                    || (neededTargets & TARGET_FLAG_UNIT_RAID && caster->IsInRaidWith(unitTarget)))
                if (caster->_IsValidAssistTarget(unitTarget, this))
                    return SPELL_CAST_OK;
            if (neededTargets & TARGET_FLAG_UNIT_MINIPET)
                if (unitTarget->GetGUID() == caster->GetCritterGUID())
                    return SPELL_CAST_OK;
            if (neededTargets & TARGET_FLAG_UNIT_PASSENGER)
                if (unitTarget->IsOnVehicle(caster))
                    return SPELL_CAST_OK;
            return SPELL_FAILED_BAD_TARGETS;
        }
        //npcbot
        else if ((neededTargets & TARGET_FLAG_CORPSE_ALLY) && unitTarget->IsNPCBot())
        {
            if (!caster->_IsValidAssistTarget(unitTarget, this))
                return SPELL_FAILED_BAD_TARGETS;
        }
        //end npcbot
    }
    return SPELL_CAST_OK;
}

bool SpellInfo::CheckTargetCreatureType(Unit const* target) const
{
    // Curse of Doom & Exorcism: not find another way to fix spell target check :/
    if (SpellFamilyName == SPELLFAMILY_WARLOCK && GetCategory() == 1179)
    {
        // not allow cast at player
        if (target->IsPlayer())
            return false;
        else
            return true;
    }
    uint32 creatureType = target->GetCreatureTypeMask();
    return !TargetCreatureType || !creatureType || (creatureType & TargetCreatureType);
}

SpellSchoolMask SpellInfo::GetSchoolMask() const
{
    return SpellSchoolMask(SchoolMask);
}

uint32 SpellInfo::GetAllEffectsMechanicMask() const
{
    uint32 mask = 0;
    if (Mechanic)
        mask |= 1 << Mechanic;
    for (int i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].IsEffect() && Effects[i].Mechanic)
            mask |= 1 << Effects[i].Mechanic;
    return mask;
}

uint32 SpellInfo::GetEffectMechanicMask(uint8 effIndex) const
{
    uint32 mask = 0;
    if (Mechanic)
        mask |= 1 << Mechanic;
    if (Effects[effIndex].IsEffect() && Effects[effIndex].Mechanic)
        mask |= 1 << Effects[effIndex].Mechanic;
    return mask;
}

uint32 SpellInfo::GetSpellMechanicMaskByEffectMask(uint32 effectMask) const
{
    uint32 mask = 0;
    if (Mechanic)
        mask |= 1 << Mechanic;
    for (int i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if ((effectMask & (1 << i)) && Effects[i].Mechanic)
            mask |= 1 << Effects[i].Mechanic;
    return mask;
}

Mechanics SpellInfo::GetEffectMechanic(uint8 effIndex) const
{
    if (Effects[effIndex].IsEffect() && Effects[effIndex].Mechanic)
        return Mechanics(Effects[effIndex].Mechanic);
    if (Mechanic)
        return Mechanics(Mechanic);
    return MECHANIC_NONE;
}

bool SpellInfo::HasAnyEffectMechanic() const
{
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (Effects[i].Mechanic)
            return true;
    return false;
}

uint32 SpellInfo::GetDispelMask() const
{
    return GetDispelMask(DispelType(Dispel));
}

uint32 SpellInfo::GetDispelMask(DispelType type)
{
    // If dispel all
    if (type == DISPEL_ALL)
        return DISPEL_ALL_MASK;
    else
        return uint32(1 << type);
}

uint32 SpellInfo::GetExplicitTargetMask() const
{
    return ExplicitTargetMask;
}

AuraStateType SpellInfo::GetAuraState() const
{
    return _auraState;
}

AuraStateType SpellInfo::LoadAuraState() const
{
    // Seals
    if (GetSpellSpecific() == SPELL_SPECIFIC_SEAL)
        return AURA_STATE_JUDGEMENT;

    // Conflagrate aura state on Immolate and Shadowflame
    if (SpellFamilyName == SPELLFAMILY_WARLOCK &&
            // Immolate
            ((SpellFamilyFlags[0] & 4) ||
             // Shadowflame
             (SpellFamilyFlags[2] & 2)))
        return AURA_STATE_CONFLAGRATE;

    // Faerie Fire (druid versions)
    if (SpellFamilyName == SPELLFAMILY_DRUID && SpellFamilyFlags[0] & 0x400)
        return AURA_STATE_FAERIE_FIRE;

    // Sting (hunter's pet ability)
    if (GetCategory() == 1133)
        return AURA_STATE_FAERIE_FIRE;

    // Victorious
    if (SpellFamilyName == SPELLFAMILY_WARRIOR &&  SpellFamilyFlags[1] & 0x00040000)
        return AURA_STATE_WARRIOR_VICTORY_RUSH;

    // Swiftmend state on Regrowth & Rejuvenation
    if (SpellFamilyName == SPELLFAMILY_DRUID && SpellFamilyFlags[0] & 0x50)
        return AURA_STATE_SWIFTMEND;

    // Deadly poison aura state
    if (SpellFamilyName == SPELLFAMILY_ROGUE && SpellFamilyFlags[0] & 0x10000)
        return AURA_STATE_DEADLY_POISON;

    // Enrage aura state
    if (Dispel == DISPEL_ENRAGE)
        return AURA_STATE_ENRAGE;

    // Bleeding aura state
    if (GetAllEffectsMechanicMask() & 1 << MECHANIC_BLEED)
        return AURA_STATE_BLEEDING;

    if (GetSchoolMask() & SPELL_SCHOOL_MASK_FROST)
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            if (Effects[i].IsAura() && (Effects[i].ApplyAuraName == SPELL_AURA_MOD_STUN
                                        || Effects[i].ApplyAuraName == SPELL_AURA_MOD_ROOT))
                return AURA_STATE_FROZEN;

    switch (Id)
    {
        case 71465: // Divine Surge
        case 50241: // Oculus, Drake spell Evasive Maneuvers
            return AURA_STATE_UNKNOWN22;
        case 9991:  // Touch of Zanzil
        case 35331: // Black Blood
        case 9806:  // Phantom Strike
        case 35325: // Glowing Blood
        case 35328: // Lambent Blood
        case 35329: // Vibrant Blood
        case 16498: // Faerie Fire
        case 6950:
        case 20656:
        case 25602:
        case 32129:
            return AURA_STATE_FAERIE_FIRE;
        default:
            break;
    }

    return AURA_STATE_NONE;
}

SpellSpecificType SpellInfo::GetSpellSpecific() const
{
    return _spellSpecific;
}

SpellSpecificType SpellInfo::LoadSpellSpecific() const
{
    switch (SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
            {
                // Food / Drinks (mostly)
                if (AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
                {
                    bool food = false;
                    bool drink = false;
                    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    {
                        if (!Effects[i].IsAura())
                            continue;
                        switch (Effects[i].ApplyAuraName)
                        {
                            // Food
                            case SPELL_AURA_MOD_REGEN:
                            case SPELL_AURA_OBS_MOD_HEALTH:
                                food = true;
                                break;
                            // Drink
                            case SPELL_AURA_MOD_POWER_REGEN:
                            case SPELL_AURA_OBS_MOD_POWER:
                                drink = true;
                                break;
                            default:
                                break;
                        }
                    }

                    if (food && drink)
                        return SPELL_SPECIFIC_FOOD_AND_DRINK;
                    else if (food)
                        return SPELL_SPECIFIC_FOOD;
                    else if (drink)
                        return SPELL_SPECIFIC_DRINK;
                }
                // scrolls effects
                else
                {
                    SpellInfo const* firstRankSpellInfo = GetFirstRankSpell();
                    switch (firstRankSpellInfo->Id)
                    {
                        case 8118: // Strength
                        case 8099: // Stamina
                        case 8112: // Spirit
                        case 8096: // Intellect
                        case 8115: // Agility
                        case 8091: // Armor
                            return SPELL_SPECIFIC_SCROLL;
                    }
                }
                break;
            }
        case SPELLFAMILY_MAGE:
            {
                // family flags 18(Molten), 25(Frost/Ice), 28(Mage)
                if (SpellFamilyFlags[0] & 0x12040000)
                    return SPELL_SPECIFIC_MAGE_ARMOR;

                // Arcane brillance and Arcane intelect (normal check fails because of flags difference)
                if (SpellFamilyFlags[0] & 0x400)
                    return SPELL_SPECIFIC_MAGE_ARCANE_BRILLANCE;

                if ((SpellFamilyFlags[0] & 0x1000000) && Effects[0].ApplyAuraName == SPELL_AURA_MOD_CONFUSE)
                    return SPELL_SPECIFIC_MAGE_POLYMORPH;

                break;
            }
        case SPELLFAMILY_WARLOCK:
            {
                // only warlock curses have this
                if (Dispel == DISPEL_CURSE)
                    return SPELL_SPECIFIC_CURSE;

                // Warlock (Demon Armor | Demon Skin | Fel Armor)
                if (SpellFamilyFlags[1] & 0x20000020 || SpellFamilyFlags[2] & 0x00000010)
                    return SPELL_SPECIFIC_WARLOCK_ARMOR;

                //seed of corruption and corruption
                if (SpellFamilyFlags[1] & 0x10 || SpellFamilyFlags[0] & 0x2)
                    return SPELL_SPECIFIC_WARLOCK_CORRUPTION;
                break;
            }
        case SPELLFAMILY_PRIEST:
            {
                // Divine Spirit and Prayer of Spirit
                if (SpellFamilyFlags[0] & 0x20)
                    return SPELL_SPECIFIC_PRIEST_DIVINE_SPIRIT;

                break;
            }
        case SPELLFAMILY_HUNTER:
            {
                // only hunter stings have this
                if (Dispel == DISPEL_POISON)
                    return SPELL_SPECIFIC_STING;

                // only hunter aspects have this (but not all aspects in hunter family)
                if (SpellFamilyFlags.HasFlag(0x00380000, 0x00440000, 0x00001010))
                    return SPELL_SPECIFIC_ASPECT;

                break;
            }
        case SPELLFAMILY_PALADIN:
            {
                // Collection of all the seal family flags. No other paladin spell has any of those.
                if (SpellFamilyFlags[1] & 0x26000C00
                        || SpellFamilyFlags[0] & 0x0A000000)
                    return SPELL_SPECIFIC_SEAL;

                if (SpellFamilyFlags[0] & 0x00002190)
                    return SPELL_SPECIFIC_HAND;

                // Judgement of Wisdom, Judgement of Light, Judgement of Justice
                if (Id == 20184 || Id == 20185 || Id == 20186)
                    return SPELL_SPECIFIC_JUDGEMENT;

                // only paladin auras have this (for palaldin class family)
                if (SpellFamilyFlags[2] & 0x00000020)
                    return SPELL_SPECIFIC_AURA;

                // Illidari Council Paladin (Gathios the Shatterer)
                if (Id == 41459 || Id == 41469)
                    return SPELL_SPECIFIC_SEAL;

                break;
            }
        case SPELLFAMILY_SHAMAN:
            {
                // family flags 10 (Lightning), 42 (Earth), 37 (Water), proc shield from T2 8 pieces bonus
                if (SpellFamilyFlags[1] & 0x420
                        || (SpellFamilyFlags[0] & 0x00000400 && HasAttribute(SPELL_ATTR1_NO_THREAT))
                        || Id == 23552)
                    return SPELL_SPECIFIC_ELEMENTAL_SHIELD;

                break;
            }
        case SPELLFAMILY_DEATHKNIGHT:
            if (Id == 48266 || Id == 48263 || Id == 48265)
                return SPELL_SPECIFIC_PRESENCE;
            break;
    }

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].Effect == SPELL_EFFECT_APPLY_AURA)
        {
            switch (Effects[i].ApplyAuraName)
            {
                case SPELL_AURA_MOD_CHARM:
                case SPELL_AURA_MOD_POSSESS_PET:
                case SPELL_AURA_MOD_POSSESS:
                case SPELL_AURA_AOE_CHARM:
                    return SPELL_SPECIFIC_CHARM;
                case SPELL_AURA_TRACK_CREATURES:
                    /// @workaround For non-stacking tracking spells (We need generic solution)
                    if (Id == 30645) // Gas Cloud Tracking
                        return SPELL_SPECIFIC_NORMAL;
                    [[fallthrough]]; /// @todo: Not sure whether the fallthrough was a mistake (forgetting a break) or intended. This should be double-checked.
                case SPELL_AURA_TRACK_RESOURCES:
                case SPELL_AURA_TRACK_STEALTHED:
                    return SPELL_SPECIFIC_TRACKER;
            }
        }
    }

    return SPELL_SPECIFIC_NORMAL;
}

float SpellInfo::GetMinRange(bool positive) const
{
    if (!RangeEntry)
        return 0.0f;
    if (positive)
        return RangeEntry->RangeMin[1];
    return RangeEntry->RangeMin[0];
}

float SpellInfo::GetMaxRange(bool positive, Unit* caster, Spell* spell) const
{
    if (!RangeEntry)
        return 0.0f;
    float range;
    if (positive)
        range = RangeEntry->RangeMax[1];
    else
        range = RangeEntry->RangeMax[0];
    if (caster)
        if (Player* modOwner = caster->GetSpellModOwner())
            modOwner->ApplySpellMod(Id, SPELLMOD_RANGE, range, spell);
    return range;
}

int32 SpellInfo::GetDuration() const
{
    if (!DurationEntry)
        return 0;
    return (DurationEntry->Duration[0] == -1) ? -1 : std::abs(DurationEntry->Duration[0]);
}

int32 SpellInfo::GetMaxDuration() const
{
    if (!DurationEntry)
        return 0;
    return (DurationEntry->Duration[2] == -1) ? -1 : std::abs(DurationEntry->Duration[2]);
}

uint32 SpellInfo::CalcCastTime(Unit* caster, Spell* spell) const
{
    // not all spells have cast time index and this is all is pasiive abilities
    if (!CastTimeEntry)
        return 0;

    int32 castTime = CastTimeEntry->CastTime;
    if (HasAttribute(SPELL_ATTR0_USES_RANGED_SLOT) && (!IsAutoRepeatRangedSpell()))
        castTime += 500;

    if (caster)
        caster->ModSpellCastTime(this, castTime, spell);

    return (castTime > 0) ? uint32(castTime) : 0;
}

uint32 SpellInfo::GetMaxTicks() const
{
    int32 DotDuration = GetDuration();
    if (DotDuration == 0)
        return 1;

    // 200% limit
    if (DotDuration > 30000)
        DotDuration = 30000;

    for (uint8 x = 0; x < MAX_SPELL_EFFECTS; x++)
    {
        if (Effects[x].Effect == SPELL_EFFECT_APPLY_AURA)
            switch (Effects[x].ApplyAuraName)
            {
                case SPELL_AURA_PERIODIC_DAMAGE:
                case SPELL_AURA_PERIODIC_HEAL:
                case SPELL_AURA_PERIODIC_LEECH:
                case SPELL_AURA_PERIODIC_TRIGGER_SPELL_FROM_CLIENT:
                    if (Effects[x].Amplitude != 0)
                        return DotDuration / Effects[x].Amplitude;
                    break;
            }
    }

    return 6;
}

uint32 SpellInfo::GetRecoveryTime() const
{
    return RecoveryTime > CategoryRecoveryTime ? RecoveryTime : CategoryRecoveryTime;
}

int32 SpellInfo::CalcPowerCost(Unit const* caster, SpellSchoolMask schoolMask, Spell* spell) const
{
    // Spell drain all exist power on cast (Only paladin lay of Hands)
    if (AttributesEx & SPELL_ATTR1_USE_ALL_MANA)
    {
        // If power type - health drain all
        if (PowerType == POWER_HEALTH)
            return caster->GetHealth();
        // Else drain all power
        if (PowerType < MAX_POWERS)
            return caster->GetPower(Powers(PowerType));
        LOG_ERROR("spells", "SpellInfo::CalcPowerCost: Unknown power type '{}' in spell {}", PowerType, Id);
        return 0;
    }

    // Base powerCost
    int32 powerCost = ManaCost;
    // PCT cost from total amount
    if (ManaCostPercentage)
    {
        switch (PowerType)
        {
            // health as power used
            case POWER_HEALTH:
                powerCost += int32(CalculatePct(caster->GetCreateHealth(), ManaCostPercentage));
                break;
            case POWER_MANA:
                powerCost += int32(CalculatePct(caster->GetCreateMana(), ManaCostPercentage));
                break;
            case POWER_RAGE:
            case POWER_FOCUS:
            case POWER_ENERGY:
            case POWER_HAPPINESS:
                powerCost += int32(CalculatePct(caster->GetMaxPower(Powers(PowerType)), ManaCostPercentage));
                break;
            case POWER_RUNE:
            case POWER_RUNIC_POWER:
                LOG_DEBUG("spells.aura", "CalculateManaCost: Not implemented yet!");
                break;
            default:
                LOG_ERROR("spells", "CalculateManaCost: Unknown power type '{}' in spell {}", PowerType, Id);
                return 0;
        }
    }
    SpellSchools school = GetFirstSchoolInMask(schoolMask);
    // Flat mod from caster auras by spell school
    powerCost += caster->GetInt32Value(static_cast<uint16>(UNIT_FIELD_POWER_COST_MODIFIER) + school);

    // Shiv - costs 20 + weaponSpeed*10 energy (apply only to non-triggered spell with energy cost)
    if (AttributesEx4 & SPELL_ATTR4_WEAPON_SPEED_COST_SCALING)
    {
        uint32 speed = 0;
        if (SpellShapeshiftFormEntry const* ss = sSpellShapeshiftFormStore.LookupEntry(caster->GetShapeshiftForm()))
            speed = ss->attackSpeed;
        else
        {
            WeaponAttackType slot = BASE_ATTACK;
            if (AttributesEx3 & SPELL_ATTR3_REQUIRES_OFF_HAND_WEAPON)
                slot = OFF_ATTACK;

            speed = caster->GetAttackTime(slot);
        }

        powerCost += speed / 100;
    }

    // Apply cost mod by spell
    if (Player* modOwner = caster->GetSpellModOwner())
        modOwner->ApplySpellMod(Id, SPELLMOD_COST, powerCost, spell);

    if (!caster->IsControlledByPlayer())
    {
        if (HasAttribute(SPELL_ATTR0_SCALES_WITH_CREATURE_LEVEL))
        {
            GtNPCManaCostScalerEntry const* spellScaler = sGtNPCManaCostScalerStore.LookupEntry(SpellLevel - 1);
            GtNPCManaCostScalerEntry const* casterScaler = sGtNPCManaCostScalerStore.LookupEntry(caster->GetLevel() - 1);
            if (spellScaler && casterScaler)
                powerCost *= casterScaler->ratio / spellScaler->ratio;
        }
    }

    //npcbot - apply bot spell cost mods
    if (powerCost > 0 && caster->IsNPCBot())
        caster->ToCreature()->ApplyCreatureSpellCostMods(this, powerCost);
    //end npcbot

    // PCT mod from user auras by school
    powerCost = int32(powerCost * (1.0f + caster->GetFloatValue(static_cast<uint16>(UNIT_FIELD_POWER_COST_MULTIPLIER) + school)));
    if (powerCost < 0)
        powerCost = 0;
    return powerCost;
}

bool SpellInfo::IsRanked() const
{
    return ChainEntry != nullptr;
}

uint8 SpellInfo::GetRank() const
{
    if (!ChainEntry)
        return 1;
    return ChainEntry->rank;
}

SpellInfo const* SpellInfo::GetFirstRankSpell() const
{
    if (!ChainEntry)
        return this;
    return ChainEntry->first;
}
SpellInfo const* SpellInfo::GetLastRankSpell() const
{
    if (!ChainEntry)
        return nullptr;
    return ChainEntry->last;
}
SpellInfo const* SpellInfo::GetNextRankSpell() const
{
    if (!ChainEntry)
        return nullptr;
    return ChainEntry->next;
}
SpellInfo const* SpellInfo::GetPrevRankSpell() const
{
    if (!ChainEntry)
        return nullptr;
    return ChainEntry->prev;
}

SpellInfo const* SpellInfo::GetAuraRankForLevel(uint8 level) const
{
    // ignore passive spells
    //if (IsPassive())
    //    return this;

    // Client ignores spell with these attributes (sub_53D9D0)
    if (HasAttribute(SPELL_ATTR0_COOLDOWN_ON_EVENT) || HasAttribute(SPELL_ATTR2_ALLOW_LOW_LEVEL_BUFF) || HasAttribute(SPELL_ATTR3_ONLY_PROC_ON_CASTER))
        return this;

    bool needRankSelection = false;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (IsPositiveEffect(i) &&
                (Effects[i].Effect == SPELL_EFFECT_APPLY_AURA ||
                 Effects[i].Effect == SPELL_EFFECT_APPLY_AREA_AURA_PARTY ||
                 Effects[i].Effect == SPELL_EFFECT_APPLY_AREA_AURA_RAID))
        {
            needRankSelection = true;
            break;
        }
    }

    // not required
    if (!needRankSelection)
        return this;

    SpellInfo const* nextSpellInfo = nullptr;

    sScriptMgr->OnBeforeAuraRankForLevel(this, nextSpellInfo, level);

    if (nextSpellInfo != nullptr)
        return nextSpellInfo;

    for (nextSpellInfo = this; nextSpellInfo != nullptr; nextSpellInfo = nextSpellInfo->GetPrevRankSpell())
    {
        // if found appropriate level
        if (uint32(level + 10) >= nextSpellInfo->SpellLevel)
            return nextSpellInfo;

        // one rank less then
    }

    // not found
    return nullptr;
}

bool SpellInfo::IsRankOf(SpellInfo const* spellInfo) const
{
    return GetFirstRankSpell() == spellInfo->GetFirstRankSpell();
}

bool SpellInfo::IsDifferentRankOf(SpellInfo const* spellInfo) const
{
    if (Id == spellInfo->Id)
        return false;
    return IsRankOf(spellInfo);
}

bool SpellInfo::IsHighRankOf(SpellInfo const* spellInfo) const
{
    if (ChainEntry && spellInfo->ChainEntry)
    {
        if (ChainEntry->first == spellInfo->ChainEntry->first)
            if (ChainEntry->rank > spellInfo->ChainEntry->rank)
                return true;
    }
    return false;
}

void SpellInfo::_InitializeExplicitTargetMask()
{
    bool srcSet = false;
    bool dstSet = false;
    uint32 targetMask = Targets;
    // prepare target mask using effect target entries
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!Effects[i].IsEffect())
            continue;
        targetMask |= Effects[i].TargetA.GetExplicitTargetMask(srcSet, dstSet);
        targetMask |= Effects[i].TargetB.GetExplicitTargetMask(srcSet, dstSet);

        // add explicit target flags based on spell effects which have EFFECT_IMPLICIT_TARGET_EXPLICIT and no valid target provided
        if (Effects[i].GetImplicitTargetType() != EFFECT_IMPLICIT_TARGET_EXPLICIT)
            continue;

        // extend explicit target mask only if valid targets for effect could not be provided by target types
        uint32 effectTargetMask = Effects[i].GetMissingTargetMask(srcSet, dstSet, targetMask);

        // don't add explicit object/dest flags when spell has no max range
        if (GetMaxRange(true) == 0.0f && GetMaxRange(false) == 0.0f)
            effectTargetMask &= ~(TARGET_FLAG_UNIT_MASK | TARGET_FLAG_GAMEOBJECT | TARGET_FLAG_CORPSE_MASK | TARGET_FLAG_DEST_LOCATION);
        targetMask |= effectTargetMask;
    }
    ExplicitTargetMask = targetMask;
}

bool SpellInfo::_IsPositiveEffect(uint8 effIndex, bool deep) const
{
    // not found a single positive spell with this attribute
    if (HasAttribute(SPELL_ATTR0_AURA_IS_DEBUFF))
        return false;

    switch (Mechanic)
    {
        case MECHANIC_IMMUNE_SHIELD:
            return true;
        default:
            break;
    }

    // Special case: effects which determine positivity of whole spell
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (Effects[i].IsAura() && Effects[i].ApplyAuraName == SPELL_AURA_MOD_STEALTH)
            return true;
    }

    switch (Effects[effIndex].Effect)
    {
        case SPELL_EFFECT_DUMMY:
            // some explicitly required dummy effect sets
            switch (Id)
            {
                default:
                    break;
            }
            break;
        // always positive effects (check before target checks that provided non-positive result in some case for positive effects)
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_LEARN_SPELL:
        case SPELL_EFFECT_SKILL_STEP:
        case SPELL_EFFECT_HEAL_PCT:
        case SPELL_EFFECT_ENERGIZE_PCT:
            return true;
        case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
            return false;

        case SPELL_EFFECT_GAMEOBJECT_DAMAGE:
            return false;

        case SPELL_EFFECT_SCHOOL_DAMAGE:
            {
                bool only = true;
                for (int i = EFFECT_0; i <= EFFECT_2; ++i)
                {
                    if (Effects[effIndex].Effect > 0 && Effects[effIndex].Effect != SPELL_EFFECT_SCHOOL_DAMAGE)
                        only = false;
                    if (Effects[effIndex].Effect == SPELL_EFFECT_GAMEOBJECT_DAMAGE)
                        return false;
                }
                // effects with school damage only cannot be positive...
                if (only)
                    return false;
                break;
            }
        case SPELL_EFFECT_KNOCK_BACK:
        case SPELL_EFFECT_KNOCK_BACK_DEST:
            {
                for (int i = EFFECT_0; i <= EFFECT_2; ++i)
                    if (Effects[effIndex].Effect == SPELL_EFFECT_GAMEOBJECT_DAMAGE)
                        return false;
                break;
            }

        // non-positive aura use
        case SPELL_EFFECT_APPLY_AURA:
        case SPELL_EFFECT_APPLY_AREA_AURA_FRIEND:
            {
                switch (Effects[effIndex].ApplyAuraName)
                {
                    case SPELL_AURA_MOD_DAMAGE_DONE:            // dependent from bas point sign (negative -> negative)
                    case SPELL_AURA_MOD_STAT:
                    case SPELL_AURA_MOD_SKILL:
                    case SPELL_AURA_MOD_DODGE_PERCENT:
                    case SPELL_AURA_MOD_HEALING_PCT:
                    case SPELL_AURA_MOD_HEALING_DONE:
                    case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
                        if (Effects[effIndex].CalcValue() < 0)
                            return false;
                        break;
                    case SPELL_AURA_MOD_DAMAGE_TAKEN:           // dependent from bas point sign (positive -> negative)
                        if (Effects[effIndex].CalcValue() > 0)
                            return false;
                        break;
                    case SPELL_AURA_MOD_CRIT_PCT:
                    case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
                        if (Effects[effIndex].CalcValue() > 0)
                            return true;                        // some expected positive spells have SPELL_ATTR1_NEGATIVE
                        break;
                    case SPELL_AURA_ADD_TARGET_TRIGGER:
                        return true;
                    case SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE:
                    case SPELL_AURA_PERIODIC_TRIGGER_SPELL_FROM_CLIENT:
                    case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
                        if (!deep)
                        {
                            if (SpellInfo const* spellTriggeredProto = sSpellMgr->GetSpellInfo(Effects[effIndex].TriggerSpell))
                            {
                                // negative targets of main spell return early
                                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                                {
                                    if (!spellTriggeredProto->Effects[i].Effect)
                                        continue;
                                    // if non-positive trigger cast targeted to positive target this main cast is non-positive
                                    // this will place this spell auras as debuffs
                                    if (_IsPositiveTarget(spellTriggeredProto->Effects[i].TargetA.GetTarget(), spellTriggeredProto->Effects[i].TargetB.GetTarget()) && !spellTriggeredProto->_IsPositiveEffect(i, true))
                                        return false;
                                }
                            }
                        }
                    case SPELL_AURA_PROC_TRIGGER_SPELL:
                        // many positive auras have negative triggered spells at damage for example and this not make it negative (it can be canceled for example)
                        break;
                    case SPELL_AURA_MOD_STUN:                   //have positive and negative spells, we can't sort its correctly at this moment.
                        if (effIndex == 0 && Effects[1].Effect == 0 && Effects[2].Effect == 0)
                            return false;                       // but all single stun aura spells is negative
                        break;
                    case SPELL_AURA_MOD_PACIFY_SILENCE:
                        if (Id == 24740)             // Wisp Costume
                            return true;
                        return false;
                    case SPELL_AURA_MOD_ROOT:
                    case SPELL_AURA_MOD_FEAR:
                    case SPELL_AURA_MOD_SILENCE:
                    case SPELL_AURA_GHOST:
                    case SPELL_AURA_PERIODIC_LEECH:
                    case SPELL_AURA_MOD_STALKED:
                    case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                    case SPELL_AURA_PREVENT_RESURRECTION:
                        return false;
                    case SPELL_AURA_PERIODIC_DAMAGE:            // used in positive spells also.
                        // part of negative spell if casted at self (prevent cancel)
                        if (Effects[effIndex].TargetA.GetTarget() == TARGET_UNIT_CASTER)
                            return false;
                        break;
                    case SPELL_AURA_MOD_DECREASE_SPEED:         // used in positive spells also
                        // part of positive spell if casted at self
                        if (Effects[effIndex].TargetA.GetTarget() != TARGET_UNIT_CASTER)
                            return false;
                        // but not this if this first effect (didn't find better check)
                        if (HasAttribute(SPELL_ATTR0_AURA_IS_DEBUFF) && effIndex == 0)
                            return false;
                        break;
                    case SPELL_AURA_MECHANIC_IMMUNITY:
                        {
                            // non-positive immunities
                            switch (Effects[effIndex].MiscValue)
                            {
                                case MECHANIC_BANDAGE:
                                case MECHANIC_SHIELD:
                                case MECHANIC_MOUNT:
                                case MECHANIC_INVULNERABILITY:
                                    return false;
                                default:
                                    break;
                            }
                            break;
                        }
                    case SPELL_AURA_ADD_FLAT_MODIFIER:          // mods
                    case SPELL_AURA_ADD_PCT_MODIFIER:
                        {
                            // non-positive mods
                            switch (Effects[effIndex].MiscValue)
                            {
                                case SPELLMOD_COST:                 // dependent from bas point sign (negative -> positive)
                                    if (Effects[effIndex].CalcValue() > 0)
                                    {
                                        if (!deep)
                                        {
                                            bool negative = true;
                                            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                                            {
                                                if (i != effIndex)
                                                    if (_IsPositiveEffect(i, true))
                                                    {
                                                        negative = false;
                                                        break;
                                                    }
                                            }
                                            if (negative)
                                                return false;
                                        }
                                    }
                                    break;
                                default:
                                    break;
                            }
                            break;
                        }
                    default:
                        break;
                }
                break;
            }
        default:
            break;
    }

    // non-positive targets
    if (!_IsPositiveTarget(Effects[effIndex].TargetA.GetTarget(), Effects[effIndex].TargetB.GetTarget()))
        return false;

    // negative spell if triggered spell is negative
    if (!deep && !Effects[effIndex].ApplyAuraName && Effects[effIndex].TriggerSpell)
    {
        if (SpellInfo const* spellTriggeredProto = sSpellMgr->GetSpellInfo(Effects[effIndex].TriggerSpell))
            if (!spellTriggeredProto->_IsPositiveSpell())
                return false;
    }

    // ok, positive
    return true;
}

bool SpellInfo::_IsPositiveSpell() const
{
    // spells with at least one negative effect are considered negative
    // some self-applied spells have negative effects but in self casting case negative check ignored.
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (!_IsPositiveEffect(i, true))
            return false;
    return true;
}

bool SpellInfo::_IsPositiveTarget(uint32 targetA, uint32 targetB)
{
    // non-positive targets
    switch (targetA)
    {
        case TARGET_UNIT_NEARBY_ENEMY:
        case TARGET_UNIT_TARGET_ENEMY:
        case TARGET_UNIT_SRC_AREA_ENEMY:
        case TARGET_UNIT_DEST_AREA_ENEMY:
        case TARGET_UNIT_CONE_ENEMY_24:
        case TARGET_UNIT_CONE_ENEMY_54:
        case TARGET_UNIT_CONE_ENEMY_104:
        case TARGET_DEST_DYNOBJ_ENEMY:
        case TARGET_DEST_TARGET_ENEMY:
            return false;
        default:
            break;
    }
    if (targetB)
        return _IsPositiveTarget(targetB, 0);
    return true;
}

void SpellInfo::_UnloadImplicitTargetConditionLists()
{
    // find the same instances of ConditionList and delete them.
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        ConditionList* cur = Effects[i].ImplicitTargetConditions;
        if (!cur)
            continue;
        for (uint8 j = i; j < MAX_SPELL_EFFECTS; ++j)
        {
            if (Effects[j].ImplicitTargetConditions == cur)
                Effects[j].ImplicitTargetConditions = nullptr;
        }
        delete cur;
    }
}

bool SpellInfo::CheckElixirStacking(Unit const* caster) const
{
    if (!caster)
    {
        return true;
    }

    // xinef: check spell group
    uint32 groupId = sSpellMgr->GetSpellGroup(Id);
    if (groupId != SPELL_GROUP_GUARDIAN_AND_BATTLE_ELIXIRS)
    {
        return true;
    }

    SpellGroupSpecialFlags sFlag = sSpellMgr->GetSpellGroupSpecialFlags(Id);
    for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!Effects[i].IsAura())
        {
            continue;
        }

        Unit::AuraApplicationMap const& Auras = caster->GetAppliedAuras();
        for (Unit::AuraApplicationMap::const_iterator itr = Auras.begin(); itr != Auras.end(); ++itr)
        {
            // xinef: aura is not groupped or in different group
            uint32 auraGroup = sSpellMgr->GetSpellGroup(itr->first);
            if (auraGroup != groupId)
            {
                continue;
            }

            // Cannot apply guardian/battle elixir if flask is present
            if (sFlag == SPELL_GROUP_SPECIAL_FLAG_ELIXIR_BATTLE || sFlag == SPELL_GROUP_SPECIAL_FLAG_ELIXIR_GUARDIAN)
            {
                SpellGroupSpecialFlags sAuraFlag = sSpellMgr->GetSpellGroupSpecialFlags(itr->first);
                if ((sAuraFlag & SPELL_GROUP_SPECIAL_FLAG_FLASK) == SPELL_GROUP_SPECIAL_FLAG_FLASK)
                {
                    return false;
                }
            }
        }
    }

    return true;
}
