SET FOREIGN_KEY_CHECKS=0;

-- ----------------------------
-- Table structure for `auctionhousebot`
-- ----------------------------
DROP TABLE IF EXISTS `auctionhousebot`;
CREATE TABLE `auctionhousebot` (
  `item` int(11) unsigned NOT NULL COMMENT 'Item Id',
  `stack` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT 'Stack Size',
  `bid` int(11) unsigned NOT NULL DEFAULT '1' COMMENT 'Bid Price',
  `buyout` int(11) unsigned NOT NULL DEFAULT '1' COMMENT 'Buyout Price'
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of auctionhousebot
-- ----------------------------
INSERT INTO `auctionhousebot` VALUES ('11370', '10', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('8167', '20', '100000', '150000');
INSERT INTO `auctionhousebot` VALUES ('8170', '10', '50000', '80000');
INSERT INTO `auctionhousebot` VALUES ('2589', '20', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('2592', '20', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('4306', '20', '9600', '12000');
INSERT INTO `auctionhousebot` VALUES ('4338', '20', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('14047', '20', '20000', '25000');
INSERT INTO `auctionhousebot` VALUES ('2318', '10', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('2319', '10', '5600', '7000');
INSERT INTO `auctionhousebot` VALUES ('4234', '10', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('4304', '10', '12000', '15000');
INSERT INTO `auctionhousebot` VALUES ('2881', '1', '12000', '15000');
INSERT INTO `auctionhousebot` VALUES ('2835', '20', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('2836', '20', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('2838', '20', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('7912', '20', '12000', '15000');
INSERT INTO `auctionhousebot` VALUES ('12365', '20', '12000', '15000');
INSERT INTO `auctionhousebot` VALUES ('2770', '20', '10000', '20000');
INSERT INTO `auctionhousebot` VALUES ('2775', '1', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('2771', '20', '800', '1000');
INSERT INTO `auctionhousebot` VALUES ('2776', '1', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('2772', '20', '38400', '48000');
INSERT INTO `auctionhousebot` VALUES ('7911', '1', '32000', '40000');
INSERT INTO `auctionhousebot` VALUES ('3858', '20', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('10620', '20', '24000', '30000');
INSERT INTO `auctionhousebot` VALUES ('765', '20', '640', '800');
INSERT INTO `auctionhousebot` VALUES ('2447', '20', '640', '800');
INSERT INTO `auctionhousebot` VALUES ('785', '20', '1280', '1600');
INSERT INTO `auctionhousebot` VALUES ('2449', '20', '1280', '1600');
INSERT INTO `auctionhousebot` VALUES ('2452', '20', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('2450', '20', '1600', '2000');
INSERT INTO `auctionhousebot` VALUES ('2453', '20', '1600', '2000');
INSERT INTO `auctionhousebot` VALUES ('3820', '20', '6400', '8000');
INSERT INTO `auctionhousebot` VALUES ('3355', '20', '3200', '4000');
INSERT INTO `auctionhousebot` VALUES ('3369', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('3356', '20', '1920', '2400');
INSERT INTO `auctionhousebot` VALUES ('3357', '20', '4800', '6000');
INSERT INTO `auctionhousebot` VALUES ('3818', '20', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('3821', '20', '9600', '12000');
INSERT INTO `auctionhousebot` VALUES ('3358', '20', '11200', '14000');
INSERT INTO `auctionhousebot` VALUES ('3819', '20', '6400', '8000');
INSERT INTO `auctionhousebot` VALUES ('8153', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('4625', '20', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('8831', '20', '19200', '24000');
INSERT INTO `auctionhousebot` VALUES ('8836', '20', '6080', '7600');
INSERT INTO `auctionhousebot` VALUES ('8838', '20', '3840', '4800');
INSERT INTO `auctionhousebot` VALUES ('8839', '20', '24000', '30000');
INSERT INTO `auctionhousebot` VALUES ('8845', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('8846', '20', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('13463', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('13464', '20', '6400', '8000');
INSERT INTO `auctionhousebot` VALUES ('13465', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('13466', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('13467', '20', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('13468', '1', '800000', '1000000');
INSERT INTO `auctionhousebot` VALUES ('6338', '1', '12000', '15000');
INSERT INTO `auctionhousebot` VALUES ('11128', '1', '24000', '30000');
INSERT INTO `auctionhousebot` VALUES ('11144', '1', '120000', '150000');
INSERT INTO `auctionhousebot` VALUES ('16206', '1', '800000', '1000000');
INSERT INTO `auctionhousebot` VALUES ('10978', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('11084', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('11138', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('11139', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('11177', '1', '56000', '70000');
INSERT INTO `auctionhousebot` VALUES ('11178', '1', '64000', '80000');
INSERT INTO `auctionhousebot` VALUES ('14343', '1', '72000', '90000');
INSERT INTO `auctionhousebot` VALUES ('14344', '1', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('20725', '1', '800000', '1000000');
INSERT INTO `auctionhousebot` VALUES ('10938', '10', '20000', '25000');
INSERT INTO `auctionhousebot` VALUES ('10998', '10', '24000', '30000');
INSERT INTO `auctionhousebot` VALUES ('11134', '10', '28000', '35000');
INSERT INTO `auctionhousebot` VALUES ('11174', '10', '36000', '45000');
INSERT INTO `auctionhousebot` VALUES ('16202', '10', '48000', '60000');
INSERT INTO `auctionhousebot` VALUES ('10940', '20', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('11083', '20', '60000', '75000');
INSERT INTO `auctionhousebot` VALUES ('11137', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('11176', '20', '120000', '150000');
INSERT INTO `auctionhousebot` VALUES ('9262', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('12363', '1', '800000', '1000000');
INSERT INTO `auctionhousebot` VALUES ('8171', '1', '240000', '300000');
INSERT INTO `auctionhousebot` VALUES ('783', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('4232', '1', '16000', '20000');
INSERT INTO `auctionhousebot` VALUES ('4235', '1', '24000', '30000');
INSERT INTO `auctionhousebot` VALUES ('8169', '1', '32000', '40000');
INSERT INTO `auctionhousebot` VALUES ('15417', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('774', '1', '160', '200');
INSERT INTO `auctionhousebot` VALUES ('818', '1', '240', '300');
INSERT INTO `auctionhousebot` VALUES ('1210', '1', '320', '400');
INSERT INTO `auctionhousebot` VALUES ('5498', '1', '160', '200');
INSERT INTO `auctionhousebot` VALUES ('1206', '1', '400', '500');
INSERT INTO `auctionhousebot` VALUES ('1705', '1', '480', '600');
INSERT INTO `auctionhousebot` VALUES ('5500', '1', '800', '1000');
INSERT INTO `auctionhousebot` VALUES ('3864', '1', '800', '1000');
INSERT INTO `auctionhousebot` VALUES ('1529', '1', '1600', '2000');
INSERT INTO `auctionhousebot` VALUES ('7909', '1', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('7075', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('12361', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('7910', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('7077', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('7076', '1', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('12799', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('12364', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('7080', '1', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('7191', '1', '400000', '500000');
INSERT INTO `auctionhousebot` VALUES ('7067', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('7069', '1', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('7070', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('7082', '1', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('12803', '1', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('7078', '1', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('7068', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('12808', '1', '8000', '10000');
INSERT INTO `auctionhousebot` VALUES ('7972', '1', '4000', '5000');
INSERT INTO `auctionhousebot` VALUES ('7971', '1', '40000', '50000');
INSERT INTO `auctionhousebot` VALUES ('13926', '1', '800000', '1000000');
INSERT INTO `auctionhousebot` VALUES ('12811', '1', '800000', '1000000');
INSERT INTO `auctionhousebot` VALUES ('6358', '20', '160000', '200000');
INSERT INTO `auctionhousebot` VALUES ('6359', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('13422', '20', '240000', '300000');
INSERT INTO `auctionhousebot` VALUES ('6522', '20', '80000', '100000');
INSERT INTO `auctionhousebot` VALUES ('10286', '20', '50000', '80000');
INSERT INTO `auctionhousebot` VALUES ('19183', '50', '500000', '500000');
