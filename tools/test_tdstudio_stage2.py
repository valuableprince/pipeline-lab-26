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


def compile_exports(source, output, server, prefix, profile="stage2"):
    files = sorted(source.glob("*.inc")) if source.exists() else []
    if not files:
        return  # Restart fixture only loads; it intentionally produces no export.
    dest = output / "tdstudio-exports"
    dest.mkdir(parents=True, exist_ok=True)
    text = '#include <open.mp>\n'
    calls = []
    personal = global_td = empty = cyrillic = False
    moved_exports = 0
    fixture = {
        "stage2": 'Привет, \\"Дима\\"! C:\\\\test 100% ~g~текст',
        "stage31": 'Логотип \\"TARKENSO\\" C:\\\\ui 100% ~g~текст',
        "stage32": 'Геометрия \\"TARKENSO\\" C:\\\\ui 100% ~g~текст',
        "stage33": 'Шаблон \\"TARKENSO\\" C:\\\\ui 100% ~g~текст',
    }[profile]
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
        cyrillic |= fixture in content
        if fixture in content:
            positions = [tuple(map(float, pair)) for pair in re.findall(
                r'(?:CreatePlayerTextDraw\(playerid, |TextDrawCreate\()([0-9.]+), ([0-9.]+),', content)]
            if profile == "stage2":
                assert len(positions) == 4, (file.name, positions)
                assert abs(positions[0][0] - 13.345) < 0.0001 and abs(positions[0][1] - 66.89) < 0.0001, positions
                assert positions[1:] == [(271.0, 199.0)] * 3, positions
            elif profile == "stage31":
                ids = list(map(int, re.findall(r'// Element ID (\d+), order', content)))
                assert ids == [5, 6, 1, 2, 3, 4], (file.name, ids)
                assert positions == [(20., 30.), (310., 270.), (11., 19.), (31., 39.), (51., 59.), (71., 79.)], positions
            elif profile == "stage32":
                ids = list(map(int, re.findall(r'// Element ID (\d+), order', content)))
                assert ids == [5, 1, 2, 3, 4], (file.name, ids)
                assert positions == [(20., 20.), (260., 170.), (380., 170.), (260., 190.), (380.125, 190.25)], positions
                sizes = [tuple(map(float, pair)) for pair in re.findall(
                    r'(?:PlayerTextDraw|TextDraw)TextSize\([^\n]+, ([0-9.]+), ([0-9.]+)\);', content)]
                assert sizes == [(10., 10.), (10., 80.), (60., 10.), (81., 16.), (60.125, 20.375)], sizes
                letters = re.findall(r'(?:PlayerTextDraw|TextDraw)LetterSize\([^\n]+, ([0-9.]+), ([0-9.]+)\);', content)
                assert len(letters) == 1 and all(abs(float(v)-.5) < 1e-7 for v in letters[0]), letters
                assert re.search(r'Alignment\([^\n]+TEXT_DRAW_ALIGN:2\);', content)
                assert 'SetPreviewRot' in content and '1.500000000' in content
                assert '0x7CA6C030' not in content and len(positions) == 5, 'editor guides leaked into export'
            else:
                ids = list(map(int, re.findall(r'// Element ID (\d+), order', content)))
                assert ids == list(range(1, 9)), ids
                assert positions == [(275.,190.),(295.,210.),(315.,230.),(335.,250.),
                                     (100.,100.),(120.,120.),(140.,140.),(160.,160.)], positions
                assert content.count('Шаблон') == 2
                assert '0x7CA6C030' not in content
                sizes = [tuple(map(float, pair)) for pair in re.findall(
                    r'(?:PlayerTextDraw|TextDraw)TextSize\([^\n]+, ([0-9.]+), ([0-9.]+)\);', content)]
                assert sizes == [(315.,20.),(30.,20.),(40.,30.),(30.,40.),
                                 (140.,20.),(30.,20.),(40.,30.),(30.,40.)], sizes
            moved_exports += 1
        calls.extend([f'{symbol}_Create({"0" if is_personal else ""});',
                      f'{symbol}_Show(0);', f'{symbol}_Hide(0);',
                      f'{symbol}_Hit({"0, PlayerText:INVALID_PLAYER_TEXT_DRAW" if is_personal else "Text:INVALID_TEXT_DRAW"});',
                      f'{symbol}_Destroy({"0" if is_personal else ""});'])
    assert personal and global_td and cyrillic and (empty or profile != "stage2"), "missing export fixture coverage"
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


