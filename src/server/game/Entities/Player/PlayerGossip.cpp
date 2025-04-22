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

#include "BattlegroundMgr.h"
#include "Chat.h"
#include "GossipDef.h"
#include "Language.h"
#include "ObjectMgr.h"
#include "OutdoorPvPMgr.h"
#include "Pet.h"
#include "Player.h"
#include "WorldSession.h"
#include "Chat.h"

/*********************************************************/
/***                    GOSSIP SYSTEM                  ***/
/*********************************************************/

void Player::PrepareGossipMenu(WorldObject* source, uint32 menuId /*= 0*/, bool showQuests /*= false*/)
{
    PlayerMenu* menu = PlayerTalkClass;
    menu->ClearMenus();

    menu->GetGossipMenu().SetMenuId(menuId);

    GossipMenuItemsMapBounds menuItemBounds = sObjectMgr->GetGossipMenuItemsMapBounds(menuId);

    // if default menuId and no menu options exist for this, use options from default options
    if (menuItemBounds.first == menuItemBounds.second && menuId == GetDefaultGossipMenuForSource(source))
        menuItemBounds = sObjectMgr->GetGossipMenuItemsMapBounds(0);

    uint32 npcflags = 0;

    if (source->IsCreature())
    {
        npcflags = source->ToUnit()->GetNpcFlags();
        if (showQuests && npcflags & UNIT_NPC_FLAG_QUESTGIVER)
            PrepareQuestMenu(source->GetGUID());
    }
    else if (source->IsGameObject())
        if (showQuests && source->ToGameObject()->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
            PrepareQuestMenu(source->GetGUID());

    for (GossipMenuItemsContainer::const_iterator itr = menuItemBounds.first; itr != menuItemBounds.second; ++itr)
    {
        bool canTalk = true;
        if (!sConditionMgr->IsObjectMeetToConditions(this, source, itr->second.Conditions))
            continue;

        if (Creature* creature = source->ToCreature())
        {
            if (!(itr->second.OptionNpcFlag & npcflags))
                continue;

            switch (itr->second.OptionType)
            {
                case GOSSIP_OPTION_ARMORER:
                    canTalk = false;                       // added in special mode
                    break;
                case GOSSIP_OPTION_SPIRITHEALER:
                    if (!isDead())
                        canTalk = false;
                    break;
                case GOSSIP_OPTION_VENDOR:
                {
                    if (!creature->isVendorWithIconSpeak())
                    {
                        VendorItemData const* vendorItems = itr->second.ActionMenuID ? sObjectMgr->GetNpcVendorItemList(itr->second.ActionMenuID) : creature->GetVendorItems();
                        if (!vendorItems || vendorItems->Empty())
                        {
                            LOG_ERROR("sql.sql", "Creature {} have UNIT_NPC_FLAG_VENDOR but have empty trading item list.", creature->GetGUID().ToString());
                            canTalk = false;
                        }
                        break;
                    }
                    break;
                }
                case GOSSIP_OPTION_LEARNDUALSPEC:
                case GOSSIP_OPTION_DUALSPEC_INFO:
                    if (!(GetSpecsCount() == 1 && creature->isCanTrainingAndResetTalentsOf(this) && !(GetLevel() < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))))
                        canTalk = false;
                    break;
                case GOSSIP_OPTION_UNLEARNTALENTS:
                    if (!creature->isCanTrainingAndResetTalentsOf(this))
                        canTalk = false;
                    break;
                case GOSSIP_OPTION_UNLEARNPETTALENTS:
                    if (!GetPet() || GetPet()->getPetType() != HUNTER_PET || GetPet()->m_spells.size() <= 1 || creature->GetCreatureTemplate()->trainer_type != TRAINER_TYPE_PETS || creature->GetCreatureTemplate()->trainer_class != CLASS_HUNTER)
                        canTalk = false;
                    break;
                case GOSSIP_OPTION_TAXIVENDOR:
                    if (GetSession()->SendLearnNewTaxiNode(creature))
                        return;
                    break;
                case GOSSIP_OPTION_BATTLEFIELD:
                    if (!creature->isCanInteractWithBattleMaster(this, false))
                        canTalk = false;
                    break;
                case GOSSIP_OPTION_STABLEPET:
                    if (!IsClass(CLASS_HUNTER, CLASS_CONTEXT_PET))
                        canTalk = false;
                    break;
                case GOSSIP_OPTION_QUESTGIVER:
                    canTalk = false;
                    break;
                case GOSSIP_OPTION_TRAINER:
                    if (!creature->IsValidTrainerForPlayer(this))
                    {
                        canTalk = false;
                    }
                    break;
                case GOSSIP_OPTION_GOSSIP:
                    if (creature->isVendorWithIconSpeak())
                    {
                        VendorItemData const* vendorItems = creature->GetVendorItems();
                        if (!vendorItems || vendorItems->Empty())
                        {
                            canTalk = false;
                        }
                    }
                    break;
                case GOSSIP_OPTION_SPIRITGUIDE:
                case GOSSIP_OPTION_INNKEEPER:
                case GOSSIP_OPTION_BANKER:
                case GOSSIP_OPTION_PETITIONER:
                case GOSSIP_OPTION_TABARDDESIGNER:
                case GOSSIP_OPTION_AUCTIONEER:
                    break;                                  // no checks
                case GOSSIP_OPTION_OUTDOORPVP:
                    if (!sOutdoorPvPMgr->CanTalkTo(this, creature, itr->second))
                        canTalk = false;
                    break;
                default:
                    LOG_ERROR("sql.sql", "Creature entry {} has unknown OptionType {} for menu {}", creature->GetEntry(), itr->second.OptionType, itr->second.MenuID);
                    canTalk = false;
                    break;
            }
        }
        else if (GameObject* go = source->ToGameObject())
        {
            switch (itr->second.OptionType)
            {
                case GOSSIP_OPTION_GOSSIP:
                    if (go->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER && go->GetGoType() != GAMEOBJECT_TYPE_GOOBER)
                        canTalk = false;
                    break;
                default:
                    canTalk = false;
                    break;
            }
        }

        if (canTalk)
        {
            // using gossip_menu_option texts by default
            std::string strOptionText = itr->second.OptionText;
            std::string strBoxText = itr->second.BoxText;
            // search in broadcast_text and broadcast_text_locale
            BroadcastText const* optionBroadcastText = sObjectMgr->GetBroadcastText(itr->second.OptionBroadcastTextID);
            BroadcastText const* boxBroadcastText = sObjectMgr->GetBroadcastText(itr->second.BoxBroadcastTextID);
            LocaleConstant locale = GetSession()->GetSessionDbLocaleIndex();

            if (optionBroadcastText)
                ObjectMgr::GetLocaleString(getGender() == GENDER_MALE ? optionBroadcastText->MaleText : optionBroadcastText->FemaleText, locale, strOptionText);

            if (boxBroadcastText)
                ObjectMgr::GetLocaleString(getGender() == GENDER_MALE ? boxBroadcastText->MaleText : boxBroadcastText->FemaleText, locale, strBoxText);

            // if the language is not default and the texts weren't found, maybe they're in gossip_menu_option_locale table
            if (locale != DEFAULT_LOCALE)
            {
                if (!optionBroadcastText)
                {
                    /// Find localizations from database.
                    if (GossipMenuItemsLocale const* gossipMenuLocale = sObjectMgr->GetGossipMenuItemsLocale(MAKE_PAIR32(menuId, itr->second.OptionID)))
                        ObjectMgr::GetLocaleString(gossipMenuLocale->OptionText, locale, strOptionText);
                }

                if (!boxBroadcastText)
                {
                    /// Find localizations from database.
                    if (GossipMenuItemsLocale const* gossipMenuLocale = sObjectMgr->GetGossipMenuItemsLocale(MAKE_PAIR32(menuId, itr->second.OptionID)))
                        ObjectMgr::GetLocaleString(gossipMenuLocale->BoxText, locale, strBoxText);
                }
            }

            menu->GetGossipMenu().AddMenuItem(itr->second.OptionID, itr->second.OptionIcon, strOptionText, 0, itr->second.OptionType, strBoxText, itr->second.BoxMoney, itr->second.BoxCoded, itr->second.commandtext);
            menu->GetGossipMenu().AddGossipMenuItemData(itr->second.OptionID, itr->second.ActionMenuID, itr->second.ActionPoiID);
        }
    }

    if (sWorld->getIntConfig(CONFIG_INSTANT_TAXI) == 2 && npcflags & UNIT_NPC_FLAG_FLIGHTMASTER)
        menu->GetGossipMenu().AddMenuItem(-1, GOSSIP_ICON_INTERACT_1, GetSession()->GetAcoreString(LANG_TOGGLE_INSTANT_FLIGHT), 0, GOSSIP_ACTION_TOGGLE_INSTANT_FLIGHT, "", 0, false); // instant flight toggle option
}

