#!/usr/bin/env python3
"""Read-only financial SQL privilege audit for the account used by mysql.ini.

This is a privilege-boundary check, not an authentication or transaction test.
It never runs DML, creates users, calls procedures, or prints credentials/grants.
The result does not certify safety of callable SQL SECURITY DEFINER routines.
"""
import json
import os
import re
import sys

GRANT_RE = re.compile(r"^GRANT\s+(.+?)\s+ON\s+(.+?)\s+TO\s+", re.I)
PRIV_RE = re.compile(r"^([A-Z_ ]+?)(?:\s*\(([^()]*)\))?$")
SAFE_SCHEMA = {"USAGE", "SELECT", "SHOW VIEW"}
SAFE_FINANCE = {"USAGE", "SELECT", "SHOW VIEW"}
FINANCE_PREFIXES = ("treasury_", "bond_", "government_bond_")
FINANCE_TABLES = {"gov_transfers", "gov_transfer_post"}
AUTHORITY_TABLES = {"faction_members", "factions", "admin_roles"}


def _split_privileges(raw):
    parts, start, depth = [], 0, 0
    for i, ch in enumerate(raw):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth < 0:
                return None
        elif ch == "," and depth == 0:
            parts.append(raw[start:i].strip())
            start = i + 1
    if depth:
        return None
    parts.append(raw[start:].strip())
    out = []
    for part in parts:
        m = PRIV_RE.fullmatch(part.upper())
        if not m:
            return None
        name, columns = m.groups()
        column_list = None if columns is None else {
            x.strip().strip("\x60\"").lower() for x in columns.split(",")
        }
        if columns is not None and not all(column_list):
            return None
        out.append((name.strip(), column_list))
    return out


def assess_grants(grants, database):
    """Return redacted findings; role grants fail closed (not expanded)."""
    violations, warnings = set(), set()
    db_name = database.lower()
    if not grants:
        violations.add("no grants returned")
    for grant in grants:
        line = grant.strip()
        m = GRANT_RE.search(line)
        if not m:
            violations.add("unrecognized grant or inherited role (manual review required)")
            continue
        parsed = _split_privileges(m.group(1))
        if parsed is None:
            violations.add("unrecognized privilege syntax")
            continue
        scope = m.group(2).strip()
        if "WITH GRANT OPTION" in line.upper():
            violations.add("GRANT OPTION can expand SQL authority")
        is_routine = scope.upper().startswith(("PROCEDURE ", "FUNCTION "))
        if is_routine:
            scope = scope.split(None, 1)[1]
        scope = scope.replace(chr(96), "").replace('"', "").lower()
        bits = scope.split(".")
        if len(bits) != 2:
            violations.add("unrecognized grant scope")
            continue
        schema, obj = bits
        global_scope = schema == "*" and obj == "*"
        schema_scope = obj == "*"
        target = global_scope or schema == db_name
        if not target:
            continue
        if global_scope or schema_scope:
            for name, _ in parsed:
                if name not in SAFE_SCHEMA:
                    violations.add("excessive global/schema privilege: " + name)
            continue
        if is_routine:
            for name, _ in parsed:
                if name != "EXECUTE" and name not in SAFE_FINANCE:
                    violations.add("excessive routine privilege: " + name)
                if name == "EXECUTE" and obj.startswith(
                        ("treasury_", "gov_", "bond_")):
                    warnings.add("financial DEFINER routine is callable: "
                                 "authorization remains a separate requirement")
            continue
        if obj.startswith(FINANCE_PREFIXES) or obj in FINANCE_TABLES:
            for name, _ in parsed:
                if name not in SAFE_FINANCE:
                    violations.add("direct finance table privilege: "
                                   + name + " on " + obj)
        elif obj in AUTHORITY_TABLES:
            for name, _ in parsed:
                if name not in SAFE_FINANCE:
                    violations.add("direct organization/authority privilege: "
                                   + name + " on " + obj)
        elif obj == "accounts":
            for name, columns in parsed:
                if name in {"INSERT", "DELETE", "ALTER", "DROP", "TRIGGER",
                            "CREATE", "REFERENCES", "INDEX"}:
                    violations.add("account table privilege can bypass money controls: "
                                   + name)
                elif name == "UPDATE":
                    if columns is None or "balance" in columns:
                        violations.add("direct accounts.balance UPDATE permitted")
                    else:
                        warnings.add("column-limited accounts UPDATE: review affected fields")
                elif name not in SAFE_FINANCE:
                    violations.add("unrecognized accounts privilege: " + name)
        else:
            # Other game subsystems still use DML; audit them separately.
            for name, _ in parsed:
                if name not in {"SELECT", "INSERT", "UPDATE", "DELETE", "USAGE",
                                "SHOW VIEW"}:
                    violations.add("unexpected privilege on game table: "
                                   + name + " on " + obj)
    warnings.add("CALL privileges and trusted Pawn SQL inputs need separate review")
    return {"status": "FAIL" if violations else "PASS_FINANCE_ACL_ONLY",
            "violations": sorted(violations), "warnings": sorted(warnings)}


def main():
    host = os.environ.get("GOV_ACL_DB_HOST", "127.0.0.1")
    port = int(os.environ.get("GOV_ACL_DB_PORT", "3306"))
    database = os.environ.get("GOV_ACL_DB_NAME", "")
    user = os.environ.get("GOV_ACL_DB_USER", "")
    password = os.environ.get("GOV_ACL_DB_PASSWORD")
    if not database or not user or password is None:
        print("GOV ACL: set GOV_ACL_DB_NAME/USER/PASSWORD (and optionally HOST/PORT)",
              file=sys.stderr)
        return 2
    try:
        import mysql.connector
        with mysql.connector.connect(
                host=host, port=port, user=user, password=password,
                database=database, connection_timeout=5) as db:
            with db.cursor() as cur:
                cur.execute("SELECT CURRENT_USER(), DATABASE()")
                principal, actual_db = cur.fetchone()
                if actual_db.lower() != database.lower():
                    raise RuntimeError("connection selected unexpected database")
                cur.execute("SHOW GRANTS")
                grants = [row[0] for row in cur.fetchall()]
        findings = assess_grants(grants, actual_db)
        # MariaDB may include password hashes in raw grant statements.
        print(json.dumps({"principal": principal, "database": actual_db,
                          **findings}, ensure_ascii=False, indent=2))
        return 1 if findings["violations"] else 0
    except Exception as exc:
        print("GOV ACL: cannot verify grants; fail closed (" +
              type(exc).__name__ + ")", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
