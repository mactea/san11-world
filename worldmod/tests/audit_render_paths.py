"""Read-only static rendering inventory. Never loads or executes the game.

Generated assembly and extracted shader sources stay in the ignored build tree.
Known water/object code contracts are revision-guarded; unknown sites remain unclassified.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

import capstone
import pefile


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = {
    'water_state': (0x4227E0, 0x422975, '27cd68e9c9a6c3233629569376ec657da502ec597d3437862bfcb52062c9d73a'),
    'water_quad': (0x422980, 0x422AEF, 'c7fa4c3ffe1e8616df8a8c647cbe9b6dc0f36f573abd1a0dd8866a1fe0492e92'),
    'water_draw': (0x422C20, 0x422D57, '6c89836eb9387d4a79f06d4bf34ffece2f3bc21c6f64a8a281c82d12b10ea51b'),
    'water_animation': (0x422DA0, 0x422EA2, '8bad4522ec5ff4e97dc9ee4367dbdf3532d3b81ef0443c3a007d3807e871e3ed'),
    'shared_unlit_initializer': (0x402870, 0x4028DA, 'dce72de04e56374269bb19a1d2ba91129ecf17304b170568237ea7fc57fa615e'),
    'world_pass': (0x5A1D50, 0x5A2000, '774c8e2cbdd7b67bfaa4aa972548cb9835040294a400c80412ad705a03005e73'),
    'water_vertex_create': (0x422710, 0x422742, '15660a8e6ee9127560f6967db8819d1e7894c465c8c8f36bacf753c9f61eb0f5'),
    'water_index_create': (0x42301D, 0x42311B, '2fe3b8409ddcbc980e7a20eab347a160caaeadee6243d70a95ad365a2cf1d618'),
    'object_shader_init': (0x41B110, 0x41B162, 'bbb6bbe05e4c0fe4fcf983357f70ef8b7a58be5781457bc8b0a7650a44c6675d'),
    'object_draw': (0x41B2F0, 0x41B3AE, '40a231a1e95eebefef75aacf7fa97baaa394107b58e93b7f2c5d9062552745f1'),
    'object_pass': (0x41B7B0, 0x41B960, 'eab2a8d77ec22c1bda100b7e957fe789a7566e1fffd83cf529a739cd37bdbadb'),
    'object_declaration': (0x794828, 0x794850, '5d0e23f621c77922342b19608522e1122fd18d32f6e5a03bbee48e65fc24f3a2'),
    'model_shader_wrapper': (0x44D1E0, 0x44D241, '9eca14acb0db23fd74c1bd4118b87ae0c75f0aac4d4e35b90d1c3c6d8e34f41b'),
    'model_shader_init': (0x44AD30, 0x44AE1E, '7c885018a7d8a6a757a7c69afebd474543a0e35750b5e01054e9dd5854865cea'),
    'blend2_declaration': (0x7947F8, 0x794828, '4f2946c5cbae2fd1586771822c738a733856378cb54a976fede8b4bbc7ee2740'),
    'blend4_declaration': (0x7947C8, 0x7947F8, 'cdaf0919d68de3f809994db32d3a8735663924e71f4b95b7a1d756dbe18020b3'),
    'model_vb_wrapper': (0x44D040, 0x44D070, '50d0357a0636dda1eccf4a64e2bb103ef5a31dd5576f96f903eab8d7a0de7b9d'),
    'model_ib_wrapper': (0x44D070, 0x44D0A0, 'a5c8381ba4b3221a1f01507a85820f70b1c2864fbfd1d2501dc29ccd47fa72f1'),
    'dual_uv_shader_init': (0x4124B0, 0x4124E5, 'ebcbb993768630d49bc8782079bd9e351869c0a5eb162b52c5b5f679bfefa164'),
    'dual_uv_draw': (0x412630, 0x412728, 'bd26fbd0b3411fc3d8584252d421c1a9858aabb886ca5b05aad1b66483b0cc82'),
    'dual_uv_vb_create': (0x412C70, 0x412CA6, '678bf8d9fe3ca6318ddc86dec1256ff49eef492c43fca6ce36927c337fd49d42'),
    'dual_uv_ib_create': (0x412540, 0x4125CE, '88bc20579f1ed52b734250b3e6eb6973187f76c492a194ce627160be3f83d83b'),
    'dual_uv_declaration': (0x7768D0, 0x7768F0, 'a93e6387349431bbf0393e32a151767882e2f601dd9c8592fd270325e7cc7978'),
    'unlit_model_init': (0x4421C0, 0x44222B, 'fd8f177c69f3d49da18d7398dcc45db5cb249b53a0ae6f36c8a30bbb82aef8bc'),
    'unlit_model_draw': (0x442230, 0x4423AB, '1c22fb80c0f9e007aea5f01512aac1ffb6a4b6d9e4ef6751bac74ac6d9151dfd'),
    'unlit_declaration': (0x74E994, 0x74E9B4, '3ad5b8ef34b58958287da8aa9238a1510ca4b395ddac49a0fcc1ca6dcf3ff54f'),
    'managed_vb_wrapper': (0x44D4C0, 0x44D4EA, '2352476ac4f9b7950ae13524b744bac2a42c07fae15a1fb90885bac5a48b9e55'),
    'sprite_ib_create': (0x403340, 0x4033B4, 'c5b41afe4790d94b682b38b91516b5b55eecd0295be2e3748977594e6f6c4ece'),
    'shader_fvf_wrapper': (0x44D120, 0x44D157, '9c33d423054e494ed697031c610b5f5006c4e6570a824d0f12d5d0ce778f1b77'),
    'screen_color_state': (0x42E7E0, 0x42E84E, 'c4ba35cff63f418271169ee9241bfbd42e6331c5746ec73bc7550129a5014165'),
    'screen_texture_state': (0x42ED50, 0x42EDD4, '2815de2f0cd3ccddd5cea5fa35422dca3c716779813eec0c301edbe4f4ba0367'),
    'screen_glyph_state': (0x432A80, 0x432B05, '9d8642870175e25de3ca0874613f3bf046c72e47233e0504e14919d993a06789'),
    'screen_fan_draw': (0x42E8B0, 0x42E8C6, '658fdefb1116bfa4d11d43a083175d573e3d8ef5e7f46d1d159c8087a59eb74a'),
    'fixed_unlit_ib_create': (0x40F0C7, 0x40F0DC, '21aefa96f9c1268683a08dc248cdcb906e6214a825fe7b6e0bea193077c753d8'),
    'screen_ib_create': (0x40F76E, 0x40F786, 'c06ca0987e946439223bb7556827a81e7af09c6e720d9ddf29880798b1476c27'),
    'screen_texture_only_state': (0x4113D5, 0x411407, '6ffbad6c78446326dd94f45892999dc7d70c907d204fe2c577a9a92782734c71'),
}


def audit(exe, output):
    data = exe.read_bytes()
    pe = pefile.PE(data=data, fast_load=True)
    if pe.OPTIONAL_HEADER.ImageBase != 0x400000 or pe.FILE_HEADER.Machine != 0x14C:
        raise ValueError('unsupported executable architecture/base')
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    blocks = {}
    for name, (begin, end, expected) in CONTRACT.items():
        offset = pe.get_offset_from_rva(begin - 0x400000)
        raw = data[offset:offset + end - begin]
        digest = hashlib.sha256(raw).hexdigest()
        if digest != expected:
            raise ValueError(f'unsupported {name} at {begin:#x}: {digest}')
        instructions = list(md.disasm_lite(raw, begin))
        blocks[name] = {'begin': hex(begin), 'end': hex(end), 'sha256': digest,
                        'calls': [{'site': hex(a), 'target': op} for a, _, mnemonic, op in instructions if mnemonic == 'call'],
                        'assembly': '\n'.join(f'{a:08x} {mnemonic} {op}' for a, _, mnemonic, op in instructions)}
    shaders = []
    for match in re.finditer(rb'(?:vs|ps)\.1\.[14]\r?\n', data):
        start = match.start()
        end = data.index(b'\0', start)
        source = data[start:end].decode('ascii')
        shaders.append({'offset': hex(start), 'va': hex(pe.get_rva_from_offset(start) + 0x400000),
                        'sha256': hashlib.sha256(data[start:end]).hexdigest(),
                        'declarations': re.findall(r'^dcl_.*$', source, re.M),
                        'skinning_candidate': 'dcl_blendweight' in source,
                        'texture_outputs': sorted(set(re.findall(r'\boT\d+', source))),
                        'explicit_constants': sorted(set(map(int, re.findall(r'\bc(\d+)', source)))),
                        'source': source})
    draw_sites, indirect_candidates, shader_refs = [], [], []
    shader_addresses = {s['va'] for s in shaders}
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        raw = data[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData]
        for address, _, mnemonic, operands in md.disasm_lite(raw, 0x400000 + section.VirtualAddress):
            if mnemonic == 'call' and operands == '0x44d190':
                role = ('terrain_or_overlay' if 0x41DF50 <= address < 0x421B10 else
                        'water' if address == 0x422D24 else 'world_object' if address == 0x41B38C else 'unclassified')
                draw_sites.append({'site': hex(address), 'role': role})
            if mnemonic == 'call' and re.search(r'\+ 0x(?:144|148|14c|150)\]', operands):
                indirect_candidates.append({'site': hex(address), 'operand': operands,
                                            'warning': 'vtable offset alone does not establish a D3D9 call'})
            if mnemonic == 'push' and operands in shader_addresses:
                shader_refs.append({'site': hex(address), 'source': operands})
    declaration_offset = pe.get_offset_from_rva(0x794828 - 0x400000)
    declaration = [struct.unpack_from('<HHBBBB', data, declaration_offset + i * 8) for i in range(5)]
    result = {'exe_sha256': hashlib.sha256(data).hexdigest(), 'game_executed': False,
              'world_object_contract': {'declaration_va': '0x794828', 'declaration_elements': declaration,
                                        'base_shader': '0x77ae68', 'outline_shader': '0x77b008',
                                        'shader_slots': ['0x4ecdb24', '0x4ecdb28'],
                                        'world_matrix_constants': 'c30..c33',
                                        'base_texture_coordinates': ['oT0', 'oT1'],
                                        'fixed_unlit': {'fvf': ['0x142', '0x102', '0x2'], 'strides': [24, 20, 12], 'qualification': 'lighting/fog/specular/vertex blending disabled; supported material states'},
                  'native_implemented': True, 'game_validated': False},
              'model_contract': {'shader_wrapper_return': '0x44d22f', 'source_stack_word': 9,
                                 'shader_sources': ['0x793728', '0x793f70', '0x793928', '0x794150', '0x793bd8', '0x7943e0'],
                                 'declarations': ['0x794828', '0x7947f8', '0x7947c8'],
                                 'strides': [32, 36, 44], 'color_stream_stride': 4,
                                 'matrices': 'c30..c45', 'uv_offset': 'c46', 'height_fade': 'c47', 'outline_alpha': 'c48.w',
                                 'native_implemented': True, 'game_categories_verified': False,
                                 'capture_callers': ['0x42e600..0x432500', '0x44a600..0x44a900'],
                                 'qualification': 'known shader + exact declaration + initialized immutable buffer snapshots + supported states'},
              'additional_contracts': {
                  'unlit_model': {'shader': '0x7934d0', 'declaration': '0x74e994', 'stride': 24, 'topology': 'non-indexed triangle strip', 'draw': '0x442397'},
                  'dual_uv': {'shader': '0x769dc8', 'declaration': '0x7768d0', 'stride': 28, 'topology': 'indexed triangle strip', 'draw': '0x41271b', 'stage1_colorop': 13},
                  'screen': {'fvf': ['0x44', '0x104', '0x144'], 'strides': [20, 24, 28], 'state_blocks': ['0x42e7e0', '0x42ed80', '0x432a80'],
                             'qualification': 'shader null + exact FVF/stride + captured buffers + supported fixed-function states',
                             'up_calls': 'not established; DPUP/DIPUP remain fallback'},
                  'fixed_unlit': {'fvf': ['0x142', '0x102', '0x2'], 'strides': [24, 20, 12], 'qualification': 'lighting/fog/specular/vertex blending disabled; supported material states'},
                  'native_implemented': True, 'game_validated': False},
              'water_contract_verified': True, 'blocks': blocks, 'shaders': shaders,
              'shader_source_pushes': shader_refs, 'indexed_wrapper_calls': draw_sites,
              'indirect_draw_candidates': indirect_candidates,
              'limitations': ['static file only; runtime world-coordinate patches are not executed',
                              'virtual dispatch and inherited render state require further verification',
                              'unclassified calls are not labelled as UI, units, or vegetation without evidence']}
    output.mkdir(parents=True, exist_ok=True)
    for name, block in blocks.items():
        (output / f'{name}.asm').write_text(block.pop('assembly'), encoding='utf-8')
    for shader in shaders:
        (output / f"shader-{shader['va']}.vsh").write_text(shader.pop('source'), encoding='ascii')
    (output / 'render-paths.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / 'build/render-game/san11pk_render.exe')
    parser.add_argument('--out', type=Path, default=ROOT / 'build/offline-migration')
    args = parser.parse_args()
    result = audit(args.exe, args.out)
    print(f"PASS static water contract; shaders={len(result['shaders'])} indexed_sites={len(result['indexed_wrapper_calls'])}; no game executed")