void Player::SendPreparedGossip(WorldObject* source)
{
    if (!source)
        return;

    if (source->IsCreature())
    {
        // in case no gossip flag and quest menu not empty, open quest menu (client expect gossip menu with this flag)
        if (!source->ToCreature()->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP) && !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(source->GetGUID());
            return;
        }
    }
    else if (source->IsGameObject())
    {
        // probably need to find a better way here
        if (!PlayerTalkClass->GetGossipMenu().GetMenuId() && !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(source->GetGUID());
            return;
        }
    }

    // in case non empty gossip menu (that not included quests list size) show it
    // (quest entries from quest menu will be included in list)

    uint32 textId = GetGossipTextId(source);

    if (uint32 menuId = PlayerTalkClass->GetGossipMenu().GetMenuId())
        textId = GetGossipTextId(menuId, source);

    PlayerTalkClass->SendGossipMenu(textId, source->GetGUID());
}


void Player::SendTranerListTMP(uint32 fromlowguid, ObjectGuid toguid)
{
    Map* map = GetMap();
    ObjectGuid newguid;

    if (CreatureData const* data = sObjectMgr->GetCreatureData(uint32(fromlowguid)))
    {
        ObjectGuid trainerguid = map->GetCreatureGUIDFromSpawnId(fromlowguid, map);
        Creature* unit = map->GetCreature(trainerguid);
        if (!unit)
        {
            //LOG_ERROR("xx", "WORLD: notfindunit");
            Creature* pCreature = new Creature();
            sObjectMgr->AddCreatureToGrid(fromlowguid, data);
            pCreature->LoadCreatureFromDB(fromlowguid, map, true, false);
            newguid = pCreature->GetGUID();
        }
        else
            newguid = trainerguid;

        //LOG_ERROR("xx", "toguid {}", toguid.GetCounter());

        GetSession()->SetCurrentTranerGuid(newguid);
        PlayerTalkClass->ClearMenus();
        GetSession()->SendTrainerList(newguid, toguid);

    }
}

