-- 配置文件的WowPatch阶段设置为0，默认0为香草，1为TBC，2为WLK，3为加难香草，4为加难TBC
-- WowPatch = 0


-- 0,1,2阶段时，更新所有远古装备为不可见

-- 赫拉迪姆宝珠、幸运串珠从购买里移除
delete from npc_vendor where entry=60013 and item in (69978);
delete from npc_vendor where entry=60013 and item in (69977);


-- 远古盔灵 、 太古盔灵
update item_template set VerifiedBuild=99999 where  entry in(95998,95999) ;

-- 风剑新装备
update item_template set VerifiedBuild=99999 where  entry>97999 and entry<98012 ;	

-- 远古套装
update item_template set VerifiedBuild=99999 where  entry>95999 and entry<99999 ;	

-- 隐藏符咒碎石和符咒石板
update item_template set VerifiedBuild=99999 where  entry in(94998,94999);

-- 如果之前修改了所有>60装备为60级，这里执行sql：防止跨阶段获得物品，更新所有物品等级大于100的物品，需要等级为81（任务奖励装备不改）
-- update item_template set  RequiredLevel=81 where ItemLevel>100 and RequiredLevel=60 and entry<69990;


-- 卡拉赞外面的NPC和怪屏蔽
update creature set spawnMask=0 where id1 in (18255,17613,18253,7370,12377,12378,12379,12380);


-- 红包券开出的几率
-- select * from item_loot_template   where entry in(70134,70135,70136) and item=70008;
update item_loot_template set Chance='8' where entry in(70134,70135,70136) and item=70008;

--  点券几率
-- select * from item_loot_template   where entry in(70134,70135,70136) and item=70000;
update item_loot_template set Chance='6' where entry in(70134,70135,70136) and item=70000;


-- 根据不同阶段，禁止掉一些荣誉兑换装备的NPC 战场装/荣誉装/军装/竞技场装等
update creature set spawnMask=0 where id1 in (12794,12795,12784,12785); #75装等
update creature set spawnMask=0 where id1 in (12788,12778); #154装等

-- 42079
-- SELECT * FROM `npc_vendor` where item=42079 LIMIT 50;
update creature set spawnMask=0 where id1 in (34038,34075,34077,34059); #245装等，后面两个没有找到

-- 40807
-- SELECT * FROM `npc_vendor` where item=40807 LIMIT 50;
update creature set spawnMask=0 where id1 in (33915,33921,33926,33928,33934,33938,33941,34063,34084,35494,35495,35573,35574,40607); #232装等

-- 51334
-- SELECT * FROM `npc_vendor` where item=51334 LIMIT 50;
update creature set spawnMask=0 where id1 in (34060,34078); #264装等

-- 51399
-- SELECT * FROM `npc_vendor` where item=51399 LIMIT 50;
update creature set spawnMask=0 where id1 in (34093,34094,34095); #277装等竞技场装备

-- 40847
-- SELECT * FROM `npc_vendor` where item=40847 LIMIT 50;
update creature set spawnMask=0 where id1 in (33915,33921,33926,33928,33934,33938,33941,34063,34084,35494,35495,35573,35574,40607); #232装等竞技场装备（6赛季）

-- 40812
-- SELECT * FROM `npc_vendor` where item=40812 LIMIT 50;
update creature set spawnMask=0 where id1 in (33924,33927,33933,33935,33937,37941,37942,38858); #251装等竞技场装备（7赛季）

-- 51397
-- SELECT * FROM `npc_vendor` where item=51397 LIMIT 50;
update creature set spawnMask=0 where id1 in (33936,33939,33940); #264装等竞技场装备



-- 分阶段开放地图设定
-- 黑暗之门（外域）
delete from  areatrigger_teleport where ID in (4352,4354);


-- 新世界技能购买NPC不刷新出来
update creature set spawnMask=0 where id1 in (60111); 

