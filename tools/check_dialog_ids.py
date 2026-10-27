#!/usr/bin/env python3
"""Audit Pawn dialog IDs, including statically forwarded wrapper parameters.

This is a conservative source check, not a Pawn preprocessor. All branches are
inspected. Unresolved/computed IDs fail the check instead of being ignored.
Only dialog uses are registered: unrelated numeric constants and unused range
markers (e.g. TDS_FIRST_DIALOG) are not separate windows.
"""
from __future__ import annotations

import argparse
import ast
from dataclasses import dataclass
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
IDENT = r"[A-Za-z_]\w*"
OPAQUE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/', re.S)
DEFINE = re.compile(r"(?m)^[ \t]*#define[ \t]+(" + IDENT + r")((?:\\\r?\n|[^\n])*)")
CONST = re.compile(r"\bconst\s+(?:" + IDENT + r":)?(" + IDENT + r")\s*=\s*([^;\n]+);")
CALL = re.compile(r"\b(" + IDENT + r")\s*\(")
FUNCTION = re.compile(r"^[ \t]*(?:(?:stock|public|static)\s+)*(?:" + IDENT + r":)?$")
CONTROL = {"if", "for", "while", "switch", "sizeof", "defined"}


@dataclass(frozen=True)
class Definition:
    name: str
    expression: str
    path: str
    line: int


@dataclass(frozen=True)
class Value:
    number: int
    owner: str
    label: str
    location: str


@dataclass
class Function:
    name: str
    parameters: list[str]
    start: int
    end: int


@dataclass
class Call:
    name: str
    arguments: list[str]
    path: str
    line: int
    parent: Function | None


@dataclass
class Report:
    dialogs: dict[int, dict[str, Value]]
    errors: list[str]
    calls: int
    wrappers: list[str]


def mask(text: str) -> str:
    return OPAQUE.sub(lambda m: re.sub(r"[^\n]", " ", m[0]), text)


def split_arguments(text: str) -> list[str]:
    parts, start, depth = [], 0, 0
    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    parts.append(text[start:].strip())
    return parts


def delimiters(text: str) -> dict[int, int]:
    pairs, stack = {}, []
    matching = {")": "(", "}": "{"}
    for i, ch in enumerate(text):
        if ch in "({":
            stack.append((ch, i))
        elif ch in ")}":
            if stack and stack[-1][0] == matching[ch]:
                _, start = stack.pop()
                pairs[start] = i
    return pairs


def sources(text: str, path: str):
    """Preserve offsets/lines while ignoring strings, comments and directives."""
    clean = mask(text)
    definitions = []
    for match in DEFINE.finditer(clean):
        # NAME(args) is a function-like macro; NAME (expr) is an object macro.
        if not match[2].startswith("("):
            expression = re.sub(r"\\\r?\n", " ", match[2]).strip()
            definitions.append(Definition(match[1], expression, path, clean.count("\n", 0, match.start()) + 1))
    for match in CONST.finditer(clean):
        definitions.append(Definition(match[1], match[2], path, clean.count("\n", 0, match.start()) + 1))
    for match in re.finditer(r"\benum(?:\s+" + IDENT + r")?\s*\{([^}]+)\}", clean):
        previous = None
        for item in split_arguments(match[1]):
            field = re.fullmatch(r"\s*(?:" + IDENT + r":)?(" + IDENT + r")(?:\s*=\s*(.+))?\s*", item)
            if not field:
                # Array-field and custom-step enums require a compiler. If used
                # as a dialog ID, resolution will fail explicitly below.
                break
            expression = field[2] or (previous + " + 1" if previous else "0")
            at = clean.find(item, match.start(1))
            definitions.append(Definition(field[1], expression, path, clean.count("\n", 0, at) + 1))
            previous = field[1]
    code = re.sub(r"(?m)^[ \t]*#(?:\\\r?\n|[^\n])*", lambda m: re.sub(r"[^\n]", " ", m[0]), clean)
    pairs = delimiters(code)
    functions, calls = [], []
    matches = list(CALL.finditer(code))
    declared = set()
    for match in matches:
        opening = code.index("(", match.start(), match.end())
        end = pairs.get(opening)
        if end is None:
            continue
        following = end + 1
        while following < len(code) and code[following].isspace():
            following += 1
        prefix = code[code.rfind("\n", 0, match.start()) + 1:match.start()]
        if following < len(code) and code[following] == "{" and FUNCTION.fullmatch(prefix) and match[1] not in CONTROL:
            parameters = []
            for argument in split_arguments(code[opening + 1:end]):
                parameter = re.match(r"(?:const\s+)?&?\s*(?:" + IDENT + r":)?(" + IDENT + r")", argument)
                if argument:
                    parameters.append(parameter[1] if parameter else "")
            functions.append(Function(match[1], parameters, following, pairs.get(following, len(code))))
            declared.add(match.start())
        elif re.search(r"\b(?:native|forward)\s+(?:" + IDENT + r":)?$", prefix):
            declared.add(match.start())
    for match in matches:
        if match.start() in declared:
            continue
        opening = code.index("(", match.start(), match.end())
        if opening not in pairs:
            continue
        parent = next((f for f in functions if f.start < match.start() < f.end), None)
        calls.append(Call(match[1], split_arguments(code[opening + 1:pairs[opening]]), path,
                          code.count("\n", 0, match.start()) + 1, parent))
    return definitions, calls


