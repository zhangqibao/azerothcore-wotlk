#include "ScriptPCH.h"
#include "Player.h"
#include "Pet.h"
#include "CreatureScript.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#pragma execution_character_set("utf-8")


class npc_zy : public CreatureScript
{
public:
    npc_zy() :CreatureScript("npc_zy") {}

    //转职函数
    bool SetClassToNew(Player* player, uint8 newclass)
    {
        //检查
        if (!player || !player->IsInWorld() || !player->IsAlive() || player->IsInCombat() || player->getClass() == newclass || newclass <= 0 || newclass >= MAX_CLASSES)
            return false;

        //检查包里是否有足够的红包券
        if (player->GetItemCount(70008) < 1000)
        {
            ItemTemplate const* vProto = sObjectMgr->GetItemTemplate(70008);
            if (vProto)
                ChatHandler(player->GetSession()).PSendSysMessage(LANG_CURRENCY_NOT_ENOUGH, vProto ? vProto->Name1 : nullptr, vProto ? vProto->Name1 : nullptr, 1000);
            return false;
        }


        //---------------------------------战士---骑士---猎人---盗贼---牧师----DK----萨满----法师---术士--------小德--------------------//
        int const DBclass[MAX_CLASSES] = { 3042,  23128, 3352,  3328,  6018, 29194,  3344,   5885,  3324, NULL, 12042 };

        //遗忘旧职业技能 数组是技能训练师ID 技能自行补全 保证转职不会影响其他渠道获取的技能
        TrainerSpellData const* Spells = sObjectMgr->GetNpcTrainerSpells(DBclass[player->getClass() - 1]);
        if (Spells)
        {
            for (TrainerSpellMap::const_iterator its = Spells->spellList.begin(); its != Spells->spellList.end(); ++its)
            {
                //保险防止作弊
                if (!player->IsSpellFitByClassAndRace(its->second.spell))
                    continue;
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(its->second.spell);
                if (!spellInfo)
                    continue;
                if (!SpellMgr::IsSpellValid(spellInfo))
                    continue;
                player->removeSpell(its->second.spell, SPEC_MASK_ALL, false);
            }


        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("转职失败");
            return false;
        }

        //移除宠物 与相关的东西
        if (Pet* pet = player->GetPet())
        {
            uint32 PGuid = pet->GetCharmInfo()->GetPetNumber();
            player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
            SQLTransaction trans = CharacterDatabase.BeginTransaction();
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_DECLINEDNAME);
            stmt->SetData(0, PGuid);
            trans->Append(stmt);
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PET_AURAS);
            stmt->SetData(0, PGuid);
            trans->Append(stmt);
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PET_SPELLS);
            stmt->SetData(0, PGuid);
            trans->Append(stmt);
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PET_SPELL_COOLDOWNS);
            stmt->SetData(0, PGuid);
            trans->Append(stmt);
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_BY_OWNER);
            stmt->SetData(0, player->GetGUID().GetCounter());
            trans->Append(stmt);
            CharacterDatabase.CommitTransaction(trans);
        }

        //移除雕文
        for (uint32 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        {
            if (slot >= MAX_GLYPH_SLOT_INDEX)
                continue;

            if (uint32 glyph = player->GetGlyph(slot))
            {
                if (GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyph))
                {
                    player->RemoveAurasDueToSpell(gp->SpellId);
                    player->SetGlyph(slot, 0, true);
                    player->SendTalentsInfoData(false);
                }
            }
        }


        //技能ID-----------    长柄/魔杖/单手剑/单手斧/单手锤/双手剑/双手锤/弓/枪械/弩/双武器/拳套/开锁/匕首/投掷/投掷/法杖/板甲-----//
        int const Wpells[18] = { 200,5009, 201,  196,  198,    202,  199,  5011,266,264,674, 15590,1804,1180,2567,2764,227,750 };

        //遗忘 不是角色出生默认的武器技能
        for (int w = 0; w < 18; ++w)
            player->removeSpell(Wpells[w], SPEC_MASK_ALL, false);



        //技能SKILLID-----------     长柄/魔杖/单手剑/单手斧/单手锤/双手剑/双手锤/弓/枪械/弩/双武器/拳套/开锁/匕首/投掷/法杖/板甲-----//
        int const WSkills[17] =       { 229,226, 43,    44,     54,    55,  160,   45, 46,  226,118,  473, 633, 173, 176,136,293     };

        //遗忘SKILLID
        for (int w = 0; w < 17; ++w)
            player->SetSkill(WSkills[w], 0, 0, 0);



        //遗忘出生默认的技能 包含种族特长
        PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(true), player->getClass());
        for (PlayerCreateInfoSpells::const_iterator itc = info->customSpells.begin(); itc != info->customSpells.end(); ++itc)
            player->removeSpell(*itc, SPEC_MASK_ALL, false);

        //遗忘出生默认的技能 包含种族特长
        for (PlayerCreateInfoSkills::const_iterator itr = info->skills.begin(); itr != info->skills.end(); ++itr)
        {
            player->SetSkill(itr->SkillId, 0, 0, 0);
        }
        //遗忘职业任务奖励的技能


        //重置天赋
        player->resetTalents();
        player->InitTalentForLevel();
        player->SendTalentsInfoData(false);

        //执行转职
        player->SetByteValue(UNIT_FIELD_BYTES_0, 1, newclass);

        //更新角色新职业信息
        CharacterDatabase.Execute("UPDATE characters SET class = {} WHERE guid = {}", newclass, player->GetGUID().GetCounter());
        //sWorld->UpdateGlobalPlayerData(player->GetGUIDLow(), PLAYER_UPDATE_DATA_CLASS, player->GetName(), player->getLevel(), player->getGender(), player->getRace(), newclass);

        //强制把身上不符合新职业的武器装备卸下 有位置就放 没位置自动发到邮箱里
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                //判断不符合要求的
                uint16 Cdest = 0;
                ItemTemplate const* pProto = pItem->GetTemplate();
                InventoryResult Check = player->CanEquipItem(NULL_SLOT, Cdest, pItem, false);
                if ((pProto->AllowableClass & player->getClassMask()) == 0 || (pProto->RequiredSpell != 0 && !player->HasSpell(pProto->RequiredSpell)) || Check != EQUIP_ERR_OK)
                {
                    ItemPosCountVec dest;
                    uint8 Msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
                    if (Msg == EQUIP_ERR_OK)
                    {
                        player->RemoveItem(INVENTORY_SLOT_BAG_0, pItem->GetSlot(), true);
                        player->StoreItem(dest, pItem, true);
                        //更新泰坦之握
                        player->UpdateTitansGrip();
                    }
                    else
                    {
                        player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, pItem->GetSlot(), true);
                        SQLTransaction trans = CharacterDatabase.BeginTransaction();
                        pItem->DeleteFromInventoryDB(trans);
                        pItem->SaveToDB(trans);
                        MailDraft("转职", "这些装备不支持新职业").AddItem(pItem).SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
                        CharacterDatabase.CommitTransaction(trans);
                    }
                }
            }
        }

        //保存角色信息
        player->SaveToDB(false, false);
        //重新学习默认技能
        player->LearnDefaultSkills();// learnDefaultSpells()
        //重置属性获取对应职业的Power(能量条/怒气条之类的)
        player->InitStatsForLevel(true);
        //如果新职业是DK就载入符文冷却
        player->InitRunes();
        //更新角色属性
        player->SetCanModifyStats(true);
        player->UpdateAllStats();

        //重新学习新职业技能。。。自行到训练师处重新学习。。。否则技能摆放很乱。。需小退
        /*
        TrainerSpellData const* NewSpells = sObjectMgr->GetNpcTrainerSpells(DBclass[newclass - 1]);
        if (NewSpells)
        {
                for (TrainerSpellMap::const_iterator its = NewSpells->spellList.begin(); its != NewSpells->spellList.end(); ++its)
                {
                    //保险防止作弊
                        if (!player->IsSpellFitByClassAndRace(its->second.spell))
                                continue;
                        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(its->second.spell);
                        if (!spellInfo)
                                continue;
                        if (!SpellMgr::IsSpellValid(spellInfo))
                                continue;
                        player->_addSpell(*its, SPEC_MASK_ALL, true);
                }
        }
        */

        //刷新服务器角色缓存信息
        //sCharacterCache->RefreshCacheEntry(player->GetGUID().GetCounter());//离线的话这个清缓存可以，在线不能这样清,否则guild就被清零了
        sCharacterCache->RefreshCacheEntry(player->GetGUID().GetCounter(), player->GetGuildId());
        
        /*
        //这个不能正常刷新角色缓存
        if (Player* cPlayer = ObjectAccessor::FindConnectedPlayer(player->GetGUID()))
        {
            if (sCharacterCache->HasCharacterCacheEntry(cPlayer->GetGUID()))
            {
                sCharacterCache->UpdateCharacterData(cPlayer->GetGUID(), cPlayer->GetName(), cPlayer->getGender(), cPlayer->getRace());
            }
            else
            {
                sCharacterCache->AddCharacterCacheEntry(cPlayer->GetGUID(), cPlayer->GetSession()->GetAccountId(), cPlayer->GetName(),
                    cPlayer->getGender(), cPlayer->getRace(), cPlayer->getClass(), cPlayer->GetLevel());
            }

            sCharacterCache->UpdateCharacterAccountId(cPlayer->GetGUID(), cPlayer->GetSession()->GetAccountId());
            sCharacterCache->UpdateCharacterGuildId(cPlayer->GetGUID(), cPlayer->GetGuildId());
            sCharacterCache->UpdateCharacterMailCount(cPlayer->GetGUID(), cPlayer->GetMailSize(), true);
            sCharacterCache->UpdateCharacterArenaTeamId(cPlayer->GetGUID(), ARENA_SLOT_2v2, cPlayer->GetArenaTeamId(ARENA_SLOT_2v2));
            sCharacterCache->UpdateCharacterArenaTeamId(cPlayer->GetGUID(), ARENA_SLOT_3v3, cPlayer->GetArenaTeamId(ARENA_SLOT_3v3));
            sCharacterCache->UpdateCharacterArenaTeamId(cPlayer->GetGUID(), ARENA_SLOT_5v5, cPlayer->GetArenaTeamId(ARENA_SLOT_5v5));

            if (Group* group = cPlayer->GetGroup())
            {
                sCharacterCache->UpdateCharacterGroup(cPlayer->GetGUID(), group->GetGUID());
            }
            else
            {
                sCharacterCache->ClearCharacterGroup(cPlayer->GetGUID());
            }
        }
        */
        //end -----------


        ChatHandler(player->GetSession()).PSendSysMessage("恭喜你转职完成");
        player->GetSession()->SendAreaTriggerMessage("恭喜你转职完成");

        //如果是转职为dk,传送到dk出生地
        if (newclass == CLASS_DEATH_KNIGHT)
        {
            //如果已经完成过最终任务 黑锋要塞之战 13166
            if (player->GetQuestStatus(13166) == QUEST_STATUS_REWARDED)
            {
                //就不传送到出生地了
            }
            else
            {
                player->TeleportTo(609, 2357.6f, -5663.94f, 426.03f, player->GetOrientation(), TELE_TO_NOT_UNSUMMON_PET);
                WorldLocation homeLoc = WorldLocation(609, 2357.6f, -5663.94f, 426.03f, player->GetOrientation());
                player->SetHomebind(homeLoc, 4342);
                //需要删除炉石，否则如果玩家使用会造成任务链断裂无法下到地面
                player->DestroyItemCount(6948, 1, true);
            }

        }

        
        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        uint32 Race = player->getRace();
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, 10, "欢迎使用转职功能", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);

        if (Race != RACE_BLOODELF && player->getClass() != CLASS_WARRIOR)
            AddGossipItemFor(player, 10, "转职为战士", CLASS_WARRIOR, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if (Race != RACE_NIGHTELF && Race != RACE_GNOME && Race != RACE_ORC && Race != RACE_UNDEAD_PLAYER && Race != RACE_TAUREN && Race != RACE_TROLL && player->getClass() != CLASS_PALADIN)
            AddGossipItemFor(player, 10, "转职为圣骑士", CLASS_PALADIN, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if (Race != RACE_HUMAN && Race != RACE_GNOME && Race != RACE_UNDEAD_PLAYER && player->getClass() != CLASS_HUNTER)
            AddGossipItemFor(player, 10, "转职为猎人", CLASS_HUNTER, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if (Race != RACE_DRAENEI && Race != RACE_TAUREN && player->getClass() != CLASS_ROGUE)
            AddGossipItemFor(player, 10, "转职为盗贼", CLASS_ROGUE, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if (Race != RACE_GNOME && Race != RACE_ORC && Race != RACE_TAUREN && player->getClass() != CLASS_PRIEST)
            AddGossipItemFor(player, 10, "转职为牧师", CLASS_PRIEST, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if (sWorld->getIntConfig(CONFIG_WOWPATCH) > 0 || sWorld->getIntConfig(CONFIG_UINT32_EARNXP_MAX_PLAYER_LEVEL) >= 60)
        {
            if (player->getClass() != CLASS_DEATH_KNIGHT && player->GetLevel()>54)
                AddGossipItemFor(player, 10, "转职为死亡骑士", CLASS_DEATH_KNIGHT, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);
        }

        if ((Race == RACE_DRAENEI || Race == RACE_ORC || Race == RACE_TAUREN || Race == RACE_TROLL) && player->getClass() != CLASS_SHAMAN)
            AddGossipItemFor(player, 10, "转职为萨满祭司", CLASS_SHAMAN, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if ((Race == RACE_HUMAN || Race == RACE_GNOME || Race == RACE_DRAENEI || Race == RACE_UNDEAD_PLAYER || Race == RACE_TROLL || Race == RACE_BLOODELF) && player->getClass() != CLASS_MAGE)
            AddGossipItemFor(player, 10, "转职为法师", CLASS_MAGE, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if ((Race == RACE_HUMAN || Race == RACE_GNOME || Race == RACE_ORC || Race == RACE_UNDEAD_PLAYER || Race == RACE_BLOODELF) && player->getClass() != CLASS_WARLOCK)
            AddGossipItemFor(player, 10, "转职为术士", CLASS_WARLOCK, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        if ((Race == RACE_NIGHTELF || Race == RACE_TAUREN) && player->getClass() != CLASS_DRUID)
            AddGossipItemFor(player, 10, "转职为德鲁伊", CLASS_DRUID, GOSSIP_ACTION_INFO_DEF + 999, "转职需要花费1000红包券，且会遗忘掉已学习过的职业技能，确定继续吗？", 0, false);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        ClearGossipMenuFor(player);

        if (sender == GOSSIP_SENDER_MAIN && action == GOSSIP_ACTION_INFO_DEF)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (action == GOSSIP_ACTION_INFO_DEF + 999 && sender)
        {
            if (sender <= 0 || sender >= MAX_CLASSES)
                return true;

            if (!SetClassToNew(player, (uint8)sender))
                return true;

            //以下是扣材料
            player->DestroyItemCount(70008, 1000, true);
            //----
        }

        CloseGossipMenuFor(player);

        player->GetSession()->LogoutPlayer(true);//下线

        return true;
    }
};

void AddSC_npc_zy()
{
    new npc_zy;
}

