-- upload_system.db kaybolur/silinirse tekrar calistir:
--   sqlite3 upload_system.db < seed.sql
INSERT OR REPLACE INTO users (Id, NameSurname, Password) VALUES (1, 'MERT GEZER', '1ABC3');
INSERT OR REPLACE INTO users (Id, NameSurname, Password) VALUES (2, 'UMUT KARAKAYA', 'CD1ABC3');
INSERT OR REPLACE INTO users (Id, NameSurname, Password) VALUES (3, 'LALENUR ALTINOZ', 'KE3733');