def legacy_project(query):
    """Create with the original 012 procedure, rather than pretending v2 is v1."""
    row = [1, 7, 0, 12.5, 34.25, 100., 30., .3, 1.2, -1, 0, 0, 1, 1, 0, 0, 0, 1, 0,
           0, 411, 0., 0., 0., 1., 0, 0, 'Старый интерфейс'.encode('cp1251').hex()]
    wire = json.dumps([row], separators=(',', ':')).encode().hex()
    name = 'Legacy format1'.encode().hex()
    result = query(f"CALL tds_save(0,100,0,0,X'{name}',X'',1,2,CONVERT(X'{wire}' USING ascii))").strip().split('\t')
    assert result[0] == '1', result
    return int(result[1])


def sql_group_checks(query):
    columns = ['element_id','draw_order','kind','x','y','width','height','letter_x','letter_y','colour','box_colour',
               'background','font','alignment','outline','shadow','use_box','proportional','selectable','sprite','model',
               'rot_x','rot_y','rot_z','zoom','vehicle_colour1','vehicle_colour2']
    meta = query("SELECT project_id,HEX(name),HEX(description),interface_type,next_element_id,revision,next_group_id FROM tdstudio_projects WHERE name='Advanced saved copy'").strip().split('\t')
    pid, name, description, kind, next_id, revision, next_group = meta
    rows = [json.loads(row) for row in query(f"SELECT JSON_ARRAY({','.join('`'+c+'`' for c in columns)},HEX(content)) FROM tdstudio_elements WHERE project_id={pid} ORDER BY ordinal").splitlines()]
    groups = []
    for line in query(f'SELECT group_id,HEX(name) FROM tdstudio_groups WHERE project_id={pid} ORDER BY group_id').splitlines():
        gid, gname = line.split('\t')
        members = list(map(int, query(f'SELECT element_id FROM tdstudio_group_members WHERE project_id={pid} AND group_id={gid} ORDER BY element_id').split()))
        groups.append([int(gid), gname, members])
    assert [g[0] for g in groups] == [1, 2] and [len(g[2]) for g in groups] == [4, 2]
    def snapshot():
        return (query(f'SELECT revision,HEX(name),next_group_id FROM tdstudio_projects WHERE project_id={pid}'),
                query(f'SELECT JSON_ARRAY({",".join("`"+c+"`" for c in columns)},HEX(content)) FROM tdstudio_elements WHERE project_id={pid} ORDER BY ordinal'),
                query(f'SELECT group_id,HEX(name) FROM tdstudio_groups WHERE project_id={pid} ORDER BY group_id'),
                query(f'SELECT element_id,group_id FROM tdstudio_group_members WHERE project_id={pid} ORDER BY element_id'))
    def call(payload, actor=100, project=pid, project_name=name):
        elements = json.dumps(rows, separators=(',', ':')).encode().hex()
        encoded = json.dumps(payload, separators=(',', ':')).encode().hex()
        return query(f"CALL tds_save_v2({project},{actor},0,{revision},X'{project_name}',X'{description}',{kind},{next_id},CONVERT(X'{elements}' USING ascii),{next_group},CONVERT(X'{encoded}' USING ascii))").strip()
    before = snapshot()
    invalids = []
    bad = json.loads(json.dumps(groups)); bad[1][2].append(999999); invalids.append(bad)
    bad = json.loads(json.dumps(groups)); bad[1][2].append(groups[0][2][0]); invalids.append(bad)
    bad = json.loads(json.dumps(groups)); bad[1][0] = bad[0][0]; invalids.append(bad)
    bad = json.loads(json.dumps(groups)); bad[1][1] = bad[0][1]; invalids.append(bad)
    bad = json.loads(json.dumps(groups)); bad[1][2] = []; invalids.append(bad)
    bad = json.loads(json.dumps(groups)); bad[1][2][0] = 1.5; invalids.append(bad)
    bad = json.loads(json.dumps(groups)); bad[1][1] = '00'; invalids.append(bad)
    invalids.append(groups * 9)
    for invalid in invalids:
        assert call(invalid, project_name='Changed'.encode().hex()) == '0', invalid
        assert snapshot() == before, 'elements/groups/membership/revision must roll back together'
    assert call(groups, actor=101) == '0' and snapshot() == before, 'foreign group save denied'
    # A legacy writer must not drop groups from a format-2 project.
    elements = json.dumps(rows, separators=(',', ':')).encode().hex()
    assert query(f"CALL tds_save({pid},100,0,{revision},X'{name}',X'{description}',{kind},{next_id},CONVERT(X'{elements}' USING ascii))").strip() == '0'
    assert snapshot() == before
    injected = json.loads(json.dumps(groups)); injected[0][1] = "x'); SELECT 1; --".encode().hex()
    saved = call(injected, project='0').split('\t')
    assert saved[0] == '1' and int(query('SELECT COUNT(*) FROM accounts').strip()) == 3
    copy = int(saved[1])
    assert query(f'SELECT HEX(name) FROM tdstudio_groups WHERE project_id={copy} AND group_id=1').strip().lower() == injected[0][1]
    assert query(f'CALL tds_delete({copy},100,0,1)').strip() == '1'
    assert query(f'SELECT COUNT(*) FROM tdstudio_group_members WHERE project_id={copy}').strip() == '0'
    assert query(f'SELECT COUNT(*) FROM tdstudio_projects WHERE name=\'Legacy format1\' AND format_version=2 AND revision=2').strip() == '1'
    print('MySQL groups: 8 invalid payloads rolled back all four entities; ownership, v1 writer protection, HEX injection-as-data, cascade and v1 upgrade passed.')


