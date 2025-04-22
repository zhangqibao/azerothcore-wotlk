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

#include "BankPackets.h"
#include "DBCStores.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

bool WorldSession::CanUseBank(ObjectGuid bankerGUID) const
{
    // bankerGUID parameter is optional, set to 0 by default.
    if (!bankerGUID)
        bankerGUID = m_currentBankerGUID;

    bool isUsingBankCommand = (bankerGUID == GetPlayer()->GetGUID() && bankerGUID == m_currentBankerGUID);

    if (!isUsingBankCommand)
    {
        Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(bankerGUID, UNIT_NPC_FLAG_BANKER);
        if (!creature)
            return false;
    }

    return true;
}

void WorldSession::HandleBankerActivateOpcode(WorldPacket& recvData)
{
    ObjectGuid guid;

    recvData >> guid;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_BANKER);
    if (!unit)
    {
        LOG_DEBUG("network", "WORLD: HandleBankerActivateOpcode - Unit ({}) not found or you can not interact with him.", guid.ToString());
        return;
    }

    // remove fake death
    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    SendShowBank(guid);
}

void WorldSession::HandleAutoBankItemOpcode(WorldPackets::Bank::AutoBankItem& packet)
{
    LOG_DEBUG("network", "STORAGE: receive bag = {}, slot = {}", packet.Bag, packet.Slot);

    if (!CanUseBank())
    {
        LOG_DEBUG("network", "WORLD: HandleAutoBankItemOpcode - Unit ({}) not found or you can't interact with him.", m_currentBankerGUID.ToString());
        return;
    }

    Item* item = _player->GetItemByPos(packet.Bag, packet.Slot);
    if (!item)
        return;

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, item, nullptr);
        return;
    }

    if (dest.size() == 1 && dest[0].pos == item->GetPos())
    {
        _player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
        return;
    }

    if (dest[0].pos > 65318 && dest[0].pos < 65343)//从包里往银行右键存放，这个pos是物品在银行的栏位id，如果是前24格
    {
        //判断是否拥有赫拉迪姆魔盒
        if (_player->getHLDM())
        {
            //修复魔盒bug，以免利用魔盒换装后保持魔盒装备的效果------
            _player->SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, item, nullptr);
            return;
            //修复魔盒bug，以免利用魔盒换装后保持魔盒装备的效果---end---
        }
    }
    else
    {
        //判断是否是赞助坐骑，如果是就禁止
        if (m_zuoqis.size() > 0)
        {
            if (std::find(m_zuoqis.begin(), m_zuoqis.end(), item->GetEntry()) != m_zuoqis.end())
            {
                _player->SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, item, nullptr);
                return;
            }
        }
    }

    _player->RemoveItem(packet.Bag, packet.Slot, true);
    _player->ItemRemovedQuestCheck(item->GetEntry(), item->GetCount());
    _player->BankItem(dest, item, true);
    _player->UpdateTitansGrip();
}

void WorldSession::HandleAutoStoreBankItemOpcode(WorldPackets::Bank::AutoStoreBankItem& packet)
{
    LOG_DEBUG("network", "STORAGE: receive bag = {}, slot = {}", packet.Bag, packet.Slot);

    if (!CanUseBank())
    {
        LOG_DEBUG("network", "WORLD: HandleAutoStoreBankItemOpcode - Unit ({}) not found or you can't interact with him.", m_currentBankerGUID.ToString());
        return;
    }

    Item* item = _player->GetItemByPos(packet.Bag, packet.Slot);
    if (!item)
        return;

    if (packet.Slot > 38 && packet.Slot < 63)//从银行往包里右键存放，这个slot是物品在来源银行的栏位id，如果是前16格
    {
        //判断是否拥有赫拉迪姆魔盒
        if (_player->getHLDM())
        {
            //修复魔盒bug，以免利用魔盒换装后保持魔盒装备的效果------
            //判断是否拥有赫拉迪姆魔盒,无论是否有魔盒，都执行，因为可能用坐骑刷高属性
            _player->SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, item, nullptr);
            return;
            //修复魔盒bug，以免利用魔盒换装后保持魔盒装备的效果---end---
        }

    }
    else//如果不是银行的前24格
    {
        //判断是否是赞助坐骑，如果是就禁止
        if (m_zuoqis.size() > 0)
        {
            if (std::find(m_zuoqis.begin(), m_zuoqis.end(), item->GetEntry()) != m_zuoqis.end())
            {
                _player->SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, item, nullptr);
                return;
            }
        }

    }

    if (_player->IsBankPos(packet.Bag, packet.Slot))                    // moving from bank to inventory
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, item, nullptr);
            return;
        }

        _player->RemoveItem(packet.Bag, packet.Slot, true);
        if (Item const* storedItem = _player->StoreItem(dest, item, true))
            _player->ItemAddedQuestCheck(storedItem->GetEntry(), storedItem->GetCount());
    }
    else                                                    // moving from inventory to bank
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, item, nullptr);
            return;
        }

        _player->RemoveItem(packet.Bag, packet.Slot, true);
        _player->ItemRemovedQuestCheck(item->GetEntry(), item->GetCount());
        _player->BankItem(dest, item, true);
        _player->UpdateTitansGrip();
    }
}

void WorldSession::HandleBuyBankSlotOpcode(WorldPackets::Bank::BuyBankSlot& buyBankSlot)
{
    WorldPackets::Bank::BuyBankSlotResult packet;
    if (!CanUseBank(buyBankSlot.Banker))
    {
        packet.Result = ERR_BANKSLOT_NOTBANKER;
        SendPacket(packet.Write());
        LOG_DEBUG("network", "WORLD: HandleBuyBankSlotOpcode - {} not found or you can't interact with him.", buyBankSlot.Banker.ToString());
        return;
    }

    uint32 slot = _player->GetBankBagSlotCount();

    // next slot
    ++slot;

    LOG_INFO("network", "PLAYER: Buy bank bag slot, slot number = {}", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    if (!slotEntry)
    {
        packet.Result = ERR_BANKSLOT_FAILED_TOO_MANY;
        SendPacket(packet.Write());
        return;
    }

    uint32 price = slotEntry->price;

    if (!_player->HasEnoughMoney(price))
    {
        packet.Result = ERR_BANKSLOT_INSUFFICIENT_FUNDS;
        SendPacket(packet.Write());
        return;
    }

    _player->SetBankBagSlotCount(slot);
    _player->ModifyMoney(-int32(price));

    packet.Result = ERR_BANKSLOT_OK;
    SendPacket(packet.Write());

    _player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BUY_BANK_SLOT);
}

void WorldSession::SendShowBank(ObjectGuid guid)
{
    m_currentBankerGUID = guid;
    WorldPackets::Bank::ShowBank packet;
    packet.Banker = guid;
    SendPacket(packet.Write());
}
