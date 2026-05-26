-- Day3: 往 messages 表插入 10w+ 条假数据
-- 依赖：chat_db 已建库，messages 表已存在
-- 用法：mysql -u root chat_db < day3_data_gen.sql

USE chat_db;

-- 先插入 10 个测试用户（避免 sender_id / receiver_id 无意义）
INSERT IGNORE INTO users (id, username, password_hash, nickname) VALUES
  (1,  'user001', 'hash001', 'Alice'),
  (2,  'user002', 'hash002', 'Bob'),
  (3,  'user003', 'hash003', 'Carol'),
  (4,  'user004', 'hash004', 'Dave'),
  (5,  'user005', 'hash005', 'Eve'),
  (6,  'user006', 'hash006', 'Frank'),
  (7,  'user007', 'hash007', 'Grace'),
  (8,  'user008', 'hash008', 'Heidi'),
  (9,  'user009', 'hash009', 'Ivan'),
  (10, 'user010', 'hash010', 'Judy');

-- 用存储过程批量插入 10w 条 messages
DROP PROCEDURE IF EXISTS gen_messages;

DELIMITER $$

CREATE PROCEDURE gen_messages()
BEGIN
  DECLARE i INT DEFAULT 0;
  DECLARE sid INT;
  DECLARE rid INT;
  DECLARE ts  DATETIME;

  -- 关闭 autocommit，每 5000 条提交一次，速度提升 10x
  SET autocommit = 0;

  WHILE i < 100000 DO
    -- sender_id / receiver_id 随机取 1~10，确保不同
    SET sid = 1 + (i MOD 10);
    SET rid = 1 + ((i + 3) MOD 10);
    IF rid = sid THEN
      SET rid = 1 + ((sid MOD 10));
    END IF;

    -- created_at 均匀分布在过去 365 天
    SET ts = DATE_SUB(NOW(), INTERVAL FLOOR(RAND() * 525600) MINUTE);

    INSERT INTO messages (sender_id, receiver_id, content, created_at)
    VALUES (sid, rid, CONCAT('msg_content_', i, '_', CONV(FLOOR(RAND()*1000000), 10, 36)), ts);

    SET i = i + 1;

    IF i MOD 5000 = 0 THEN
      COMMIT;
    END IF;
  END WHILE;

  COMMIT;
  SET autocommit = 1;
END$$

DELIMITER ;

CALL gen_messages();
DROP PROCEDURE IF EXISTS gen_messages;

SELECT COUNT(*) AS total_messages FROM messages;
