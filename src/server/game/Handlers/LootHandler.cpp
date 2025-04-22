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

#include "Corpse.h"
#include "Creature.h"
#include "GameObject.h"
#include "Group.h"
#include "LootItemStorage.h"
#include "LootMgr.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Chat.h"
#include "WorldSessionMgr.h"

//npcbot
#include "botmgr.h"
//end npcbot

void WorldSession::HandleAutostoreLootItemOpcode(WorldPacket& recvData)
{
    LOG_DEBUG("network", "WORLD: CMSG_AUTOSTORE_LOOT_ITEM");
    Player* player = GetPlayer();
    ObjectGuid lguid = player->GetLootGUID();
    Loot* loot = nullptr;
    uint8 lootSlot = 0;
    bool _frombigbox = false;

    recvData >> lootSlot;

    if (lguid.IsGameObject())
    {
        GameObject* go = player->GetMap()->GetGameObject(lguid);
        // xinef: cheating protection
        //if (player->GetGroup() && player->GetGroup()->GetLootMethod() == MASTER_LOOT && player->GetGUID() != player->GetGroup()->GetMasterLooterGuid())
        //    go = nullptr;

        // not check distance for GO in case owned GO (fishing bobber case, for example) or Fishing hole GO
        if (!go || ((go->GetOwnerGUID() != _player->GetGUID() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && !go->IsWithinDistInMap(_player)))
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &go->loot;
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetItemByGuid(lguid);

        if (!pItem)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &pItem->loot;

        //如果是从神秘宝箱或雷霆宝箱loot
        switch (pItem->GetEntry())
        {
        case 70134:
        case 70135:
        case 70136:
        case 90118:
        case 90119:
            _frombigbox = true;
            break;
        }

    }
    else if (lguid.IsCorpse())
    {
        Corpse* bones = ObjectAccessor::GetCorpse(*player, lguid);
        if (!bones)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &bones->loot;
    }
    else
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);

        bool lootAllowed = creature && creature->IsAlive() == (player->IsClass(CLASS_ROGUE, CLASS_CONTEXT_ABILITY) && creature->loot.loot_type == LOOT_PICKPOCKETING);
        if (!lootAllowed || !creature->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
        {
            player->SendLootError(lguid, lootAllowed ? LOOT_ERROR_TOO_FAR : LOOT_ERROR_DIDNT_KILL);
            return;
        }

        loot = &creature->loot;
    }

    sScriptMgr->OnPlayerAfterCreatureLoot(player);

    InventoryResult msg;
    LootItem* lootItem = player->StoreLootItem(lootSlot, loot, msg);
    if (msg != EQUIP_ERR_OK && lguid.IsItem() && loot->loot_type != LOOT_CORPSE)
    {
        lootItem->is_looted = true;
        loot->NotifyItemRemoved(lootItem->itemIndex);
        loot->unlootedCount--;

        player->SendItemRetrievalMail(lootItem->itemid, lootItem->count);
    }

    //如果拾取的是自创物品，且是从自制宝箱loot的，全服通告
    if (lootItem && _frombigbox && msg == EQUIP_ERR_OK)
    {
        if (lootItem->itemid > 69900)
        {
            ItemTemplate const* _itempro = player->GetItemByEntry(lootItem->itemid)->GetTemplate();
            if ((_itempro->cost_item == 70000 && _itempro->cost_item_count > 180) || (_itempro->cost_item == 70008 && _itempro->cost_item_count > 360) || (_itempro->cost_item == 70001 && _itempro->cost_item_count > 720))
            {
                //Get the localised name
                std::string name = _itempro->Name1;
                if (ItemLocale const* il = sObjectMgr->GetItemLocale(_itempro->ItemId))
                {
                    ObjectMgr::GetLocaleString(il->Name, LOCALE_zhCN, name);
                }

                //全服通告
                ChatHandler(nullptr).SendWorldText(21727, player->GetName().c_str(), name.c_str());
                //给所有玩家上buff光环
                for (WorldSessionMgr::SessionMap::const_iterator itr = sWorldSessionMgr->GetAllSessions().begin(); itr != sWorldSessionMgr->GetAllSessions().end(); ++itr)
                {
                    if (!itr->second || !itr->second->GetPlayer() || !itr->second->GetPlayer()->IsInWorld() || player->TeamIdForRace(player->GetRace()) != player->TeamIdForRace(itr->second->GetPlayer()->GetRace()))
                        continue;
                    //上光环
                    itr->second->GetPlayer()->AddAura(22888, itr->second->GetPlayer());
                    itr->second->GetPlayer()->AddAura(24425, itr->second->GetPlayer());
                    //itr->second->GetPlayer()->AddAura(16609, itr->second->GetPlayer());
                    itr->second->GetPlayer()->CastSpell(itr->second->GetPlayer(), 16609, true);//这个用castspell是为了出法术霹雷的效果
                }
            }
        }

    }

    //添加拾取物品log
    if (lootItem && !player->GetSession()->IsBot())
        LootItemAddLog(player, lguid, lootItem->itemid, lootItem->count);


    // If player is removing the last LootItem, delete the empty container.
    if (loot->isLooted() && lguid.IsItem())
        DoLootRelease(lguid);
}

