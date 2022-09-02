-- 建立webserver库
create database webserver;

-- 创建user表
USE webserver;
CREATE TABLE user(
    username char(50) NULL,
    passwd char(50) NULL
)ENGINE=InnoDB;

-- 添加数据
INSERT INTO user(username, passwd) VALUES('xt', '123456');