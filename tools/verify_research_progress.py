"""Read-only audit of the native hook signatures, grades and installed research DLL."""
import ast
import hashlib
import json
import re
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
GAME = Path('C:/Games/san11/Romance Of Three Kingdom 11 PK')


def main():
    source = (ROOT / 'worldmod/src/research_progress.cpp').read_text(encoding='utf-8')
    pe = pefile.PE(str(GAME / 'san11pk.exe'), fast_load=True)
    sites = re.findall(r'\{(0x[0-9a-f]+), ("[^"]+"), (\d+),', source)
    assert len(sites) == 11
    for address, literal, length in sites:
        expected = ast.literal_eval(literal).encode('latin1')
        actual = pe.get_data(int(address, 16) - pe.OPTIONAL_HEADER.ImageBase, int(length))
        assert len(expected) == int(length) and actual == expected, (address, actual.hex(), expected.hex())
    grade_path = ROOT.parent / 'san11world-persons/tools/roster/skill_grades.py'
    namespace = {}
    exec(compile(grade_path.read_text(encoding='utf-8'), str(grade_path), 'exec'), namespace)
    grades = ''.join(namespace['GRADES'][i] for i in range(100))
    assert grades in source
    master = (GAME / 'world_custom/scenario/Scenario.s11').read_bytes()[-98 * 74:]
    assert len(master) == 98 * 74
    assert all(master[i * 74 + 73] == (255 if i < 48 else (i - 48) // 5) for i in range(98))
    assert all(master[i * 74 + 65] == 2 for i in range(48, 98))
    log = (ROOT / 'worldmod/build/research_progress_test.log').read_text(encoding='utf-8')
    assert 'research runtime fixture: 0 failures' in log and 'FAIL' not in log
    built = (ROOT / 'worldmod/build/d3d9.dll').read_bytes()
    installed = (GAME / 'worldmod_core.dll').read_bytes()
    result = dict(native_signatures_verified=len(sites), original_skill_grades=100,
                  original_research_records=98, native_hidden_groups=10,
                  runtime_fixture_passes=sum(line.startswith('PASS ') for line in log.splitlines()),
                  built_sha256=hashlib.sha256(built).hexdigest(),
                  installed_sha256=hashlib.sha256(installed).hexdigest(),
                  installed_matches_build=built == installed)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
