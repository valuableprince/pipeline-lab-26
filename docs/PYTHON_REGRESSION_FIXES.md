# Исправление полного регрессионного набора

Проверено 27.10.2026. База — `bfde01d73b8f3a9d1af349344561d29b8e8dd1da`,
ветка этапа 3.3 `feat/tdstudio-history-templates-stage33`, PR #76.
Исправления находятся в `fix/full-regression-suite-stage33`.

## Результат

| Проверка | До | После |
| --- | --- | --- |
| `python3 -m unittest discover -s tests -v` | 256 тестов, 9 падений, 24 ошибки | **353 теста, 0 падений, 0 ошибок, 0 пропусков** |
| Тот же набор с `python3 -S` | не запускался | **353 теста, 0 падений, 0 ошибок**; без site-packages |
| `python3 -m compileall -q tools tests` | — | успешно |
| Реальный Pawn-harness TDS | 926 проверок на базе этапа 3.3 | **926 проверок, 0 провалов** |
| Полная сборка MySQL-primary мода | успешна на базе этапа 3.3 | **0 ошибок / 0 предупреждений**, реальный AMX |
| `git diff --check` | — | успешно |

Рост числа Python-тестов связан с восстановлением импортов: 23 заглушки
`_FailedTest` заменились 119 настоящими тестами (+96). Добавлен один тест обоих
staging-путей, итого 353. Тесты не удалены, `skip`/`expectedFailure` не добавлены.

Использован официальный Pawn 3.10.11 из open.mp SDK 1.5.8.3079. Повторная
сборка `build/compiled.amx` содержит **2 262 579 байт**; SHA-256:
`93aeaa5eea385446b28db62c2cbbb8bf81ed8734598eb41a7a2458950f923c57`.
Компилятор действительно запущен; код возврата, warning gate и AMX-заголовок
проверены. Исходники мода и compiler flags этим исправлением не менялись.
Не следует сравнивать бинарники из случайных временных путей по одному размеру:
sscanf включает `__file` в диагностические строки.

## Причины девяти исходных падений

| Тест | Причина и исправление |
| --- | --- |
| `test_db_startup_diagnostics.test_failure_stages` | Ожидалась схема v4; текущий обязательный startup gate — v10. Проверяются реальные диагностические сообщения. |
| `test_orvelar_hud.test_three_parts_and_toggle` | Сборщик копирует `*.inc`, а тест искал явное имя файла в тексте сборщика. Проверяется фактически подготовленный модуль. |
| `test_race_map.test_build_stages_map_include` | Та же причина: теперь сравниваются реальные staged bytes в CI и Windows. |
| `test_race_map.test_world_and_lifecycle` | Требовалась смежность include-строк; между ними законно появился radio_player.inc. Сохранена проверка уникальности и порядка зависимостей. |
| `test_race_progress.test_commands_and_encoding` | Устранена зависимость от явного списка includes и старого имени workflow. |
| `test_scaffold.test_local_config_is_ignored` | Реальный пробел в `.gitignore`: добавлены `config.json` и `mysql.ini`, примеры остаются доступны Git. |
| `test_scaffold.test_pawn_entrypoint_exists` | Пролог выделен в story_prologue.inc. Проверяются include, обработчик и русская строка в этом модуле. |
| `test_start_region.test_file_and_mysql_persistence` | SELECT расширен полями персонажа/PayDay/сюжета. Проверяется наличие start_region и row_version отдельно, чтение и присваивание региона. |
| `test_username_case.test_canonical_key` | Миграция v4 остаётся проверяемой, startup gate проверяется для актуальной v10. |

Полные исходные ID сохранены в [отчёте этапа 3.3](TEXTDRAW_STUDIO_STAGE33.md).

## Причины 24 ошибок

- 23 модуля тестов не импортировались из-за отсутствующих документов,
  `.github/workflows/pawn-build.yml` и удалённого `npc_route_editor.inc`.
- Ещё один тест консоли пытался читать тот же удалённый NPC-модуль.
- Тесты используют существующий `.github/workflows/build.yml`. Устаревший
  workflow и NPC-редактор не возвращаются в проект.
