# SQL Security Gate — TARKENSO

Обязательный контроль SQL-конструкций в GitHub Actions. Проверяются все
файлы Pawn в gamemodes/ (включая ветки #if) и миграции db/**/*.sql.

## Запуск

    python3 -m unittest discover -s tests -p 'test_sql_security.py' -v
    python3 tools/check_sql_security.py --output build/sql_security_report.json

Job SQL Security Gate (sql_security) выполняется до Compile через needs.
Ошибки блокируют компиляцию; отчёт JSON выгружается как sql-security-report.

## Политика формирования SQL

- Строковые значения: mysql_format(..., "SELECT ... WHERE name='%e'", name)
- Числовые значения: mysql_format(..., "SELECT ... WHERE id=%d", id)
- Статические SQL-запросы: mysql_query(handle, "SELECT 1")
- HEX поля TextDraw Studio: TDS_Hex / TDSDB_QueryHex + экранирование
  mysql_format(..., "CALL save(X'%e')", hex_value)

Нельзя применять format() к SQL-шаблонам, использовать %s в mysql_format,
подставлять %e без кавычек, передавать пользовательский inputtext в
mysql_query / mysql_tquery, собирать SQL через непроверенную runtime-
конкатенацию и использовать динамический шаблон mysql_format.

Миграции не должны содержать PREPARE ... FROM или EXECUTE IMMEDIATE.

## Коды

| Код | Значение |
| --- | --- |
| SQL000 | Отсутствуют миграции или слишком мало SQL-точек |
| SQL001 | Динамический SQL-шаблон в mysql_format |
| SQL002 | %s или неизвестный спецификатор |
| SQL003 | %e без SQL-кавычек |
| SQL004 | Количество аргументов и спецификаторов не совпадает |
| SQL005 | Формирование SQL через format() |
| SQL006 | Недоверенный или вычисляемый аргумент выполнения SQL |
| SQL007 | Runtime-конкатенация в исполняемый SQL-буфер |
| SQL008 | Динамический SQL в миграции |

## Ограничения и меры безопасности

Анализатор — статическая эвристика, а не полноценный парсер Pawn/SQL или
taint-analysis. PASS не доказывает отсутствия всех SQL-инъекций. Нужны
дополнительные динамические тесты на изолированной БД, ограничение прав
пользователя MySQL, code review и проверка кодировки/длины запросов.

Для блокировки merge в main **также** настройте GitHub Rulesets или Branch
Protection: required status check SQL Security Gate. Без правила GitHub
может разрешить merge, хотя Compile зависит от SQL Security Gate.
