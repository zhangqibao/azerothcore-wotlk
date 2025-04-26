DROP TABLE IF EXISTS `playerbots_arena_team_names`;
CREATE TABLE `playerbots_arena_team_names` (
                                               `name_id` mediumint(8) NOT NULL AUTO_INCREMENT UNIQUE,
                                               `name` varchar(24) NOT NULL UNIQUE,
                                               `type` TINYINT(3) NOT NULL,
                                               PRIMARY KEY (`name_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=FIXED COMMENT='Playerbot arena team names';


DELETE FROM `playerbots_arena_team_names`;
INSERT INTO `playerbots_arena_team_names` (`name_id`,`name`, `type`) VALUES
(NULL, '觉醒战队',2),
(NULL, '神族战队',2),
(NULL, '不败战队',2),
(NULL, '神族',3),
(NULL, '电锯战队',3),
(NULL, '精英部队',3),
(NULL, '梦之队',5),
(NULL, '逆袭',5),
(NULL, '暗影战队',5),
(NULL, '弑神战队',5),
(NULL, '坑神大队',5),
(NULL, '金色战队',5),
(NULL, '极炫战队',5),
(NULL, '娱乐战队',5),
(NULL, '高富帅战队',5),
(NULL, '瀚星云战队',5),
(NULL, '切菜战队',5);