-- 关闭新世界随机本日常任务
update quest_template set MinLevel=100 where id in(30081,30082,30083,30084);


-- 关闭随机附魔，移除重铸卷轴和赫拉迪姆水晶
update item_template set VerifiedBuild=99999 where entry in(69996,69999,69989);
update item_template set VerifiedBuild=99999 where entry in(69987,69988,69998);

-- 放出重铸卷轴
update item_template set VerifiedBuild=12340 where entry in(69999);


-- 下面是恢复到P0阶段的SQL

-- 三大本先关闭
-- 斯坦索姆
delete from disables WHERE `entry` IN (329);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 329, 1, "", "", "STSM");

-- 通灵学院
delete from disables WHERE `entry` IN (289);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 289, 1, "", "", "STSM");

-- 黑石塔
delete from disables WHERE `entry` IN (229);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 229, 1, "", "", "STSM");

-- 厄运
DELETE FROM `disables` WHERE `entry` = 429;
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 429, 1, "", "", "厄运之槌");

-- ZG,FX等团本周常先不开放
update quest_template  set MinLevel=100 where id in(30017,30019);
update quest_template  set MinLevel=100 where id in(30018);
update quest_template  set MinLevel=100 where id in(30027);
update quest_template  set MinLevel=100 where id in(30028);
update quest_template  set MinLevel=100 where id in(30021,30022,30023,30024,30025);
-- NAXX团本周常先不开放
update quest_template  set MinLevel=100  where id in(30029,30031,30032,30033,30034);
-- 黑曜石圣殿团本周常先不开放
update quest_template  set MinLevel=100  where id in(30035);
-- 永恒之眼团本周常先不开放
update quest_template  set MinLevel=100  where id in(30036);
-- 奥杜尔团本周常先不开放
update quest_template  set MinLevel=100  where id in(30037);
-- 黑龙：奥妮克希亚 团本周常先不开放
update quest_template  set MinLevel=100  where id in(30038);
-- 十字军的试炼团本周常先不开放
update quest_template  set MinLevel=100  where id in(30039);
-- 红玉圣殿团本周常先不开放
update quest_template  set MinLevel=100  where id in(30040);
-- 冰冠堡垒团本周常先不开放
update quest_template  set MinLevel=100  where id in(30041);


-- 80本奖励改40752英雄纹章，80H本奖励改40753勇气纹章
update quest_template  set RewardItem1=40752,RewardAmount1=2,RewardItem2=70013,RewardAmount2=1,RewardItem3=0,RewardAmount3=0 where id in(24790);#1st 80
update quest_template  set RewardItem1=40752,RewardAmount1=1,RewardItem2=70013,RewardAmount2=1,RewardItem3=0,RewardAmount3=0 where id in(24791);#nth 80

update quest_template  set RewardItem1=40753,RewardAmount1=2,RewardItem2=70014,RewardAmount2=1,RewardItem3=0,RewardAmount3=0 where id in(24788);#1st hero 80
update quest_template  set RewardItem1=40752,RewardAmount1=2,RewardItem2=70014,RewardAmount2=1,RewardItem3=0,RewardAmount3=0 where id in(24789);#nth hero 80

-- 更新所有凯旋纹章为勇气纹章 P1阶段，等P3阶段再改回来
update creature_loot_template set item=40753  where item=47241;
update item_loot_template set item=40753  where item=47241;
update gameobject_loot_template set item=40753  where item=47241;
update reference_loot_template set item=40753  where item=47241;

-- -- 周常任务奖励改成勇气纹章 80团本周常
update quest_template set RewardItem4=40753 where RewardItem4=47241 and  id in(30029,30031,30032,30033,30034,30035,30036,30037,30038,30039,30040,30041);

-- 关闭所有团本

-- 60 level range - Tier 1 MC
DELETE FROM `disables` WHERE `entry` = 409;
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 409, 1, "", "", "Molten Core");

