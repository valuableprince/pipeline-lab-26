"""GOV-05 Cyrillic roundtrip: CP1251 Pawn input and utf8mb4 stored text.

Run only against the disposable CI MariaDB database; no financial writes.
The actual plugin charset contract is checked statically in test_db_primary.py.
"""
import os
import mysql.connector

name = os.environ.get("GOV00_TEST_DB", "tarkenso_gov00")
assert name == "tarkenso_gov00", "Refuse non-disposable charset UAT"

cnx = mysql.connector.connect(
    host=os.environ.get("GOV00_TEST_HOST", "127.0.0.1"),
    port=int(os.environ.get("GOV00_TEST_PORT", "3306")),
    user=os.environ.get("GOV00_TEST_USER", "root"),
    password=os.environ["GOV00_TEST_PASSWORD"],
    database=name,
    charset="cp1251",
    autocommit=True,
)
try:
    title = "Тестовый выпуск Tarkenso"
    purpose = "Проверка покупки и купонов"
    with cnx.cursor() as cur:
        cur.execute("SELECT @@character_set_client, @@character_set_connection, @@character_set_results")
        assert cur.fetchone() == ("cp1251", "cp1251", "cp1251")
        cur.execute("SHOW FULL COLUMNS FROM government_bond_issues")
        columns = {row[0]: row[2] for row in cur.fetchall()}
        assert columns["title"].startswith("utf8mb4"), columns["title"]
        assert columns["purpose"].startswith("utf8mb4"), columns["purpose"]

        # Temporary table only; does not modify existing government bonds.
        cur.execute("CREATE TEMPORARY TABLE gov05_charset_roundtrip ("
                    "title VARCHAR(128), purpose VARCHAR(512)"
                    ") CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
        cur.execute("INSERT INTO gov05_charset_roundtrip(title,purpose) VALUES(%s,%s)",
                    (title, purpose))
        cur.execute("SELECT title,purpose,HEX(title),HEX(purpose) "
                    "FROM gov05_charset_roundtrip")
        stored_title, stored_purpose, title_hex, purpose_hex = cur.fetchone()
        assert stored_title == title, (stored_title, title)
        assert stored_purpose == purpose, (stored_purpose, purpose)
        assert title_hex == title.encode("utf-8").hex().upper()
        assert purpose_hex == purpose.encode("utf-8").hex().upper()
    print("GOV-05 charset UAT PASS: CP1251 MySQL client, UTF8MB4 storage, Cyrillic roundtrip")
finally:
    cnx.close()