def unparen(expression: str) -> str:
    expression = expression.strip()
    while expression.startswith("(") and delimiters(expression).get(0) == len(expression) - 1:
        expression = expression[1:-1].strip()
    return re.sub(r"^_:\s*", "", expression)


def alternatives(expression: str) -> list[str]:
    expression = unparen(expression)
    depth, question, nested = 0, None, 0
    for i, ch in enumerate(expression):
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif depth == 0 and ch == "?":
            if question is None:
                question = i
            else:
                nested += 1
        elif depth == 0 and ch == ":" and question is not None:
            if nested:
                nested -= 1
            else:
                return alternatives(expression[question + 1:i]) + alternatives(expression[i + 1:])
    return [expression]


class Resolver:
    def __init__(self, definitions: list[Definition]):
        self.definitions: dict[str, list[Definition]] = {}
        for definition in definitions:
            self.definitions.setdefault(definition.name, []).append(definition)

    def resolve(self, expression: str, path: str, line: int, trail=()) -> list[Value]:
        if len(trail) > 64:
            raise ValueError("constant nesting exceeds 64")
        variants = alternatives(expression)
        if len(variants) > 1:
            if len(variants) > 128:
                raise ValueError("too many dialog alternatives")
            return [v for arm in variants for v in self.resolve(arm, path, line, trail)]
        expression = variants[0]
        if expression in self.definitions:
            if expression in trail:
                raise ValueError("cyclic constant: " + " -> ".join((*trail, expression)))
            entries = self.definitions[expression]
            if len(entries) != 1:
                raise ValueError(f"ambiguous definition of {expression}")
            d = entries[0]
            values = self.resolve(d.expression, d.path, d.line, (*trail, expression))
            if re.fullmatch(IDENT, unparen(d.expression)):
                return values  # explicit alias of the same logical window
            return [Value(v.number, f"{d.path}:{d.name}", d.name, f"{d.path}:{d.line}") for v in values]
        if expression == "INVALID_DIALOG_ID":
            return [Value(-1, expression, expression, f"{path}:{line}")]
        if re.fullmatch(IDENT, expression):
            raise ValueError(f"unknown dialog constant {expression}")
        try:
            tree = ast.parse(expression, mode="eval")
        except SyntaxError as exc:
            raise ValueError(f"unsupported ID expression {expression!r}") from exc
        if sum(1 for _ in ast.walk(tree)) > 128:
            raise ValueError("ID expression too complex")
        def integer(node):
            if isinstance(node, ast.Constant) and type(node.value) is int:
                result = node.value
            elif isinstance(node, ast.Name):
                values = self.resolve(node.id, path, line, trail)
                if len({v.number for v in values}) != 1:
                    raise ValueError("arithmetic with conditional constant")
                result = values[0].number
            elif isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub, ast.Invert)):
                value = integer(node.operand)
                result = value if isinstance(node.op, ast.UAdd) else (-value if isinstance(node.op, ast.USub) else ~value)
            elif isinstance(node, ast.BinOp):
                a, b = integer(node.left), integer(node.right)
                if isinstance(node.op, ast.Add): result = a + b
                elif isinstance(node.op, ast.Sub): result = a - b
                elif isinstance(node.op, ast.Mult): result = a * b
                elif isinstance(node.op, (ast.Div, ast.Mod)) and b:
                    quotient = (abs(a) // abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
                    result = quotient if isinstance(node.op, ast.Div) else a - quotient * b
                elif isinstance(node.op, (ast.LShift, ast.RShift)) and 0 <= b < 32:
                    result = a << b if isinstance(node.op, ast.LShift) else a >> b
                elif isinstance(node.op, ast.BitOr): result = a | b
                elif isinstance(node.op, ast.BitAnd): result = a & b
                elif isinstance(node.op, ast.BitXor): result = a ^ b
                else: raise ValueError("unsupported arithmetic or division by zero")
            else:
                raise ValueError(f"cannot resolve dialog ID {expression!r}; use a constant")
            if not -(2**31) <= result <= 2**31 - 1:
                raise ValueError("dialog expression exceeds signed Pawn cell")
            return result
        number = integer(tree.body)
        return [Value(number, f"{path}:{ast.dump(tree)}", expression, f"{path}:{line}")]


def audit(texts: dict[str, str]) -> Report:
    definitions, calls = [], []
    for path, text in sorted(texts.items()):
        d, c = sources(text, path)
        definitions.extend(d)
        calls.extend(c)
    resolver = Resolver(definitions)
    sinks = {"ShowPlayerDialog": {1}}
    # Discover wrappers with no hardcoded module/function list, including chains
    # forwarding the ID at a different argument position.
    while True:
        additions = set()
        for d in definitions:
            if d.name != "ShowPlayerDialog" and d.expression in sinks:
                additions.update((d.name, index) for index in sinks[d.expression])
        for call in calls:
            for index in sinks.get(call.name, ()):
                if index < len(call.arguments) and call.parent:
                    argument = unparen(call.arguments[index])
                    if argument and argument in call.parent.parameters:
                        additions.add((call.parent.name, call.parent.parameters.index(argument)))
        fresh = {(name, index) for name, index in additions if index not in sinks.get(name, set())}
        if not fresh:
            break
        for name, index in fresh:
            sinks.setdefault(name, set()).add(index)
    dialogs, errors, count = {}, [], 0
    bound, edges = set(), []
    # Macro-generated dialog calls cannot silently bypass source coverage.
    for path, text in sorted(texts.items()):
        for match in DEFINE.finditer(mask(text)):
            if match[1] == "ShowPlayerDialog" or (
                match[2].startswith("(") and any(re.search(r"\b" + re.escape(name) + r"\b", match[2]) for name in sinks)
            ):
                errors.append(f"{path}:{text.count(chr(10), 0, match.start()) + 1}: unsupported dialog macro {match[1]}; use a stock wrapper")
    for call in calls:
        for index in sorted(sinks.get(call.name, ())):
            count += 1
            location = f"{call.path}:{call.line}"
            if index >= len(call.arguments):
                errors.append(f"{location}: {call.name} has no dialog argument #{index + 1}")
                continue
            expression = unparen(call.arguments[index])
            target = (call.name, index)
            if expression and call.parent and expression in call.parent.parameters:
                source = (call.parent.name, call.parent.parameters.index(expression))
                edges.append((source, target, location))
                continue
            try:
                values = resolver.resolve(expression, call.path, call.line)
                bound.add(target)
                for value in values:
                    if value.number < 0:
                        continue  # open.mp: negative dialog ID closes the dialog
                    if value.number > 32767:
                        raise ValueError(f"ID {value.number} outside open.mp range 0..32767")
                    dialogs.setdefault(value.number, {})[value.owner] = value
            except (ValueError, RecursionError) as exc:
                errors.append(f"{location}: {call.name}: {exc}")
    while True:
        fresh = {target for source, target, _ in edges if source in bound} - bound
        if not fresh:
            break
        bound.update(fresh)
    for source, _, location in edges:
        if source not in bound:
            errors.append(f"{location}: unbound dialog wrapper {source[0]} argument #{source[1] + 1}")
    for number, owners in sorted(dialogs.items()):
        if len(owners) > 1:
            details = "; ".join(f"{v.label} ({v.location})" for v in owners.values())
            errors.append(f"duplicate dialog ID {number}: {details}")
    return Report(dialogs, sorted(set(errors)), count, sorted(set(sinks) - {"ShowPlayerDialog"}))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(argv)
    folder = args.root / "gamemodes"
    if not folder.is_dir():
        print(f"DIALOG-ID ERROR: missing source directory {folder}")
        return 2
    try:
        texts = {p.relative_to(args.root).as_posix(): p.read_text("utf-8")
                 for p in sorted(folder.rglob("*")) if p.suffix in {".pwn", ".inc"}}
    except (OSError, UnicodeError) as exc:
        print(f"DIALOG-ID ERROR: cannot read sources: {exc}")
        return 2
    report = audit(texts)
    if not report.calls:
        report.errors.append("no dialog calls found; source coverage could not be established")
    for error in report.errors:
        print("DIALOG-ID ERROR: " + error)
    print(f"Dialog IDs: {len(texts)} files, {report.calls} calls, {len(report.dialogs)} IDs, "
          f"{len(report.wrappers)} wrappers, {len(report.errors)} errors.")
    return 1 if report.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