void Player::OnGossipSelect(WorldObject* source, uint32 gossipListId, uint32 menuId)
{
    GossipMenu& gossipMenu = PlayerTalkClass->GetGossipMenu();

    // if not same, then something funky is going on
    if (menuId != gossipMenu.GetMenuId())
        return;

    GossipMenuItem const* item = gossipMenu.GetItem(gossipListId);
    if (!item)
        return;

    uint32 gossipOptionId = item->OptionType;
    ObjectGuid guid = source->GetGUID();

    if (sWorld->getIntConfig(CONFIG_INSTANT_TAXI) == 2 && source->IsCreature())
    {
        if (gossipOptionId == GOSSIP_ACTION_TOGGLE_INSTANT_FLIGHT && source->ToUnit()->GetNpcFlags() & UNIT_NPC_FLAG_FLIGHTMASTER)
        {
            ToggleInstantFlight();

            if (m_isInstantFlightOn)
                ChatHandler(GetSession()).SendNotification(LANG_INSTANT_FLIGHT_ON);
            else
                ChatHandler(GetSession()).SendNotification(LANG_INSTANT_FLIGHT_OFF);

            PlayerTalkClass->SendCloseGossip();
            return;
        }
    }

    if (source->IsGameObject())
    {
        if (gossipOptionId > GOSSIP_OPTION_QUESTGIVER)
        {
            LOG_ERROR("entities.player", "Player guid {} request invalid gossip option for GameObject entry {}", GetGUID().ToString(), source->GetEntry());
            return;
        }
    }

    GossipMenuItemData const* menuItemData = gossipMenu.GetItemData(gossipListId);
    if (!menuItemData)
        return;

    int32 cost = int32(item->BoxMoney);
    if (!HasEnoughMoney(cost))
    {
        SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
        PlayerTalkClass->SendCloseGossip();
        return;
    }

    switch (gossipOptionId)
    {
        case GOSSIP_OPTION_GOSSIP:
        case GOSSIP_OPTION_DUALSPEC_INFO:
        {

            //如果OptionType=1，尝试能否拿到commandtext
            if (item->m_commandtext.size() > 0)
            {
                //LOG_ERROR("xx", "xx2xx m_commandtext: {} ", item->m_commandtext);//测试

                //判断玩家不在战斗中，才可以执行命令
                if (!IsInCombat())
                    ChatHandler(GetSession()).ParseCommands(item->m_commandtext.c_str(), true);
                else
                {
                    ChatHandler(GetSession()).SendNotification(21715);//屏幕中间的提醒
                }

                break;
            }

            if (menuItemData->GossipActionPoi)
                PlayerTalkClass->SendPointOfInterest(menuItemData->GossipActionPoi);

            if (menuItemData->GossipActionMenuId)
            {
                PrepareGossipMenu(source, menuItemData->GossipActionMenuId);
                SendPreparedGossip(source);
            }

            break;
        }
        case GOSSIP_OPTION_OUTDOORPVP:
            sOutdoorPvPMgr->HandleGossipOption(this, source->ToCreature(), gossipListId);
            break;
        case GOSSIP_OPTION_SPIRITHEALER:
            if (isDead())
                source->ToCreature()->CastSpell(source->ToCreature(), 17251, true, nullptr, nullptr, GetGUID());
            break;
        case GOSSIP_OPTION_QUESTGIVER:
            PrepareQuestMenu(guid);
            SendPreparedQuest(guid);
            break;
        case GOSSIP_OPTION_VENDOR:
        case GOSSIP_OPTION_ARMORER:
            GetSession()->SendListInventory(guid, menuItemData->GossipActionMenuId);
            break;
        case GOSSIP_OPTION_STABLEPET:
            GetSession()->SendStablePet(guid);
            break;
        case GOSSIP_OPTION_TRAINER:
            //LOG_ERROR("xx", "GOSSIP_OPTION_TRAINER {}", GOSSIP_OPTION_TRAINER);//测试
            if (menuItemData->GossipActionMenuId == 99999)//如果是技能训练师
            {
                //LOG_ERROR("xx", "GossipActionMenuId {}", menuItemData->GossipActionMenuId);//测试
                //判断玩家职业
                if (this->GetClass())
                {
                    //LOG_ERROR("xx", "GetClass {}", this->GetClass());//测试
                    uint32 playerrace = 0;
                    if (Player::TeamIdForRace(this->GetRace()) == TEAM_ALLIANCE)
                        playerrace = 0;//联盟
                    else if (Player::TeamIdForRace(this->GetRace()) == TEAM_HORDE)
                        playerrace = 1;//部落

                    switch (this->GetClass())
                    {
                    case CLASS_PALADIN:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(32066, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(6503, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域，银月城
                            {
                                SendTranerListTMP(57671, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(32066, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(32066, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(37586, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(137653, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域，银月城
                            {
                                SendTranerListTMP(57748, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(37586, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(37586, guid);
                        }
                        break;
                        break;
                    case CLASS_SHAMAN:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(34147, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(4663, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(3548, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(34147, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(34147, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(79860, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(61721, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(63013, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(61721, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(61721, guid);
                        }
                        break;
                    case CLASS_HUNTER:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(364, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(7449, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(364, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(364, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(364, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(37609, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(46221, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(59521, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(37609, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(37609, guid);
                        }
                        break;
                    case CLASS_MAGE:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(38422, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(3474, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(57646, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(38422, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(38422, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(90463, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(47640, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(57742, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(90463, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(90463, guid);
                        }
                        break;
                    case CLASS_PRIEST:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(41835, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(3472, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(41835, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(41835, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(41835, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(1079, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(49903, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(1079, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(1079, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(1079, guid);
                        }
                        break;
                    case CLASS_WARLOCK:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(32091, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(3461, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(57641, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(32091, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(32091, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(90461, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(1000000, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(90461, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(90461, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(90461, guid);
                        }
                        break;
                    case CLASS_WARRIOR:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(31897, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(26768, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(1000001, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(31897, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(31897, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(79779, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(49851, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(85589, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(79779, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(79779, guid);
                        }
                        break;
                    case CLASS_ROGUE:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(31885, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(6593, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(57673, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(31885, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(31885, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(79787, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(46469, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(79787, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(79787, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(79787, guid);
                        }
                        break;
                    case CLASS_DRUID:
                        if (playerrace == 1)
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(1000002, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(42415, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(57648, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(42415, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(42415, guid);
                        }
                        else
                        {
                            if (GetMapId() == 0)//如果是东部王国
                            {
                                SendTranerListTMP(90466, guid);
                            }
                            else if (GetMapId() == 1)//卡里姆多
                            {
                                SendTranerListTMP(42415, guid);
                            }
                            else if (GetMapId() == 530)//秘蓝岛，外域
                            {
                                SendTranerListTMP(57708, guid);
                            }
                            else if (GetMapId() == 571)//诺森德
                            {
                                SendTranerListTMP(90466, guid);
                            }
                            else//其他地图如副本、战场中
                                SendTranerListTMP(90466, guid);
                        }
                        break;
                    case CLASS_DEATH_KNIGHT:

                        if (GetMapId() == 0)//如果是东部王国
                        {
                            SendTranerListTMP(125641, guid);
                        }
                        else if (GetMapId() == 1)//卡里姆多
                        {
                            SendTranerListTMP(125641, guid);
                        }
                        else if (GetMapId() == 530)//秘蓝岛，外域
                        {
                            SendTranerListTMP(125641, guid);
                        }
                        else if (GetMapId() == 571)//诺森德
                        {
                            SendTranerListTMP(125641, guid);
                        }
                        else//其他地图如副本、战场中
                            SendTranerListTMP(125641, guid);

                        break;
                    }
                }
            }
            else
                GetSession()->SendTrainerList(guid);
            break;
        case GOSSIP_OPTION_LEARNDUALSPEC:
            if (GetSpecsCount() == 1 && GetLevel() >= sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))
            {
                // Cast spells that teach dual spec
                // Both are also ImplicitTarget self and must be cast by player
                CastSpell(this, 63680, true, nullptr, nullptr, GetGUID());
                CastSpell(this, 63624, true, nullptr, nullptr, GetGUID());

                PrepareGossipMenu(source, menuItemData->GossipActionMenuId);
                SendPreparedGossip(source);
            }
            break;
        case GOSSIP_OPTION_UNLEARNTALENTS:
            PlayerTalkClass->SendCloseGossip();
            SendTalentWipeConfirm(guid);
            break;
        case GOSSIP_OPTION_UNLEARNPETTALENTS:
            PlayerTalkClass->SendCloseGossip();
            ResetPetTalents();
            break;
        case GOSSIP_OPTION_TAXIVENDOR:
            GetSession()->SendTaxiMenu(source->ToCreature());
            break;
        case GOSSIP_OPTION_INNKEEPER:
            PlayerTalkClass->SendCloseGossip();
            SetBindPoint(guid);
            break;
        case GOSSIP_OPTION_BANKER:
            GetSession()->SendShowBank(guid);
            break;
        case GOSSIP_OPTION_PETITIONER:
            PlayerTalkClass->SendCloseGossip();
            GetSession()->SendPetitionShowList(guid);
            break;
        case GOSSIP_OPTION_TABARDDESIGNER:
            PlayerTalkClass->SendCloseGossip();
            GetSession()->SendTabardVendorActivate(guid);
            break;
        case GOSSIP_OPTION_AUCTIONEER:
            GetSession()->SendAuctionHello(guid, source->ToCreature());
            break;
        case GOSSIP_OPTION_SPIRITGUIDE:
            PrepareGossipMenu(source);
            SendPreparedGossip(source);
            break;
        case GOSSIP_OPTION_BATTLEFIELD:
        {
            BattlegroundTypeId bgTypeId = sBattlegroundMgr->GetBattleMasterBG(source->GetEntry());

            if (bgTypeId == BATTLEGROUND_TYPE_NONE)
            {
                LOG_ERROR("entities.player", "A user ({}) requested battlegroundlist from a npc who is no battlemaster", GetGUID().ToString());
                return;
            }

            GetSession()->SendBattleGroundList(guid, bgTypeId);
            break;
        }
    }

    ModifyMoney(-cost);
}

uint32 Player::GetGossipTextId(WorldObject* source)
{
    if (!source)
        return DEFAULT_GOSSIP_MESSAGE;

    return GetGossipTextId(GetDefaultGossipMenuForSource(source), source);
}

uint32 Player::GetGossipTextId(uint32 menuId, WorldObject* source)
{
    uint32 textId = DEFAULT_GOSSIP_MESSAGE;

    if (!menuId)
        return textId;

    GossipMenusMapBounds menuBounds = sObjectMgr->GetGossipMenusMapBounds(menuId);

    for (GossipMenusContainer::const_iterator itr = menuBounds.first; itr != menuBounds.second; ++itr)
    {
        if (sConditionMgr->IsObjectMeetToConditions(this, source, itr->second.Conditions))
            textId = itr->second.TextID;
    }

    return textId;
}

uint32 Player::GetDefaultGossipMenuForSource(WorldObject* source)
{
    switch (source->GetTypeId())
    {
        case TYPEID_UNIT:
            return source->ToCreature()->GetCreatureTemplate()->GossipMenuId;
        case TYPEID_GAMEOBJECT:
            return source->ToGameObject()->GetGOInfo()->GetGossipMenuId();
        default:
            break;
    }

    return 0;
}

void Player::ToggleInstantFlight()
{
    m_isInstantFlightOn = !m_isInstantFlightOn;
}
