#!/usr/bin/env python3
"""Real R41/MySQL tests, plus compilation of actual game-generated Pawn exports.

The target must be a disposable database named tds_test*. This resets it.
No Python database package is required: the MySQL CLI and existing R41 are used.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

from check_pawn_output import analyze_pawn_output
from test_tdstudio import ROOT, run


def compile_exports(source, output, server, prefix):
    files = sorted(source.glob("*.inc")) if source.exists() else []
    if not files:
        return  # Restart fixture only loads; it intentionally produces no export.
    dest = output / "tdstudio-exports"
    dest.mkdir(parents=True, exist_ok=True)
    text = '#include <open.mp>\n'
    calls = []
    personal = global_td = empty = cyrillic = False
    moved_exports = 0
    for file in files:
        data = file.read_bytes()
        content = data.decode("cp1251", errors="strict")
        assert content.encode("cp1251") == data
        shutil.copy2(file, dest / file.name)
        shutil.copy2(file.with_suffix(".txt"), dest / file.with_suffix(".txt").name)
        text += f'#include "{file.name}"\n'
        match = re.search(r"stock (TD_[a-z0-9]+_[0-9a-f]+)_Create\((playerid)?\)", content, re.I)
        assert match, file.name
        symbol, is_personal = match.groups()
        personal |= bool(is_personal)
        global_td |= not bool(is_personal)
        empty |= "// Element ID" not in content
        cyrillic |= 'Привет, \\"Дима\\"! C:\\\\test 100% ~g~текст' in content
        if 'Привет, \\"Дима\\"! C:\\\\test 100% ~g~текст' in content:
            positions = [tuple(map(float, pair)) for pair in re.findall(
                r'(?:CreatePlayerTextDraw\(playerid, |TextDrawCreate\()([0-9.]+), ([0-9.]+),', content)]
            assert len(positions) == 4, (file.name, positions)
            assert abs(positions[0][0] - 13.345) < 0.0001 and abs(positions[0][1] - 66.89) < 0.0001, positions
            assert positions[1:] == [(271.0, 199.0)] * 3, positions
            moved_exports += 1
        calls.extend([f'{symbol}_Create({"0" if is_personal else ""});',
                      f'{symbol}_Show(0);', f'{symbol}_Hide(0);',
                      f'{symbol}_Hit({"0, PlayerText:INVALID_PLAYER_TEXT_DRAW" if is_personal else "Text:INVALID_TEXT_DRAW"});',
                      f'{symbol}_Destroy({"0" if is_personal else ""});'])
    assert personal and global_td and empty and cyrillic, "missing export fixture coverage"
    assert moved_exports == 2, "both TextDraw and PlayerTextDraw must export confirmed keyboard coordinates"
    text += '\nmain() {\n' + '\n'.join('    '+line for line in calls) + '\n}\n'
    context = dest / "export_compile.pwn"
    context.write_bytes(text.encode("cp1251"))
    target = dest / "export_compile.amx"
    compiler_env = os.environ.copy()
    compiler_env["LD_LIBRARY_PATH"] = str(server / "qawno") + os.pathsep + compiler_env.get("LD_LIBRARY_PATH", "")
    process = subprocess.run(prefix + [str(server / "qawno/pawncc"), "-d3", f"-i{server / 'qawno/include'}",
        f"-i{dest}", f"-o{target}", str(context)], capture_output=True, timeout=45, env=compiler_env)
    log = (process.stdout + process.stderr).decode("utf-8", errors="replace")
    (output / "tdstudio-export-compile.log").write_text(log, encoding="utf-8")
    if process.returncode or analyze_pawn_output(log) or not target.exists() or not target.stat().st_size:
        raise RuntimeError(f"Export compilation failed:\n{log}")
    print(f"Exports: {len(files)} actual .inc files compiled together, all lifecycle functions and Hit referenced.")


def settings(path):
    pairs = {}
    for line in path.read_text().splitlines():
        if '=' in line and not line.lstrip().startswith('#'):
            key, value = line.split('=', 1)
            pairs[key.strip()] = value.strip()
    assert re.fullmatch(r'tds_test[a-zA-Z0-9_]*', pairs.get('database', '')), 'Only disposable tds_test* databases allowed'
    return pairs


def sql_checks(query):
    columns = ['element_id','draw_order','kind','x','y','width','height','letter_x','letter_y','colour','box_colour',
               'background','font','alignment','outline','shadow','use_box','proportional','selectable','sprite','model',
               'rot_x','rot_y','rot_z','zoom','vehicle_colour1','vehicle_colour2']
    meta = query("SELECT project_id,HEX(name),HEX(description),interface_type,next_element_id,revision FROM tdstudio_projects WHERE name='Persistent clone'").strip().split('\t')
    assert len(meta) == 6, meta
    pid, name, description, kind, next_id, revision = meta
    elements = query(f"SELECT JSON_ARRAY({','.join('`'+c+'`' for c in columns)},HEX(content)) FROM tdstudio_elements WHERE project_id={pid} ORDER BY ordinal")
    rows = [json.loads(row) for row in elements.splitlines()]
    def call(payload, actor=100, project=pid, project_name=name):
        encoded = json.dumps(payload,separators=(',',':')).encode().hex()
        return query(f"CALL tds_save({project},{actor},0,{revision},X'{project_name}',X'{description}',{kind},{next_id},CONVERT(X'{encoded}' USING ascii))")
    before = query(f"SELECT revision,HEX(name),(SELECT COUNT(*) FROM tdstudio_elements WHERE project_id={pid}) FROM tdstudio_projects WHERE project_id={pid}")
    invalid = [list(row) for row in rows]
    invalid[1][2] = 99
    assert call(invalid).strip() == '0', 'invalid later element must roll back the entire transaction'
    assert before == query(f"SELECT revision,HEX(name),(SELECT COUNT(*) FROM tdstudio_elements WHERE project_id={pid}) FROM tdstudio_projects WHERE project_id={pid}")
    assert call(rows,actor=101).strip() == '0', 'foreign write must fail'
    assert query(f'CALL tds_delete({pid},101,0,{revision})').strip() == '0', 'foreign delete must fail'
    assert str(pid) not in query('CALL tds_list(101,0,0)').split('\t')[0::3], 'foreign list must be filtered'
    assert not query(f'CALL tds_load({pid},101,0)').strip(), 'foreign load must return no rows'
    injection = "sql'); SELECT 1; --".encode().hex()
    response = call(rows,project='0',project_name=injection).strip().split('\t')
    assert response[0] == '1', response
    injected_id = response[1]
    assert query(f'SELECT HEX(name) FROM tdstudio_projects WHERE project_id={injected_id}').strip().lower() == injection
    assert int(query('SELECT COUNT(*) FROM accounts').strip()) == 3
    assert query(f'CALL tds_delete({injected_id},100,0,1)').strip() == '1'
    bad_shape = [list(row) for row in rows]; bad_shape[0] = bad_shape[0][:-1]
    assert call(bad_shape).strip() == '0'
    print('MySQL: rollback after partial insert, ownership filters, typed parameters/injection, invalid payload and cascade delete passed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--mysql-config',type=Path,required=True)
    parser.add_argument('--mysql-client',type=Path,default=Path('mysql'))
    parser.add_argument('--plugin',type=Path,required=True,help='Directory containing plugins/mysql.so and log-core.so')
    parser.add_argument('--prefix',default='')
    parser.add_argument('--output',type=Path,default=ROOT/'build')
    args = parser.parse_args()
    cfg = settings(args.mysql_config)
    env = os.environ.copy(); env['MYSQL_PWD'] = cfg.get('password','')
    client = [str(args.mysql_client),'--no-defaults','--protocol=TCP','--batch','--raw','--skip-column-names',
              '-h'+cfg.get('hostname','127.0.0.1'),'-P'+cfg.get('server_port','3306'),'-u'+cfg['username']]
    def query(statement, database=True):
        result = subprocess.run(client + ([cfg['database']] if database else []),input=statement,text=True,capture_output=True,env=env,timeout=30)
        if result.returncode: raise RuntimeError(result.stderr)
        return result.stdout
    db = cfg['database']
    query(f'DROP DATABASE IF EXISTS `{db}`; CREATE DATABASE `{db}`;',False)
    for migration in sorted((ROOT/'db/migrations').glob('*.sql')):
        query(migration.read_text())
    query("INSERT INTO accounts(account_id,username,password_hash,background) VALUES (100,'Veyran',REPEAT('a',60),1),(101,'Developer',REPEAT('b',60),1),(102,'Other',REPEAT('c',60),1)")
    prefix = shlex.split(args.prefix)
    run(args.server,prefix,args.output,'tdstudio_stage2.pwn',args.mysql_config,args.plugin)
    shutil.copy2(args.output/'tdstudio-test-run.log',args.output/'tdstudio-mysql-run.log')
    sql_checks(query)
    # A fresh open.mp process has no in-memory project; it must reconstruct from MySQL.
    run(args.server,prefix,args.output,'tdstudio_stage2.pwn',args.mysql_config,args.plugin,restart=True)
    shutil.copy2(args.output/'tdstudio-test-run.log',args.output/'tdstudio-restart-run.log')
    print('Restart: a new open.mp process restored every persisted element/property from MySQL.')


if __name__ == '__main__':
    main()