- Восстановлены содержательные инструкции `FILE_AUTH.md`, `ADMIN_SYSTEM.md`,
  `LEADER_SYSTEM.md`, `COMMAND_PROCESSOR.md`, `RACE_AUDIO.md`, `RACE_EVENT.md`,
  `RACE_PLAYLIST.md`, `SERVER_GUARD.md`, `STREAMER.md`, описывающие текущий код
  и ручную приёмку. Проверки документации продолжают выполняться.

После восстановления импортов обнаружились дополнительные старые ожидания:
единственный контейнер вместо 24 контейнеров схемы v10, старые интервалы
визуализации мешков, модель в руке 1264 вместо 1265, глобальный чат вместо
локального RP_SendLocal, старый каталог команд, старые имена AMX-артефактов,
запрет любого PlayerTextDraw-callback в основном моде и запрет наблюдения
GetPlayerMoney даже для исправления клиентского HUD. Они также исправлены.

Защитные контракты сохранены: границы контейнеров/слотов, вместимость,
аудит и права основателя, CAS, START TRANSACTION/ROLLBACK/COMMIT, порядок
сохранения до выплаты, rollback ночной премии, server-owned balance,
единственная регистрация команд, маршрутизация кликов TDS и сортировки.
Убрана старая фиктивная проверка, дописывавшая ожидаемую строку к самому
проверяемому исходнику; вместо неё проверяется действующий расчёт и откат премии.

## Проверка сборочной инфраструктуры

`tests/build_contracts.py` подготавливает реальные исходники обоими путями:

1. Вызывает Windows `build()` с двойником компилятора, который читает его
   временный вход. Эта часть проверяет подготовку файлов, **не** Windows-компиляцию.
2. Исполняет Python-блок `Stage source` существующего CI verbatim в изолированной
   копии gamemodes, без сетевых обращений.
3. Сравнивает точные CP1251-байты всех 65 includes, включая вложенные TDS-модули,
   и entrypoint с TARKENSO_DB_PRIMARY. Новый `test_build_staging.py` проверяет
   полный набор файлов обоих сборщиков.

Многократные проверки отдельных подсистем используют один кеш staging на
запуск Python. Подготовленные файлы и двойник AMX удаляются с временным каталогом.
Полная компиляция и Pawn-runtime проверяются отдельно настоящими инструментами.

В `.github/workflows/build.yml` выбранные 29 Python-наборов заменены на единый
полный `unittest discover` и `compileall`. Старый warning gate, Pawn-harness,
сборка/артефакт и branch triggers сохранены. Изменение CI необходимо, чтобы
ошибки импортов в остальных тестах снова не оставались незамеченными.
GitHub Actions для PR в ветку этапа 3.3 не заявляется как выполненный: существующий
автоматический PR-trigger по-прежнему ограничен main.

## Воспроизведение и границы проверки

```sh
python3 -m compileall -q tools tests
python3 -m unittest discover -s tests -v
python3 -S -m unittest discover -s tests -v
python3 tools/test_tdstudio.py --server /path/to/openmp/Server
git diff --check
```

Основной мод собран с `#define TARKENSO_DB_PRIMARY`: entrypoint и все includes
конвертируются из UTF-8 в CP1251 в staging, затем вызывается официальный pawncc
и `tools/check_pawn_output.py`. На Windows штатная команда:
`python tools/build_windows.py --server D:/Server --output build`.

Локальные логи: `build/regression-fix/python-full.log`, `python-stdlib.log`,
`pawn/tdstudio-test-compile.log`, `pawn/tdstudio-test-run.log`, `build/compiler.log`.
Базовые 9/24 повторно воспроизведены перед правками и записаны в
`build/regression-fix/baseline.json`.

Файлы gamemodes, db, tools, зависимости, схема MySQL, игровые механики и
настройки хостинга не изменялись. Реальные MySQL/restart/export-проверки этапа 3.3
описаны в прежнем отчёте; в этом исправлении они не перезапускались, поскольку
код хранения и экспорта не менялся. GTA-клиент не подключался: Pawn-harness
исполняет настоящий Pawn на open.mp, но игроки, UI и ввод являются двойниками.