void WorldSession::HandleLootMoneyOpcode(WorldPacket& /*recvData*/)
{
    LOG_DEBUG("network", "WORLD: CMSG_LOOT_MONEY");

    Player* player = GetPlayer();
    ObjectGuid guid = player->GetLootGUID();
    if (!guid)
        return;

    Loot* loot = nullptr;
    bool shareMoney = true;

    switch (guid.GetHigh())
    {
        case HighGuid::GameObject:
            {
                GameObject* go = GetPlayer()->GetMap()->GetGameObject(guid);

                // do not check distance for GO if player is the owner of it (ex. fishing bobber)
                if (go && ((go->GetOwnerGUID() == player->GetGUID() || go->IsWithinDistInMap(player))))
                {
                    loot = &go->loot;
                }

                break;
            }
        case HighGuid::Corpse:                               // remove insignia ONLY in BG
            {
                Corpse* bones = ObjectAccessor::GetCorpse(*player, guid);

                if (bones && bones->IsWithinDistInMap(player, INTERACTION_DISTANCE))
                {
                    loot = &bones->loot;
                    shareMoney = false;
                }

                break;
            }
        case HighGuid::Item:
            {
                if (Item* item = player->GetItemByGuid(guid))
                {
                    loot = &item->loot;
                    shareMoney = false;
                }
                break;
            }
        case HighGuid::Unit:
        case HighGuid::Vehicle:
            {
                Creature* creature = player->GetMap()->GetCreature(guid);
                bool lootAllowed = creature && creature->IsAlive() == (player->IsClass(CLASS_ROGUE, CLASS_CONTEXT_ABILITY) && creature->loot.loot_type == LOOT_PICKPOCKETING);
                if (lootAllowed && creature->IsWithinDistInMap(player, INTERACTION_DISTANCE))
                {
                    loot = &creature->loot;
                    if (creature->IsAlive())
                        shareMoney = false;
                }
                else
                    player->SendLootError(guid, lootAllowed ? LOOT_ERROR_TOO_FAR : LOOT_ERROR_DIDNT_KILL);
                break;
            }
        default:
            return;                                         // unlootable type
    }

    if (loot)
    {
        sScriptMgr->OnPlayerBeforeLootMoney(player, loot);
        loot->NotifyMoneyRemoved();
        //npcbot
        if (shareMoney && player->GetGroup() && BotMgr::GetNpcBotMoneyShareEnabled())
        {
            Group* group = player->GetGroup();
            std::vector<Player*> playersNear;
            uint32 bots_count = 0;
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member)
                    continue;

                if (player->IsAtGroupRewardDistance(member))
                    playersNear.push_back(member);

                if (!member->HaveBot())
                    continue;

                BotMap const* botMap = member->GetBotMgr()->GetBotMap();
                for (auto const& kv : *botMap)
                {
                    Creature const* bot = kv.second;
                    if (bot && bot->IsAlive() && bot->IsInMap(player) && (group->IsMember(kv.first) || !BotMgr::GetNpcBotMoneyShareGroupOnly()) &&
                        (member->GetMap()->IsDungeon() || player->GetDistance(bot) <= sWorld->getFloatConfig(CONFIG_GROUP_XP_DISTANCE)))
                        ++bots_count;
                }
            }

            uint32 sharers_count = uint32(playersNear.size()) + bots_count;
            uint32 goldPerPlayer = uint32(loot->gold / sharers_count);

            for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
            {
                (*i)->ModifyMoney(goldPerPlayer);
                (*i)->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, goldPerPlayer);

                WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
                data << uint32(goldPerPlayer);
                data << uint8(sharers_count <= 1); // Controls the text displayed in chat. 0 is "Your share is..." and 1 is "You loot..."
                (*i)->SendDirectMessage(&data);

                if (!(*i)->GetSession()->IsBot())
                    LootMoneyAddLog((*i)->GetSession()->GetPlayer(), guid, uint32(goldPerPlayer));//添加金币拾取log

            }
        }
        else
        //end npcbot
        if (shareMoney && player->GetGroup())      //item, pickpocket and players can be looted only single player
        {
            Group* group = player->GetGroup();

            std::vector<Player*> playersNear;
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member)
                    continue;

                if (player->IsAtLootRewardDistance(member))
                    playersNear.push_back(member);
            }

            uint32 goldPerPlayer = uint32((loot->gold) / (playersNear.size()));

            for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
            {
                (*i)->ModifyMoney(goldPerPlayer);
                (*i)->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, goldPerPlayer);

                WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
                data << uint32(goldPerPlayer);
                data << uint8(playersNear.size() > 1 ? 0 : 1);     // Controls the text displayed in chat. 0 is "Your share is..." and 1 is "You loot..."
                (*i)->GetSession()->SendPacket(&data);

                if (!(*i)->GetSession()->IsBot())
                    LootMoneyAddLog((*i)->GetSession()->GetPlayer(), guid, uint32(goldPerPlayer));//添加金币拾取log
            }
        }
        else
        {
            sScriptMgr->OnPlayerAfterCreatureLootMoney(player);
            player->ModifyMoney(loot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);

            WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
            data << uint32(loot->gold);
            data << uint8(1);   // "You loot..."
            SendPacket(&data);

            //为了优化性能不加金币拾取日志了
            //if (!player->GetSession()->IsBot())
            //LootMoneyAddLog(player, guid, uint32(loot->gold));//添加金币拾取log


        }

        sScriptMgr->OnLootMoney(player, loot->gold);

        loot->gold = 0;

        // Delete the money loot record from the DB
        if (loot->containerGUID)
            sLootItemStorage->RemoveStoredLootMoney(loot->containerGUID, loot);

        // Delete container if empty
        if (loot->isLooted() && guid.IsItem())
            DoLootRelease(guid);
    }
}


