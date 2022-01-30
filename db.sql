CREATE TABLE `risultati` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ip_address` tinytext DEFAULT NULL,
  `dl` double unsigned DEFAULT NULL,
  `ul` double DEFAULT NULL,
  `dataeora` datetime DEFAULT current_timestamp(),
  PRIMARY KEY (`id`)
)
