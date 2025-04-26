DROP TABLE IF EXISTS `playerbots_guild_names`;
CREATE TABLE `playerbots_guild_names` (
                                          `name_id` INT(11) NOT NULL AUTO_INCREMENT UNIQUE,
                                          `name` varchar(24) NOT NULL UNIQUE,
                                          PRIMARY KEY (`name_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 ROW_FORMAT=FIXED COMMENT='Playerbot guild names';

INSERT INTO `playerbots_guild_names` VALUES
(NULL, '黑手'),
(NULL, '永恒辉煌'),
(NULL, '傲天神域'),
(NULL, '雪域纵横'),
(NULL, '燃烧军团'),
(NULL, '红色自卫队'),
(NULL, '锦衣卫'),
(NULL, '基地组织'),
(NULL, '楼兰月影');