-- 60 level range - Tier 2 BWL
DELETE FROM `disables` WHERE `entry` = 469;
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 469, 1, "", "", "Blackwing Lair");

-- 60 level range - Zul’Gurub ZG
DELETE FROM `disables` WHERE `entry` = 309;
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 309, 1, "", "", "Zul’Gurub");

-- 60 level range - Ahn’Qiraj AQL
DELETE FROM `disables` WHERE `entry` IN (509, 531);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 509, 1, "", "", "Ahn’Qiraj Ruins"),
(2, 531, 1, "", "", "Ahn’Qiraj Temple");



-- 70 level range 团本
DELETE FROM `disables` WHERE `entry` IN (532, 534, 544, 548, 550, 564, 565, 568, 580);

INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 532, 1, "", "", "Karazhan"), -- 卡拉赞
(2, 534, 1, "", "", "Hyjal Summit"), -- 海加尔山之战 
(2, 544, 1, "", "", "Magtheridon's Lair"), -- 玛瑟里顿
(2, 548, 1, "", "", "Serpentshrine Cavern"), -- 毒蛇 
(2, 550, 1, "", "", "The Eye"), -- 风暴要塞
(2, 564, 1, "", "", "Black Temple"), -- 黑暗神殿 
(2, 565, 1, "", "", "Gruul's Lair"), -- 格鲁尔
(2, 568, 1, "", "", "Zul'Aman"), --  祖阿曼
(2, 580, 1, "", "", "Sunwell Plateau"); -- 太阳之井

-- TBC 五人本

-- 61-69的副本
DELETE FROM `disables` WHERE `entry` IN (543, 542, 547, 546, 557, 558);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 543, 3, "", "", "地狱火城墙"),
(2, 542, 3, "", "", "鲜血熔炉"),
(2, 547, 3, "", "", "奴隶围栏"),
(2, 546, 3, "", "", "幽暗沼泽"),
(2, 557, 3, "", "", "法力陵墓"),
(2, 558, 3, "", "", "奥金尼地穴");

-- 70的副本
DELETE FROM `disables` WHERE `entry` IN (540, 545, 556, 552, 553, 554, 555,  585);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 540, 3, "", "", "破碎大厅"),
(2, 545, 3, "", "", "蒸汽地窖"),
(2, 556, 3, "", "", "塞泰克大厅"),
(2, 552, 3, "", "", "禁魔监狱"),
(2, 553, 3, "", "", "生态船"),
(2, 554, 3, "", "", "能源舰"),
(2, 555, 3, "", "", "暗影迷宫"),
(2, 585, 3, "", "", "魔导师平台");

DELETE FROM `disables` WHERE `entry` IN (560, 595, 269);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 560, 3, "", "", "旧希尔斯布莱德丘陵：逃离敦霍尔德");
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 595, 3, "", "", "净化斯坦索姆");
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 269, 3, "", "", "黑色沼泽：开启黑暗之门");

-- 80 五人本

-- 71-74 level range
DELETE FROM `disables` WHERE `entry` IN (574, 576, 600, 601, 619);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 574, 3, "", "", "Utgarde Keep"),
(2, 576, 3, "", "", "The Nexus"),
(2, 600, 3, "", "", "Drak’Tharon Keep"),
(2, 601, 3, "", "", "Azjol-Nerub"),
(2, 619, 3, "", "", "Ahn’kahet: The Old Kingdom");

-- 75-79 level range
DELETE FROM `disables` WHERE `entry` IN (575, 578, 595, 599, 602, 604, 608);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 575, 3, "", "", "Utgarde Pinnacle"),
(2, 578, 3, "", "", "The Oculus"),
(2, 595, 3, "", "", "The Culling of Stratholme"),
(2, 599, 3, "", "", "Halls of Stone"),
(2, 602, 3, "", "", "Halls of Lightning"),
(2, 604, 3, "", "", "Gundrak"),
(2, 608, 3, "", "", "Violet Hold");