def sql_template_checks(query):
    """Exercise the actual 016 routines, independent of frontend authorization."""
    columns = ['element_id','draw_order','kind','x','y','width','height','letter_x','letter_y','colour','box_colour',
               'background','font','alignment','outline','shadow','use_box','proportional','selectable','sprite','model',
               'rot_x','rot_y','rot_z','zoom','vehicle_colour1','vehicle_colour2']
    private_name = 'Личный логотип'.encode('cp1251').hex()
    meta = query(f"SELECT template_id,HEX(name),HEX(description),next_element_id,next_group_id,origin_x,origin_y,revision FROM tdstudio_templates WHERE name=CONVERT(X'{private_name}' USING cp1251)").strip().split('\t')
    tid, name, description, next_id, next_group, ox, oy, revision = meta
    rows = [json.loads(row) for row in query(f"SELECT JSON_ARRAY({','.join('`'+c+'`' for c in columns)},HEX(content)) FROM tdstudio_template_elements WHERE template_id={tid} ORDER BY ordinal").splitlines()]
    groups = []
    for line in query(f'SELECT group_id,HEX(name) FROM tdstudio_template_groups WHERE template_id={tid} ORDER BY group_id').splitlines():
        gid, gname = line.split('\t')
        members = list(map(int,query(f'SELECT element_id FROM tdstudio_template_members WHERE template_id={tid} AND group_id={gid} ORDER BY element_id').split()))
        groups.append([int(gid),gname,members])
    assert len(rows) == 4 and len(groups) == 2
    def counts():
        return tuple(query(f'SELECT COUNT(*) FROM {table}').strip() for table in
                     ['tdstudio_templates','tdstudio_template_elements','tdstudio_template_groups','tdstudio_template_members'])
    def save(elements=rows, membership=groups, actor=100, privileged=0, visibility=0, label=name, category=5, origin_x=ox):
        payload = json.dumps(elements,separators=(',',':')).encode().hex()
        group_payload = json.dumps(membership,separators=(',',':')).encode().hex()
        return query(f"CALL tds_template_save({actor},{privileged},X'{label}',X'{description}',{category},{visibility},1,{next_id},CONVERT(X'{payload}' USING ascii),{next_group},CONVERT(X'{group_payload}' USING ascii),{origin_x},{oy})").strip()
    before = counts()
    bad = json.loads(json.dumps(rows)); bad[1][2] = 99
    assert save(elements=bad) == '0' and counts() == before, 'partial element insert must roll back all entities'
    bad = json.loads(json.dumps(groups)); bad[1][2].append(99999)
    assert save(membership=bad) == '0' and counts() == before, 'partial group insert must roll back all entities'
    assert save(actor=101,visibility=1) == '0' and counts() == before, 'SQL must enforce publication privilege'
    assert save(category=65536) == '0' and counts() == before
    assert save(origin_x=float(ox)+1) == '0' and counts() == before, 'origin mismatch rolls back a complete staged component'
    assert not query(f'CALL tds_template_load({tid},101,0)').strip(), 'private read denied in SQL'
    shared = query("SELECT template_id,revision FROM tdstudio_templates WHERE visibility=1").strip().split('\t')
    sid, srevision = shared
    assert len(query(f'CALL tds_template_load({sid},101,0)').splitlines()) == 4, 'shared read allowed'
    assert query(f"CALL tds_template_update({sid},101,0,{srevision},X'{name}',X'{description}')").strip() == '0'
    assert query(f'CALL tds_template_delete({sid},101,0,{srevision})').strip() == '0'
    assert query(f"CALL tds_template_update({tid},100,0,{revision},X'{name}',X'{description}')").strip() == '1'
    assert query(f"CALL tds_template_update({tid},100,0,{revision},X'{name}',X'{description}')").strip() == '0', 'stale library revision denied'
    injection = "sql'); SELECT 1; --".encode().hex()
    result = save(label=injection).split('\t'); assert result[0] == '1', result
    clone = int(result[1])
    assert query(f'SELECT HEX(name) FROM tdstudio_templates WHERE template_id={clone}').strip().lower() == injection
    assert int(query('SELECT COUNT(*) FROM accounts').strip()) == 3
    assert query(f'CALL tds_template_delete({clone},100,0,1)').strip() == '1'
    assert query(f'SELECT COUNT(*) FROM tdstudio_template_members WHERE template_id={clone}').strip() == '0'
    for i in range(23):
        assert save(label=f'Page {i:02}'.encode().hex(),category=6).split('\t')[0] == '1'
    page1 = query('CALL tds_template_list(100,0,6,0)').splitlines()
    page2 = query('CALL tds_template_list(100,0,6,20)').splitlines()
    assert len(page1) == 21 and len(page2) == 3 and page1[20] == page2[0]
    assert len({row.split('\t')[0] for row in page1[:20]+page2}) == 23
    assert not query('CALL tds_template_list(100,0,6,1000001)').strip()
    assert query("SELECT COUNT(*) FROM tdstudio_templates WHERE name='Maximum component'").strip() == '1'
    assert query("SELECT COUNT(*) FROM tdstudio_template_elements e JOIN tdstudio_templates t USING(template_id) WHERE t.name='Maximum component'").strip() == '64'
    print('MySQL templates: rollback after partial element/group writes, server-independent rights, CAS, HEX injection-as-data, cascade, maximum payload and 23-template pagination passed.')


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
        if migration.name in {'015_tdstudio_groups.sql','016_tdstudio_templates.sql'}:
            continue
        query(migration.read_text())
    query("INSERT INTO accounts(account_id,username,password_hash,background) VALUES (100,'Veyran',REPEAT('a',60),1),(101,'Developer',REPEAT('b',60),1),(102,'Other',REPEAT('c',60),1)")
    legacy = legacy_project(query)
    original = query(f'SELECT project_id,HEX(name),creator_id,format_version,revision,next_element_id FROM tdstudio_projects WHERE project_id={legacy}')
    original_elements = query(f'SELECT ordinal,element_id,draw_order,x,y,width,height,letter_x,letter_y,colour,box_colour,background,font,alignment,outline,shadow,use_box,proportional,selectable,sprite,model,rot_x,rot_y,rot_z,zoom,vehicle_colour1,vehicle_colour2,HEX(content) FROM tdstudio_elements WHERE project_id={legacy} ORDER BY ordinal')
    query((ROOT/'db/migrations/015_tdstudio_groups.sql').read_text())
    assert original == query(f'SELECT project_id,HEX(name),creator_id,format_version,revision,next_element_id FROM tdstudio_projects WHERE project_id={legacy}')
    assert original_elements == query(f'SELECT ordinal,element_id,draw_order,x,y,width,height,letter_x,letter_y,colour,box_colour,background,font,alignment,outline,shadow,use_box,proportional,selectable,sprite,model,rot_x,rot_y,rot_z,zoom,vehicle_colour1,vehicle_colour2,HEX(content) FROM tdstudio_elements WHERE project_id={legacy} ORDER BY ordinal')
    assert query(f'SELECT next_group_id FROM tdstudio_projects WHERE project_id={legacy}').strip() == '1'
    existing_tables = ['tdstudio_projects','tdstudio_elements','tdstudio_groups','tdstudio_group_members']
    before16 = [query(f'SELECT * FROM {table}') for table in existing_tables]
    query((ROOT/'db/migrations/016_tdstudio_templates.sql').read_text())
    assert before16 == [query(f'SELECT * FROM {table}') for table in existing_tables], '016 must preserve all existing project rows'
    assert query(f'CALL tds_delete({legacy},100,0,1)').strip() == '1'
    prefix = shlex.split(args.prefix)
    run(args.server,prefix,args.output,'tdstudio_stage2.pwn',args.mysql_config,args.plugin)
    shutil.copy2(args.output/'tdstudio-test-run.log',args.output/'tdstudio-mysql-run.log')
    sql_checks(query)
    # A fresh open.mp process has no in-memory project; it must reconstruct from MySQL.
    run(args.server,prefix,args.output,'tdstudio_stage2.pwn',args.mysql_config,args.plugin,restart=True)
    shutil.copy2(args.output/'tdstudio-test-run.log',args.output/'tdstudio-restart-run.log')
    print('Restart: a new open.mp process restored every persisted element/property from MySQL.')
    legacy_project(query)
    advanced = args.output/'stage31'
    run(args.server,prefix,advanced,'tdstudio_stage31.pwn',args.mysql_config,args.plugin,export_profile='stage31')
    shutil.copy2(advanced/'tdstudio-test-run.log',advanced/'tdstudio-stage31-run.log')
    sql_group_checks(query)
    run(args.server,prefix,advanced,'tdstudio_stage31.pwn',args.mysql_config,args.plugin,restart=True,export_profile='stage31')
    shutil.copy2(advanced/'tdstudio-test-run.log',advanced/'tdstudio-stage31-restart-run.log')
    print('Groups restart: fresh open.mp process restored group IDs/names/membership, stable element IDs, layers and confirmed positions.')
    geometry = args.output/'stage32'
    run(args.server,prefix,geometry,'tdstudio_stage32.pwn',args.mysql_config,args.plugin,export_profile='stage32')
    shutil.copy2(geometry/'tdstudio-test-run.log',geometry/'tdstudio-stage32-run.log')
    geometry_rows = query("SELECT e.element_id,e.x,e.y,e.width,e.height,e.letter_x,e.letter_y FROM tdstudio_elements e JOIN tdstudio_projects p USING(project_id) WHERE p.name='Geometry saved copy' ORDER BY e.draw_order")
    assert len(geometry_rows.splitlines()) == 5, geometry_rows
    assert query("SELECT COUNT(*) FROM tdstudio_group_members m JOIN tdstudio_projects p USING(project_id) WHERE p.name='Geometry saved copy'").strip() == '4'
    run(args.server,prefix,geometry,'tdstudio_stage32.pwn',args.mysql_config,args.plugin,restart=True,export_profile='stage32')
    shutil.copy2(geometry/'tdstudio-test-run.log',geometry/'tdstudio-stage32-restart-run.log')
    print('Geometry restart: a fresh open.mp process restored six geometry fields, layers, IDs, group membership and Cyrillic. Both export types compiled with verified native geometry.')
    library = args.output/'stage33'
    run(args.server,prefix,library,'tdstudio_stage33.pwn',args.mysql_config,args.plugin,export_profile='stage33')
    shutil.copy2(library/'tdstudio-test-run.log',library/'tdstudio-stage33-run.log')
    sql_template_checks(query)
    run(args.server,prefix,library,'tdstudio_stage33.pwn',args.mysql_config,args.plugin,restart=True,export_profile='stage33')
    shutil.copy2(library/'tdstudio-test-run.log',library/'tdstudio-stage33-restart-run.log')
    print('Templates restart: a fresh open.mp process loaded reusable components and restored confirmed project geometry/groups. Undo checkpoints, rights, stale callbacks and both compiled exports passed.')


if __name__ == '__main__':
    main()
