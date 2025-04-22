
#include "AuctionHouseMgr.h"
#include "AuctionHouseBotMgr.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Logging/Log.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "World.h"
#include "Config.h"

AuctionHouseBotMgr::~AuctionHouseBotMgr()
{
    m_items.clear();

    if (m_config)
        m_config.reset();
}

void AuctionHouseBotMgr::Load()
{
    /* 1 - DELETE */
    m_items.clear();
    m_loaded = false;

    if (m_config)
        m_config.reset();

    /*2 - LOAD */
    QueryResult result = WorldDatabase.Query("SELECT `item`, `stack`, `bid`, `buyout` FROM `auctionhousebot`");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 AuctionHouseBot items");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;

    Field* fields;
    do
    {
        AuctionHouseBotEntry e;
        fields    = result->Fetch();
        e.item    = fields[0].Get<uint32>();
        e.stack   = fields[1].Get<uint32>();
        e.bid     = fields[2].Get<uint32>();
        e.buyout  = fields[3].Get<uint32>();

        m_items.push_back(e);

        //LOG_ERROR("auctionHouse", "item {}", e.item);//测试，有数据

        ++count;
    }
    while (result->NextRow());

    //LOG_ERROR("auctionHouse", "Loaded {}", count);//测试，有数据

    LOG_INFO("server.loading", ">> >> Loaded {} AuctionHouseBot items", count);
    LOG_INFO("server.loading", " ");


    /* CONFIG */
    m_config                 = std::make_unique<AuctionHouseBotConfig>();
    m_config->enable         = sWorld->getBoolConfig(CONFIG_AHBOT_ENABLE);
    //m_config->ahid          = (AuctionHouseId)sWorld->getIntConfig(CONFIG_AHBOT_AH_ID);//AHid用这个，共用的是7黑水拍卖行
    m_config->ahid           = AuctionHouseId::Neutral;
    m_config->botguid        = sWorld->getIntConfig(CONFIG_AHBOT_BOT_GUID);
    m_config->botaccount     = sWorld->getIntConfig(CONFIG_AHBOT_ACCOUNT);
    m_config->ahfid          = sWorld->getIntConfig(CONFIG_AHBOT_AH_FID);//这个好像没用到
    m_config->itemcount      = sWorld->getIntConfig(CONFIG_AHBOT_ITEMCOUNT);

    m_auctionHouseEntry = sAuctionMgr->GetAuctionHouseEntryFromFactionTemplate((uint32)m_config->ahid);
    if (!m_auctionHouseEntry)
    {
        LOG_ERROR("auctionHouse", "AAHBot::Load() : No auction house for faction {}", (uint32)m_config->ahid);
        return;
    }
    m_loaded = true;
}

void AuctionHouseBotMgr::Update(bool force /* = false */)
{
    if (!m_loaded)
        return;

    ASSERT(m_config);
    ASSERT(m_auctionHouseEntry);

    if (!(m_config->enable || force))
        return;

    if (m_items.empty() ||  /*m_config->botguid==0 ||*/ m_config->botaccount == 0)
    {
        LOG_ERROR("auctionHouse", "AHBot::Update() : Bad config or empty table.");
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr->GetAuctionsMap(m_auctionHouseEntry->houseId);
    if (!auctionHouse)
    {
        LOG_ERROR("auctionHouse", "AHBot::Update() : No auction house for faction {}", m_config->ahfid);
        return;
    }

    uint32 auctions     = auctionHouse->Getcount();
    uint32 items        = m_config->itemcount;
    uint32 entriesCount = m_items.size();

    //LOG_ERROR("auctionHouse", "auctions {} items {} entriesCount {}", auctions, items, entriesCount);//测试

    while (auctions < items)
    {
        AuctionHouseBotEntry item = m_items[urand(0, entriesCount - 1)];
        AddItem(item, auctionHouse);
        auctions++;
    }
}

AuctionHouseBotMgr* AuctionHouseBotMgr::instance()
{
    static AuctionHouseBotMgr instance;
    return &instance;
}


void AuctionHouseBotMgr::AddItem(AuctionHouseBotEntry e, AuctionHouseObject *auctionHouse)
{
    ASSERT(m_auctionHouseEntry);

    ItemTemplate const* prototype = sObjectMgr->GetItemTemplate(e.item);
    if (prototype == nullptr)
    {
        LOG_ERROR("auctionHouse", "AHBot::AddItem() : Item {} does not exist.", e.item);
        return;
    }

    Item* item = Item::CreateItem(e.item, 1);
    if (!item)
    {
        LOG_ERROR("auctionHouse", "AHBot::AddItem() : Cannot create item.");
        return;
    }

    LOG_ERROR("auctionHouse", "AHBot::AddItem() : Adding item {}", e.item);

    uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(e.item);
    if (randomPropertyId != 0)
        item->SetItemRandomProperties(randomPropertyId);

    uint32 etime = urand(1, 3);
    switch (etime)
    {
        case 1:
            etime = 43200;
            break;
        case 2:
            etime = 86400;
            break;
        case 3:
            etime = 172800;
            break;
        default:
            etime = 86400;
            break;
    }
    item->SetCount(e.stack);

    uint32 dep = sAuctionMgr->GetAuctionDeposit(m_auctionHouseEntry, etime, item, item->GetCount());

    AuctionEntry* auctionEntry       = new AuctionEntry;
    auctionEntry->Id                 = sObjectMgr->GenerateAuctionID();
    auctionEntry->houseId            = m_config->ahid;
    auctionEntry->auctionHouseEntry  = m_auctionHouseEntry;
    auctionEntry->item_guid          = item->GetGUID();
    auctionEntry->item_template      = item->GetEntry();
    auctionEntry->owner              = ObjectGuid::Empty;
    auctionEntry->startbid           = e.bid;
    auctionEntry->buyout             = e.buyout;
    auctionEntry->bidder             = ObjectGuid::Empty;
    auctionEntry->bid                = 0;
    auctionEntry->deposit            = dep;
    auctionEntry->expire_time        = (time_t) etime + time(nullptr);

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    item->FSetState(ITEM_NEW);
    item->SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    sAuctionMgr->AddAItem(item);
    auctionHouse->AddAuction(auctionEntry);
    CharacterDatabaseTransaction trans2 = CharacterDatabase.BeginTransaction();
    auctionEntry->SaveToDB(trans2);
    CharacterDatabase.CommitTransaction(trans2);
}


