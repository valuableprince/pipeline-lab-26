# DB-MIGRATION-01 — первичная схема MySQL/MariaDB

Схема предназначена для будущего единого источника истины TARKENSO. В ней предусмотрены игроки, bcrypt-хеши (без пересчёта), серверный баланс, Mute, медицинские параметры, прогресс гонок и уборщика, админправа, баны, составы организаций и глобальные данные кварталов/топ-10.

**Важно:** эта ветка НЕ меняет Pawn runtime. Пока не завершены тесты БД и обратная совместимость, сервер читает и пишет старые `scriptfiles`. Никакого параллельного двойного начисления денег не допускается.

## Запуск

1. Полностью остановить игровой сервер и сделать побайтовый архив папки `scriptfiles` (включая `.bak`, `.tmp`, .adm и .ban). Применять перенос на неподвижной копии.
2. Создать **пустую** БД MySQL 8 / MariaDB 10.6+ в UTF8MB4 и отдельного пользователя с минимальными правами. Применить `db/migrations/001_initial.sql` к пустой БД **один раз**, затем `db/migrations/002_start_region.sql` (в том числе при обновлении работающей схемы v1, заранее сделав резервную копию).
3. Запустить проверку `python -m tools.migrate_legacy --scriptfiles PATH`. Она не обращается к БД, не показывает bcrypt-хеши и не меняет файлы.
4. Установить совместимый драйвер вне репозитория: `python -m pip install mysql-connector-python`. Задать `TARKENSO_DB_HOST`, `TARKENSO_DB_PORT` (необязательно), `TARKENSO_DB_NAME`, `TARKENSO_DB_USER`, `TARKENSO_DB_PASSWORD`. Текстовые файлы с паролем **не коммитить**.
5. Выполнить `python -m tools.migrate_legacy --scriptfiles PATH --apply --confirm EMPTY_DATABASE`. Только явное `--apply` выполняет запись. Импорт в непустую БД запрещён. Данные сохраняются одним транзакционным commit; при любой ошибке выполняется rollback.
6. Сверить числа строк и отдельные контрольные суммы архивной копии; затем отдельно проверить Pawn MySQL-адаптер на тестовом сервере. **Не менять prod config.json до приёмки.**

### Ограничения

* Весь импорт — перенос **снимка**, не live-replication. Изменения после снятия копии не попадут в БД.
* Записи организаций с ID 1–3 импортируются как активные, ID 4 — как зарезервированная отключённая организация, даже если файла состава ещё нет.
* Система не переносит историю `ledger.log`, репорты, административные audit.log, старые чаты, объекты карты и прочие эфемерные данные. Хранить файловый архив отдельно.
* При наличии незавершённых `.tmp`, повреждённых файлов или конфликтующих членов организаций импорт **прерывается**, исправление вручную с аудитом первоисточника.
* Файлы аккаунтов `SIGNAL0317|1..5` сохраняют bcrypt-хеш без изменения. Для исторических `v1/v2` установка маркера выплат уборщика из `.clean` только фиксирует уже заработанную историю — не выдаёт деньги заново.
* `.ban` может быть выдан на ник без аккаунта, поэтому у таблицы bans нет внешнего ключа на accounts.
* Для соответствия реальной экономике защита дублей выплат должна переехать в транзакции сервера и использовать уникальные event-id. Этот SQL и импортер не включают переключение игрового хранения.
* Доступ и TLS к конкретному хостингу проверяются перед подключением; TLS-опции будущего Pawn-компонента и возможности MySQL провайдера требуется согласовать.

## TextDraw Studio, migration 012

Apply `migrations/012_tdstudio_projects.sql` once after 011, using MySQL 8.0.16+ and a migration account with CREATE ROUTINE. Runtime uses the existing R41 connection, EXECUTE on the four `tds_*` routines and CRUD on the two new tables. The routine security model is INVOKER. No existing game table is altered. See `docs/TEXTDRAW_STUDIO_STAGE2.md` for CP1251 transport, installation and testing.

TextDraw Studio stage 3.1 adds `migrations/015_tdstudio_groups.sql` after 012 (013/014 are reserved for LoadTest/LoadNet in current main). Apply once with the migration account, then grant the existing runtime user CRUD on `tdstudio_groups`/`tdstudio_group_members` and EXECUTE on `tds_save_v2`/`tds_load_v2`. It adds project-scoped groups, membership and `next_group_id`; format-1 records and original routines are preserved. No dependency on LoadTest/LoadNet migrations is introduced. See `docs/TEXTDRAW_STUDIO_STAGE31.md` for installation, compatibility, real MySQL checks and manual GTA acceptance.