-- 80 level range 团本
DELETE FROM `disables` WHERE `entry` IN (249, 533, 603, 615, 616, 624, 631, 632, 649, 650, 658, 668, 724, 13276);
INSERT INTO `disables` (`sourceType`, `entry`, `flags`, `params_0`, `params_1`, `comment`) VALUES 
(2, 249, 3, "", "", "Onyxia Lair"), -- 奥妮克希亚
(4, 13276, 0, "", "", "Onyxia Lair LK Statistic"),
(2, 533, 3, "", "", "Naxxramas"), -- 纳克萨玛斯 
(2, 603, 3, "", "", "Ulduar"), -- 奥杜尔
(2, 615, 3, "", "", "The Obsidian Sanctum"), -- 黑曜石圣殿
(2, 616, 3, "", "", "The Eye of Eternity"), -- 永恒之眼 
(2, 624, 3, "", "", "Vault of Archavon"), -- 阿尔卡冯的宝库
(2, 631, 15, "", "", "Icecrown Citadel"),-- 冰冠堡垒
(2, 632, 3, "", "", "The Forge of Souls"),  -- 新三本（灵魂\矿坑\映像大厅，冠军的试炼）
(2, 649, 15, "", "", "Trial of The Crusader"), -- 十字军的试炼
(2, 650, 3, "", "", "Trial of the Champion"),  -- 新三本（灵魂\矿坑\映像大厅，冠军的试炼）
(2, 658, 3, "", "", "Pit of Saron"),  -- 新三本（灵魂\矿坑\映像大厅，冠军的试炼）
(2, 668, 3, "", "", "Halls of Reflection"),  -- 新三本（灵魂\矿坑\映像大厅，冠军的试炼）
(2, 724, 15, "", "", "The Ruby Sanctum"); -- 红玉圣殿




-- 关闭野外boss
update creature set spawnMask=0 where id1 in (14890,14888,14887,14889,6109);
update creature set spawnMask=0 where id1 in (12397);
update creature set spawnMask=0 where id1 in (17711,18728);
-- 取消自动刷新事件
delete from game_event_creature where eventEntry=100;


-- 关闭荣誉装兑换
-- 根据不同阶段，禁止掉一些荣誉兑换装备的NPC 战场装/荣誉装/军装/竞技场装等

-- 42079
-- SELECT * FROM `npc_vendor` where item=42079 LIMIT 50;
update creature set spawnMask=0 where id1 in (34038,34075,34077,34059); #245装等，后面两个没有找到

-- 40807
-- SELECT * FROM `npc_vendor` where item=40807 LIMIT 50;
update creature set spawnMask=0 where id1 in (33915,33921,33926,33928,33934,33938,33941,34063,34084,35494,35495,35573,35574,40607); #232装等

-- 十字军试炼门口的兑换T9的NPC，还有LM方面的：35577,35579(本次给遗忘了，没有屏蔽这两个)
update creature set spawnMask=0 where id1 in (35580,35578); #232装等
update creature set spawnMask=0 where id1 in (35577,35579); #232装等

-- 213-226装等 征服纹章兑换 5件套和散件NPC
update creature set spawnMask=0 where id1 in (33963); #232装等
-- 232-277装等 凯旋 寒冰兑换NPC
update creature set spawnMask=0 where id1 in (35497,35500,35498,35496); #232-277装等
-- 珠宝商人 有251戒指兑换NPC
update creature set spawnMask=0 where id1 in (32172); #有251戒指

#31582 200装等 英雄纹章兑换 5件套和散件 P1作为5人80本纹章奖励
#31581 213装等 勇气纹章兑换 5件套和散件 P1作为5人80H本纹章奖励
#33963 213-226装等 征服纹章兑换 5件套和散件 P1要屏蔽掉
#35497,35500,35498,35496 232-277装等 凯旋 寒冰兑换 
#37688,37696 251-277装等  寒冰兑换 冰冠皇冠团本里的NPC，可以不屏蔽
#32172 珠宝商人 有251戒指