void WorldSession::HandleAutostoreLootItemOpcodePlus(Player* player, ObjectGuid guid, uint8 lootSlot)
{
    LOG_DEBUG("network", "WORLD: CMSG_AUTOSTORE_LOOT_ITEM");
    //LOG_ERROR("xx", "AutostoreLootItem {} lootSlot {}", guid.GetCounter(), lootSlot);//测试
    ObjectGuid lguid = guid;
    Loot* loot = nullptr;

    if (lguid.IsGameObject())
    {
        GameObject* go = player->GetMap()->GetGameObject(lguid);
        // xinef: cheating protection
        //if (player->GetGroup() && player->GetGroup()->GetLootMethod() == MASTER_LOOT && player->GetGUID() != player->GetGroup()->GetMasterLooterGuid())
        //    go = nullptr;

        // not check distance for GO in case owned GO (fishing bobber case, for example) or Fishing hole GO
        if (!go || ((go->GetOwnerGUID() != _player->GetGUID() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && !go->IsWithinDistInMap(_player)))
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &go->loot;
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetItemByGuid(lguid);

        if (!pItem)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &pItem->loot;
    }
    else if (lguid.IsCorpse())
    {
        Corpse* bones = ObjectAccessor::GetCorpse(*player, lguid);
        if (!bones)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &bones->loot;
    }
    else
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);
        if (!creature)
            return;

        bool lootAllowed = creature && creature->IsAlive() == (player->IsClass(CLASS_ROGUE, CLASS_CONTEXT_ABILITY) && creature->loot.loot_type == LOOT_PICKPOCKETING);
        //if (!lootAllowed || !creature->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
        if (!lootAllowed)
        {
            //LOG_ERROR("xx", "not lootAllowed ");//测试
            player->SendLootError(lguid, lootAllowed ? LOOT_ERROR_TOO_FAR : LOOT_ERROR_DIDNT_KILL);
            return;
        }

        loot = &creature->loot;
    }

    sScriptMgr->OnPlayerAfterCreatureLoot(player);

    InventoryResult msg;
    LootItem* lootItem = player->StoreLootItem(lootSlot, loot, msg);
    if (msg != EQUIP_ERR_OK && lguid.IsItem() && loot->loot_type != LOOT_CORPSE)
    {
        //LOG_ERROR("xx", "EQUIP_ERR_OK ");//测试
        lootItem->is_looted = true;
        loot->NotifyItemRemoved(lootItem->itemIndex);
        loot->unlootedCount--;

        player->SendItemRetrievalMail(lootItem->itemid, lootItem->count);
    }

    //一键拾取时不写入日志了，防止服务器卡顿
    //添加拾取物品log
    //if(lootItem)
    //LootItemAddLog(player, lguid, lootItem->itemid, lootItem->count);

    // If player is removing the last LootItem, delete the empty container.
    if (loot->isLooted() && lguid.IsItem())
        DoLootRelease(lguid);
}
void WorldSession::LootMoneyAddLog(Player* player, ObjectGuid guid, uint32 money)
{
    if (!player || !guid || !money)
        return;

    if (guid.IsItem())
        LOG_INFO("loot", " loot money:{} - {}G looter: {}[{}] [loot from item]", money, money / 10000, player->GetName(), player->GetGUID().GetCounter());
    else if (guid.IsGameObject())
        LOG_INFO("loot", " loot money:{} - {}G looter: {}[{}] [loot from object:{}]", money, money / 10000, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
    else if (guid.IsCreature())
        LOG_INFO("loot", " loot money:{} - {}G looter: {}[{}] [loot from creature:{}]", money, money / 10000, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
    else
        LOG_INFO("loot", " loot money:{} - {}G looter: {}[{}] [loot from {}]", money, money / 10000, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
}

void WorldSession::LootItemAddLog(Player* player, ObjectGuid guid, uint32 itemid, uint32 itemcount, Player* Leader)
{
    if (!player || !guid || !itemid)
        return;
    if (!itemcount)
        itemcount = 1;

    if (Leader)
    {
        if (guid.IsItem())
            LOG_INFO("loot", " loot  Leader:{}[{}] gives {}x{} to {}[{}] [loot from item]", Leader->GetName(), Leader->GetGUID().GetCounter(), itemid, itemcount, player->GetName(), player->GetGUID().GetCounter());
        else if (guid.IsGameObject())
            LOG_INFO("loot", " loot  Leader:{}[{}] gives {}x{} to {}[{}] [loot from object:{}]", Leader->GetName(), Leader->GetGUID().GetCounter(), itemid, itemcount, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
        else if (guid.IsCreature())
            LOG_INFO("loot", " loot  Leader:{}[{}] gives {}x{} to {}[{}] [loot from creature:{}]", Leader->GetName(), Leader->GetGUID().GetCounter(), itemid, itemcount, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
        else
            LOG_INFO("loot", " loot  Leader:{}[{}] gives {}x{} to {}[{}] [loot from {}]", Leader->GetName(), Leader->GetGUID().GetCounter(), itemid, itemcount, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
    }
    else
    {
        if (guid.IsItem())
            LOG_INFO("loot", " loot {}x{} looter: {}[{}] [loot from item]", itemid, itemcount, player->GetName(), player->GetGUID().GetCounter());
        else if (guid.IsGameObject())
            LOG_INFO("loot", " loot {}x{} looter: {}[{}] [loot from object:{}]", itemid, itemcount, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
        else if (guid.IsCreature())
            LOG_INFO("loot", " loot {}x{} looter: {}[{}] [loot from creature:{}]", itemid, itemcount, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());
        else
            LOG_INFO("loot", " loot {}x{} looter: {}[{}] [loot from {}]", itemid, itemcount, player->GetName(), player->GetGUID().GetCounter(), guid.GetEntry());

    }

}
void WorldSession::HandleLootMoneyOpcodePlus(Player* player, ObjectGuid guid)
{
    LOG_DEBUG("network", "WORLD: CMSG_LOOT_MONEY");

    if (!guid)
        return;

    Loot* loot = nullptr;
    bool shareMoney = true;

    switch (guid.GetHigh())
    {
    case HighGuid::GameObject:
    {
        GameObject* go = GetPlayer()->GetMap()->GetGameObject(guid);

        // do not check distance for GO if player is the owner of it (ex. fishing bobber)
        if (go && ((go->GetOwnerGUID() == player->GetGUID() || go->IsWithinDistInMap(player))))
        {
            loot = &go->loot;
        }

        break;
    }
    case HighGuid::Corpse:                               // remove insignia ONLY in BG
    {
        Corpse* bones = ObjectAccessor::GetCorpse(*player, guid);

        if (bones && bones->IsWithinDistInMap(player, INTERACTION_DISTANCE))
        {
            loot = &bones->loot;
            shareMoney = false;
        }

        break;
    }
    case HighGuid::Item:
    {
        if (Item* item = player->GetItemByGuid(guid))
        {
            loot = &item->loot;
            shareMoney = false;
        }
        break;
    }
    case HighGuid::Unit:
    case HighGuid::Vehicle:
    {
        Creature* creature = player->GetMap()->GetCreature(guid);
        bool lootAllowed = creature && creature->IsAlive() == (player->IsClass(CLASS_ROGUE, CLASS_CONTEXT_ABILITY) && creature->loot.loot_type == LOOT_PICKPOCKETING);
        //if (lootAllowed && creature->IsWithinDistInMap(player, INTERACTION_DISTANCE))
        if (lootAllowed)
        {
            loot = &creature->loot;
            if (creature->IsAlive())
                shareMoney = false;
        }
        else
            player->SendLootError(guid, lootAllowed ? LOOT_ERROR_TOO_FAR : LOOT_ERROR_DIDNT_KILL);
        break;
    }
    default:
        return;                                         // unlootable type
    }

    if (loot)
    {
        sScriptMgr->OnPlayerBeforeLootMoney(player, loot);
        loot->NotifyMoneyRemoved();
        if (shareMoney && player->GetGroup())      //item, pickpocket and players can be looted only single player
        {
            Group* group = player->GetGroup();

            std::vector<Player*> playersNear;
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member)
                    continue;

                if (player->IsAtLootRewardDistance(member))
                    playersNear.push_back(member);
            }

            uint32 goldPerPlayer = uint32((loot->gold) / (playersNear.size()));

            for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
            {
                (*i)->ModifyMoney(goldPerPlayer);
                (*i)->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, goldPerPlayer);

                WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
                data << uint32(goldPerPlayer);
                data << uint8(playersNear.size() > 1 ? 0 : 1);     // Controls the text displayed in chat. 0 is "Your share is..." and 1 is "You loot..."
                (*i)->GetSession()->SendPacket(&data);

                //一键拾取时不写入日志了，防止服务器卡顿
                //LootMoneyAddLog((*i)->GetSession()->GetPlayer(), guid, uint32(goldPerPlayer));//添加金币拾取log
            }
        }
        else
        {
            sScriptMgr->OnPlayerAfterCreatureLootMoney(player);
            player->ModifyMoney(loot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);

            WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
            data << uint32(loot->gold);
            data << uint8(1);   // "You loot..."
            SendPacket(&data);

            //一键拾取时不写入日志了，防止服务器卡顿
            //LootMoneyAddLog(player, guid, uint32(loot->gold));//添加金币拾取log
        }

        sScriptMgr->OnLootMoney(player, loot->gold);

        loot->gold = 0;

        // Delete the money loot record from the DB
        if (loot->containerGUID)
            sLootItemStorage->RemoveStoredLootMoney(loot->containerGUID, loot);

        // Delete container if empty
        if (loot->isLooted() && guid.IsItem())
            DoLootRelease(guid);
    }
}

void WorldSession::HandleLootOpcode(WorldPacket& recvData)
{
    LOG_DEBUG("network", "WORLD: CMSG_LOOT");

    ObjectGuid guid;
    recvData >> guid;

    // Check possible cheat
    if (!GetPlayer()->IsAlive() || !guid.IsCreatureOrVehicle())
        return;

    // interrupt cast
    if (GetPlayer()->IsNonMeleeSpellCast(false))
        GetPlayer()->InterruptNonMeleeSpells(false);

    GetPlayer()->SendLoot(guid, LOOT_CORPSE);

    if (guid.IsCreature() && GetPlayer()->GetMap()->GetCreature(guid) && !GetPlayer()->GetSession()->IsBot())//如果不做这个判断，战场捡尸体会崩;如果不加后面的判断，会偶尔崩，错误是：引发了未经处理的异常:读取访问权限冲突。Map::GetObjectA<Creature>(...) 返回 nullptr。
    {

        //判断是否有一键拾取宝典
        uint32 shiqubook = GetPlayer()->GetItemCount(91888, false);

        if (shiqubook && !(GetPlayer()->GetMap()->GetCreature(guid)->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND) && !GetPlayer()->GetMap()->GetCreature(guid)->isWorldBoss())//副本boss不加入自动拾取
        {

            //第二套方案：范围拾取
            std::list<Creature*> NearCreatureCorpos;
            GetPlayer()->GetCreaturesCorpseInRange(NearCreatureCorpos, 50.f);//拾取范围50码
            std::list<Creature*>::const_iterator itr;

            //如果是在队伍中
            // the player whose group may loot the corpse
            Player* recipient = GetPlayer()->GetMap()->GetCreature(guid)->GetLootRecipient();
            if (recipient && NearCreatureCorpos.size() > 0)
            {
                if (Group* group = GetPlayer()->GetGroup())
                {
                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "xx1x");//测试
                    if (group == recipient->GetGroup())
                    {
                        if (group->GetLootMethod() != MASTER_LOOT && group->GetLootMethod() != FREE_FOR_ALL)//不是队长分配也不是自由拾取时
                        {
                            uint16 _limitnum = 0;
                            for (itr = NearCreatureCorpos.begin(); itr != NearCreatureCorpos.end(); ++itr)
                            {
                                if (!(*itr)->GetGUID().IsCreature())//不是creature跳出
                                    continue;

                                if (_limitnum >= 300)//一键拾取限制50个怪
                                {
                                    continue;
                                }

                                if ((*itr)->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND || (*itr)->isWorldBoss())//creature是副本首领跳出
                                {
                                    continue;
                                }


                                Loot const* loot = &(*itr)->loot;

                                if (((*itr)->GetLootRecipientGUID() == GetPlayer()->GetGUID() || (!loot->roundRobinPlayer) || (loot->roundRobinPlayer == _player->GetGUID())) && !loot->isLooted())//如果玩家是当前的拾取者，执行一键拾取
                                {
                                    HandleLootOpcodePlus(_player, (*itr)->GetGUID());
                                    HandleLootMoneyOpcodePlus(_player, (*itr)->GetGUID());

                                    for (uint8 i = 0; i < MAX_NR_LOOT_ITEMS; ++i)
                                        HandleAutostoreLootItemOpcodePlus(_player, (*itr)->GetGUID(), i);

                                    HandleLootReleaseOpcodePlus(_player, (*itr)->GetGUID());
                                    //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "inner %u to player %u roundRobinPlayer %u GetLootRecipientGuid %u", (*itr)->GetGUIDLow(), _player->GetGUIDLow(), loot->roundRobinPlayer, (*itr)->GetLootRecipientGuid());//测试
                                }
                                //else
                                //{
                                    //HandleLootReleaseOpcodePlus(_player, (*itr)->GetObjectGuid());
                                //}

                                //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "add %u to player %u roundRobinPlayer %u GetLootRecipientGuid %u", (*itr)->GetGUIDLow(), _player->GetGUIDLow(), loot->roundRobinPlayer, (*itr)->GetLootRecipientGuid());//测试

                                _limitnum++;
                            }

                            return;
                        }

                        else if (group->GetLootMethod() == FREE_FOR_ALL)//如果是队伍中的自由拾取模式
                        {
                            uint16 _limitnum = 0;
                            for (itr = NearCreatureCorpos.begin(); itr != NearCreatureCorpos.end(); ++itr)
                            {
                                if (!(*itr)->GetGUID().IsCreature())//不是creature跳出
                                    continue;

                                if (_limitnum >= 300)//一键拾取限制50个怪
                                {
                                    continue;
                                }

                                if ((*itr)->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND || (*itr)->isWorldBoss())//creature是副本首领跳出
                                {
                                    continue;
                                }

                                Loot const* loot = &(*itr)->loot;
                                //if ( (*itr)->GetLootRecipientGuid() == GetPlayer()->GetObjectGuid() && !loot->isLooted() && !_player->IsBot())//如果玩家不是playerbot，执行一键拾取
                                if ((*itr)->GetLootRecipientGroupGUID() == GetPlayer()->GetGroup()->GetGUID().GetCounter() && !loot->isLooted() && !_player->GetSession()->IsBot())//如果玩家不是playerbot，执行一键拾取
                                {
                                    HandleLootOpcodePlus(_player, (*itr)->GetGUID());
                                    HandleLootMoneyOpcodePlus(_player, (*itr)->GetGUID());

                                    for (uint8 i = 0; i < MAX_NR_LOOT_ITEMS; ++i)
                                        HandleAutostoreLootItemOpcodePlus(_player, (*itr)->GetGUID(), i);

                                    HandleLootReleaseOpcodePlus(_player, (*itr)->GetGUID());
                                }
                                //sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "add %u to player %u", (*itr)->GetGUIDLow(), _player->GetGUIDLow());//测试
                                _limitnum++;

                            }

                            return;
                        }

                    }

                }
                else//如果没有队伍
                {
                    //LOG_ERROR("xx", "test ");//测试
                    //获取拾取列表数组中的creature
                    uint16 _limitnum = 0;
                    for (itr = NearCreatureCorpos.begin(); itr != NearCreatureCorpos.end(); ++itr)
                    {
                        if (!(*itr)->GetGUID().IsCreature())//不是creature跳出
                            continue;

                        if (_limitnum >= 300)//一键拾取限制50个怪
                        {
                            continue;
                        }

                        //LOG_ERROR("xx", "try creatureguid {} ", (*itr)->GetGUID().GetCounter());//测试

                        if ((*itr)->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND || (*itr)->isWorldBoss())//creature是副本首领跳出
                        {
                            //LOG_ERROR("xx", "continue ");//测试
                            continue;
                        }

                        Loot const* loot = &(*itr)->loot;
                        //if ((*itr)->GetLootRecipientGuid() == GetPlayer()->GetObjectGuid())//如果玩家是当前的拾取者，执行一键拾取

                        //LOG_ERROR("xx", "l-GetLootRecipientGUID {} ", (*itr)->GetLootRecipientGUID().GetCounter());//测试
                        //LOG_ERROR("xx", "l-playerGetGUID {} ", GetPlayer()->GetGUID().GetCounter());//测试
                        //if(!loot->isLooted())
                        //LOG_ERROR("xx", "l-canLoot ");//测试

                        if ((*itr)->GetLootRecipientGUID() == GetPlayer()->GetGUID() && !loot->isLooted())//如果玩家是当前的拾取者，执行一键拾取
                        {
                            //LOG_ERROR("xx", "startloot creatureguid {} ", (*itr)->GetGUID().GetCounter());//测试
                            HandleLootOpcodePlus(_player, (*itr)->GetGUID());
                            HandleLootMoneyOpcodePlus(_player, (*itr)->GetGUID());

                            for (uint8 i = 0; i < MAX_NR_LOOT_ITEMS; ++i)
                                HandleAutostoreLootItemOpcodePlus(_player, (*itr)->GetGUID(), i);

                            HandleLootReleaseOpcodePlus(_player, (*itr)->GetGUID());
                        }

                        _limitnum++;

                    }

                    return;
                }
            }

        }

    }
}


void WorldSession::HandleLootOpcodePlus(Player* player, ObjectGuid guid)
{
    if (!guid.IsAnyTypeCreature() && !guid.IsPlayer() && !guid.IsCorpse())
    {
        return;
    }

    // Check possible cheat
    if (!player->IsAlive())
        return;

    if (!player->IsInWorld())
        return;

    if (player->IsNonMeleeSpellCast(false))
        player->InterruptNonMeleeSpells(false);


    GetPlayer()->SendLootPlus(guid, LOOT_CORPSE);


}

void WorldSession::HandleLootReleaseOpcode(WorldPacket& recvData)
{
    LOG_DEBUG("network", "WORLD: CMSG_LOOT_RELEASE");

    // cheaters can modify lguid to prevent correct apply loot release code and re-loot
    // use internal stored guid
    ObjectGuid guid;
    recvData >> guid;

    if (ObjectGuid lguid = GetPlayer()->GetLootGUID())
        if (lguid == guid)
            DoLootRelease(lguid);
}

void WorldSession::HandleLootReleaseOpcodePlus(Player* player, ObjectGuid guid)
{
    DoLootRelease(guid, true);
}

void WorldSession::DoLootRelease(ObjectGuid lguid, bool quick)
{
    Player*  player = GetPlayer();
    Loot*    loot;

    if (quick)
        player->SetLootGUID(lguid);
    else
        player->SetLootGUID(ObjectGuid());

    player->SendLootRelease(lguid);

    player->RemoveUnitFlag(UNIT_FLAG_LOOTING);

    if (!player->IsInWorld())
        return;

    if (lguid.IsGameObject())
    {
        GameObject* go = GetPlayer()->GetMap()->GetGameObject(lguid);

        // not check distance for GO in case owned GO (fishing bobber case, for example) or Fishing hole GO
        if (!go || ((go->GetOwnerGUID() != _player->GetGUID() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && !go->IsWithinDistInMap(_player)))
        {
            return;
        }

        loot = &go->loot;

        if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR)
        {
            // locked doors are opened with spelleffect openlock, prevent remove its as looted
            go->UseDoorOrButton();
        }
        else if (loot->isLooted() || go->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
        {
            if (go->GetGoType() == GAMEOBJECT_TYPE_FISHINGHOLE)
            {
                // The fishing hole used once more
                go->AddUse();                               // if the max usage is reached, will be despawned in next tick
                if (go->GetUseCount() >= go->GetGOValue()->FishingHole.MaxOpens)
                    go->SetLootState(GO_JUST_DEACTIVATED);
                else
                    go->SetLootState(GO_READY);
            }
            else
            {
                go->SetLootState(GO_JUST_DEACTIVATED);

                // Xinef: moved event execution to loot release (after everything is looted)
                // Xinef: 99% sure that this worked like this on blizz
                // Xinef: prevents exploits with just opening GO and spawning bilions of npcs, which can crash core if you know what you're doin ;)
                if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST && go->GetGOInfo()->chest.eventId)
                {
                    LOG_DEBUG("spells.aura", "Chest ScriptStart id {} for GO {}", go->GetGOInfo()->chest.eventId, go->GetSpawnId());
                    player->GetMap()->ScriptsStart(sEventScripts, go->GetGOInfo()->chest.eventId, player, go);
                }
            }

            loot->clear();
        }
        else
        {
            // not fully looted object
            go->SetLootState(GO_ACTIVATED, player);

            // if the round robin player release, reset it.
            if (player->GetGUID() == loot->roundRobinPlayer)
                loot->roundRobinPlayer.Clear();
        }
    }
    else if (lguid.IsCorpse())        // ONLY remove insignia at BG
    {
        Corpse* corpse = ObjectAccessor::GetCorpse(*player, lguid);
        if (!corpse || !corpse->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
            return;

        loot = &corpse->loot;

        // Xinef: Buggs client? (Opening loot after closing)
        //if (loot->isLooted())
        {
            loot->clear();
            corpse->RemoveFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);
        }
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetItemByGuid(lguid);
        if (!pItem)
            return;

        loot = &pItem->loot;
        ItemTemplate const* proto = pItem->GetTemplate();

        // destroy only 5 items from stack in case prospecting and milling
        if (proto->Flags & (ITEM_FLAG_IS_PROSPECTABLE | ITEM_FLAG_IS_MILLABLE))
        {
            pItem->m_lootGenerated = false;
            pItem->loot.clear();

            uint32 count = pItem->GetCount();

            // >=5 checked in spell code, but will work for cheating cases also with removing from another stacks.
            if (count > 5)
                count = 5;

            player->DestroyItemCount(pItem, count, true);
        }
        else if (pItem->loot.isLooted() || !proto->HasFlag(ITEM_FLAG_HAS_LOOT))
        {
            player->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
            return;
        }
    }
    else
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);
        if (!creature)
        {
            player->SendLootRelease(lguid);
            return;
        }

        bool lootAllowed = creature && creature->IsAlive() == (player->IsClass(CLASS_ROGUE, CLASS_CONTEXT_ABILITY) && creature->loot.loot_type == LOOT_PICKPOCKETING);
        //if (!lootAllowed || !creature->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
        if (!lootAllowed)
            return;

        loot = &creature->loot;
        if (loot->isLooted())
        {
            // skip pickpocketing loot for speed, skinning timer reduction is no-op in fact
            if (!creature->IsAlive())
                creature->AllLootRemovedFromCorpse();

            creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
            loot->clear();
        }
        else
        {
            // if the round robin player release, reset it.
            if (player->GetGUID() == loot->roundRobinPlayer)
            {
                loot->roundRobinPlayer.Clear();

                if (Group* group = player->GetGroup())
                    group->SendLooter(creature, nullptr);
            }
            // force dynflag update to update looter and lootable info
            creature->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
        }
    }

    //Player is not looking at loot list, he doesn't need to see updates on the loot list
    if (!lguid.IsItem())
    {
        loot->RemoveLooter(player->GetGUID());
    }
}

void WorldSession::HandleLootMasterGiveOpcode(WorldPacket& recvData)
{
    uint8 slotid;
    ObjectGuid lootguid, target_playerguid;

    recvData >> lootguid >> slotid >> target_playerguid;

    if (!_player->GetGroup() || _player->GetGroup()->GetMasterLooterGuid() != _player->GetGUID() || _player->GetGroup()->GetLootMethod() != MASTER_LOOT)
    {
        _player->SendLootError(lootguid, LOOT_ERROR_DIDNT_KILL);
        return;
    }

    Player* target = ObjectAccessor::GetPlayer(*_player, target_playerguid);
    if (!target)
    {
        _player->SendLootError(lootguid, LOOT_ERROR_PLAYER_NOT_FOUND);
        return;
    }

    LOG_DEBUG("network", "WorldSession::HandleLootMasterGiveOpcode (CMSG_LOOT_MASTER_GIVE, 0x02A3) Target = [{}].", target->GetName());

    if (_player->GetLootGUID() != lootguid)
    {
        _player->SendLootError(lootguid, LOOT_ERROR_DIDNT_KILL);
        return;
    }

    if (!_player->IsInRaidWith(target))
    {
        _player->SendLootError(lootguid, LOOT_ERROR_MASTER_OTHER);
        //LOG_DEBUG("network", "MasterLootItem: Player {} tried to give an item to ineligible player {} !", GetPlayer()->GetName(), target->GetName());
        return;
    }

    Loot* loot = nullptr;

    if (GetPlayer()->GetLootGUID().IsCreatureOrVehicle())
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lootguid);
        if (!creature)
            return;

        loot = &creature->loot;
    }
    else if (GetPlayer()->GetLootGUID().IsGameObject())
    {
        GameObject* pGO = GetPlayer()->GetMap()->GetGameObject(lootguid);
        if (!pGO)
            return;

        loot = &pGO->loot;
    }

    if (!loot)
        return;

    if (slotid >= loot->items.size() + loot->quest_items.size())
    {
        LOG_DEBUG("loot", "MasterLootItem: Player {} might be using a hack! (slot {}, size {})", GetPlayer()->GetName(), slotid, (unsigned long)loot->items.size());
        return;
    }

    LootItem& item = slotid >= loot->items.size() ? loot->quest_items[slotid - loot->items.size()] : loot->items[slotid];

    ItemPosCountVec dest;
    InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemid, item.count);
    if (!item.AllowedForPlayer(target, loot->sourceWorldObjectGUID))
        msg = EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
    if (msg != EQUIP_ERR_OK)
    {
        if (msg == EQUIP_ERR_CANT_CARRY_MORE_OF_THIS)
            _player->SendLootError(lootguid, LOOT_ERROR_MASTER_UNIQUE_ITEM);
        else if (msg == EQUIP_ERR_INVENTORY_FULL)
            _player->SendLootError(lootguid, LOOT_ERROR_MASTER_INV_FULL);
        else
            _player->SendLootError(lootguid, LOOT_ERROR_MASTER_OTHER);

        return;
    }

    // list of players allowed to receive this item in trade
    AllowedLooterSet looters = item.GetAllowedLooters();

    // not move item from loot to target inventory
    Item* newitem = target->StoreNewItem(dest, item.itemid, true, item.randomPropertyId, looters);
    target->SendNewItem(newitem, uint32(item.count), false, false, true);

    //335随机附魔mod的代码
    newitem->core_rollPossibleEnchant(target, newitem);

    target->UpdateLootAchievements(&item, loot);

    //添加拾取物品log
    if (item.itemid)
        LootItemAddLog(target, lootguid, item.itemid, item.count, _player);

    // mark as looted
    item.count = 0;
    item.is_looted = true;

    loot->NotifyItemRemoved(slotid);
    --loot->unlootedCount;
}