-- 51334
-- SELECT * FROM `npc_vendor` where item=51334 LIMIT 50;
update creature set spawnMask=0 where id1 in (34060,34078); #264装等

-- 51399
-- SELECT * FROM `npc_vendor` where item=51399 LIMIT 50;
update creature set spawnMask=0 where id1 in (34093,34094,34095); #277装等竞技场装备

-- 40847
-- SELECT * FROM `npc_vendor` where item=40847 LIMIT 50;
update creature set spawnMask=0 where id1 in (33915,33921,33926,33928,33934,33938,33941,34063,34084,35494,35495,35573,35574,40607); #232装等竞技场装备（6赛季）

-- 40812
-- SELECT * FROM `npc_vendor` where item=40812 LIMIT 50;
update creature set spawnMask=0 where id1 in (33924,33927,33933,33935,33937,37941,37942,38858); #251装等竞技场装备（7赛季）

-- 51397
-- SELECT * FROM `npc_vendor` where item=51397 LIMIT 50;
update creature set spawnMask=0 where id1 in (33936,33939,33940); #264装等竞技场装备



-- 副本cd调整 ，zug 1天CD
update mapdifficulty_dbc set RaidDuration=86400 where MapID=309;#zug 原来是259200
-- 副本cd调整 ，废墟 1天CD
update mapdifficulty_dbc set RaidDuration=86400 where MapID=509;#废墟 原来是259200
-- 副本cd调整 ，MC 1天CD
update mapdifficulty_dbc set RaidDuration=86400 where MapID=409;#MC 原来是604800
-- 副本cd调整 ，BWL 2天CD
update mapdifficulty_dbc set RaidDuration=172800 where MapID=469;#BWL 原来是604800
-- 副本cd调整 ，AQL 2天CD
update mapdifficulty_dbc set RaidDuration=172800 where MapID=531;#AQL 原来是604800
-- 副本cd调整 ，卡拉赞，格鲁尔，玛瑟里顿团本 2天CD
insert IGNORE into mapdifficulty_dbc select * from mapdifficulty_db_12340 where MapID IN (532, 565,544);
update mapdifficulty_dbc set RaidDuration=172800 where MapID IN (532, 565,544);#原来是 604800
-- 副本cd调整 ，毒蛇 风暴要塞 祖阿曼团本 2天CD
insert IGNORE into mapdifficulty_dbc select * from mapdifficulty_db_12340 where MapID IN (550, 548,568);
update mapdifficulty_dbc set RaidDuration=172800 where MapID IN (550, 548,568);#568原来是259200、550, 548原来是604800
-- 副本cd调整 ，毒蛇 风暴要塞 祖阿曼团本 2天CD
insert IGNORE into mapdifficulty_dbc select * from mapdifficulty_db_12340 where MapID IN (534, 564,580);
update mapdifficulty_dbc set RaidDuration=172800 where MapID IN (534, 564,580);#原来是604800
-- 副本cd调整 ，纳克萨玛斯 永恒之眼 黑曜石圣殿 奥杜尔 十字军的试炼 奥妮克希亚团本 2天CD
insert IGNORE into mapdifficulty_dbc select * from mapdifficulty_db_12340 where MapID IN (533,616,615,603,649,249);
update mapdifficulty_dbc set RaidDuration=172800 where MapID IN (533,616,615,603,649,249);#原来是604800


-- 临时增加探索：奎尔丹纳斯岛成就菜单（解决在wowpatch=0时无法完成第一次转生）
delete from gossip_menu_option where MenuID=71033 and OptionID=9;
INSERT INTO `gossip_menu_option` VALUES ('71033', '9', '5', '临时:探索奎尔丹纳斯岛', '0', '1', '1', '0', '0', '0', '0', '', '0', '0', '.achievement add 868');