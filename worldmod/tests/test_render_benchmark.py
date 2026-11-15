"""Exercise the benchmark CLI's acceptance gate without launching the game."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import render_benchmark


STATUS = '''render requested=9on12 actual=9on12 terrain=original
Present measurement: CPU calls, not display FPS/GPU time; automation=0 failed_calls=0 device_resets=0 last_failure=00000000
Present foreground: samples=900 mean_ms=33.333 p95_ms=35.000 p99_ms=38.000 rate=30.00 call_mean_ms=1.000 call_p95_ms=2.000 span_ms=29999.700
Present background: samples=0 mean_ms=0.000 p95_ms=0.000 p99_ms=0.000 rate=0.00 call_mean_ms=0.000 call_p95_ms=0.000 span_ms=0.000
end
'''


class BenchmarkTests(unittest.TestCase):
    def capture(self, raw, valid, mode=0, anisotropy=0, terrain='original', before=None):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory)
            (game / 'worldmod.ini').write_text(
                '[san11pk_render]\nautomation=0\ntrace=0\nrender_backend=9on12\n'
                f'render_present_interval={mode}\nterrain_anisotropy={anisotropy}\nterrain_renderer={terrain}\n', encoding='utf-8')
            output = game / 'report.json'
            argv = ['render_benchmark', '--game-dir', str(game), '--scene', 'fixed-view', '--out', str(output)]
            if before is not None:
                argv.append('--native-profile')
            responses = [raw] if before is None else [before, raw]
            with patch.object(sys, 'argv', argv), patch.object(render_benchmark.gamectl, 'run', side_effect=responses), \
                    patch.object(render_benchmark.gamectl, 'GAME', game), contextlib.redirect_stdout(io.StringIO()):
                if valid:
                    render_benchmark.main()
                else:
                    with self.assertRaises(SystemExit):
                        render_benchmark.main()
            report = json.loads(output.read_text(encoding='utf-8'))
            self.assertEqual(report['valid_foreground_capture'], valid)
            self.assertEqual(report['raw_status'], raw)
            if before is not None:
                self.assertEqual(report['raw_status_before'], before)
                self.assertEqual(report['native_cpu'], render_benchmark.profile_delta(before, raw))
                self.assertEqual(report['native_gpu_upload'], render_benchmark.gpu_upload_delta(before, raw))
                self.assertEqual(report['native_flush'], render_benchmark.flush_delta(before, raw))

    def test_clean_foreground_capture(self):
        self.capture(STATUS, True)

    def test_waits_for_fresh_continuous_foreground_frames(self):
        fresh = STATUS.replace('span_ms=29999.700', 'span_ms=999.700')
        # Old paused data and background transitions must not start sampling.
        responses = [STATUS, fresh, fresh.replace('background: samples=0', 'background: samples=1'), fresh, fresh]
        with patch.object(render_benchmark.gamectl, 'run', side_effect=responses) as run, \
                patch.object(render_benchmark.time, 'monotonic', side_effect=[0, 0, 1, 2, 3, 4]):
            render_benchmark.wait_for_render(10)
            self.assertEqual(run.call_count, 5)
        with patch.object(render_benchmark.gamectl, 'run', return_value=STATUS), \
                patch.object(render_benchmark.time, 'monotonic', side_effect=[0, 0, 11]):
            with self.assertRaises(SystemExit):
                render_benchmark.wait_for_render(10)

    def test_requires_native_terrain_to_be_active(self):
        native = STATUS.replace('terrain=original', 'terrain=dx12_mixed') + (
            'native_submitted_draws=300 base=280 outline=10 lighting=10 enabled=1 failed=0\n'
            'native_shared_queue=1\n')
        self.capture(native, True, terrain='dx12')
        self.capture(native, False)
        self.capture(STATUS, False, terrain='dx12')
        for old, new in [('draws=300', 'draws=0'), ('enabled=1', 'enabled=0'),
                         ('failed=0', 'failed=1'), ('native_shared_queue=1', 'native_shared_queue=0')]:
            self.capture(native.replace(old, new), False, terrain='dx12')

    def test_native_profile_deltas_and_invalid_snapshots(self):
        def snapshot(calls, ms):
            return 'Native CPU profile: enabled=1 cumulative=1\n' + ''.join(
                f'Native CPU {name}: calls={calls} total_ms={ms:.6f}\n' for name in render_benchmark.CPU_STAGES) + (
                f'Native GPU profile: enabled=1 cumulative=1 batches={calls} total_ms={ms} pending=4 errors=0 scope=native_batches\n'
                f'Native upload profile: enabled=1 cumulative=1 bytes={calls*100} buffer_copies={calls} buffer_hits={calls} texture_copies={calls} texture_bytes={calls*256}\n')
        def detailed(calls, ms):
            return snapshot(calls, ms) + 'Native flush profile: enabled=1 cumulative=1 scope=active_batches\n' + ''.join(
                f'Native flush {name}: batches={calls} draws={calls*8} empty=0\n' for name in render_benchmark.FLUSH_REASONS)
        start, end = detailed(3, 2), detailed(8, 7)
        delta = render_benchmark.profile_delta(start, end)
        self.assertEqual(delta['draw'], {'calls': 5, 'total_ms': 5.0})
        native = STATUS.replace('terrain=original', 'terrain=dx12_mixed') + (
            'native_submitted_draws=300 base=280 outline=10 lighting=10 enabled=1 failed=0\n'
            'native_shared_queue=1\n')
        self.capture(native + end, True, terrain='dx12', before=native + start)
        delta = render_benchmark.flush_delta(start, end)
        self.assertEqual(delta['explicit'], {'batches': 5, 'draws': 40, 'empty': 0})
        for before, after in [(end, start), (start, start), ('', end),
                              (start, end.replace('enabled=1', 'enabled=0')),
                              (start, end.replace('scope=active_batches', 'scope=all_calls')),
                              (start, end.replace('Native flush upload_limit:', 'missing:'))]:
            self.assertIsNone(render_benchmark.flush_delta(before, after))
            self.capture(native + after, False, terrain='dx12', before=native + before)
        # Old profiles cannot certify the newly requested submission diagnosis.
        for prefix in ('Native CPU close:', 'Native CPU return_depth:', 'Native flush'):
            incomplete = '\n'.join(line for line in end.splitlines() if not line.startswith(prefix))
            self.capture(native + incomplete, False, terrain='dx12', before=native + start)
        self.capture(native, False, terrain='dx12', before=native)
        for before, after in [(end, start), (start, start), ('', end),
                              (start, end.replace('enabled=1', 'enabled=0')),
                              (start, end.replace('Native CPU wait:', 'missing:'))]:
            self.assertIsNone(render_benchmark.profile_delta(before, after))
        delta = render_benchmark.gpu_upload_delta(start, end)
        self.assertEqual(delta['gpu']['batches'], 5)
        self.assertEqual(delta['gpu']['total_ms'], 5)
        self.assertEqual(delta['upload']['bytes'], 500)
        for before, after in [(end, start), (start, start), ('', end),
                              (start, end.replace('errors=0', 'errors=1')),
                              (start, end.replace('enabled=1', 'enabled=0')),
                              (start, end.replace('pending=4', 'missing=4')),
                              (start, end.replace('scope=native_batches', 'scope=whole_frame')),
                              (start, end.replace('texture_bytes=', 'missing='))]:
            self.assertIsNone(render_benchmark.gpu_upload_delta(before, after))
        # An old DLL's CPU counters alone must not certify a complete profile.
        old_end = '\n'.join(line for line in end.splitlines() if not line.startswith(('Native GPU', 'Native upload')))
        self.capture(native + old_end, False, terrain='dx12', before=native + start)

    def test_requires_requested_presentation_mode(self):
        paced = STATUS + 'Presentation: windowed=1 interval=80000000 frame_limit=60\n'
        self.capture(paced, True, mode=3)
        self.capture(paced.replace('frame_limit=60', 'frame_limit=0'), False, mode=3)
        self.capture(paced.replace('interval=80000000', 'interval=00000000'), False, mode=3)
        self.capture(STATUS, False, mode=3)
        self.capture(STATUS.replace('requested=9on12 actual=9on12', 'requested=system actual=system'), False)

    def test_rejects_contaminated_or_incomplete_captures(self):
        substitutions = [
            ('device_resets=0', 'device_resets=1'),
            ('failed_calls=0', 'failed_calls=1'),
            ('automation=0', 'automation=1'),
            ('actual=9on12', 'actual=system'),
            ('background: samples=0', 'background: samples=1'),
            ('span_ms=29999.700', 'span_ms=14000.000'),
            ('span_ms=29999.700', 'span_ms=96466.039'),  # Stale ring while the game is paused.
            (' device_resets=0', ''),  # Old DLLs cannot certify reset-free captures.
        ]
        for old, new in substitutions:
            with self.subTest(reason=new):
                self.capture(STATUS.replace(old, new), False)

    def test_requires_filter_to_take_effect_without_state_errors(self):
        filtering = STATUS + 'Terrain filtering: requested=8 effective=8 applied_draws=45000 skipped_draws=500 errors=0\n'
        self.capture(filtering, True, anisotropy=8)
        self.capture(filtering.replace('effective=8', 'effective=4'), True, anisotropy=8)
        self.capture(STATUS, False, anisotropy=8)
        self.capture(filtering, False, anisotropy=0)
        for old, new in [('requested=8', 'requested=0'), ('effective=8', 'effective=0'),
                         ('effective=8', 'effective=16'), ('applied_draws=45000', 'applied_draws=0'),
                         ('errors=0', 'errors=1')]:
            with self.subTest(reason=new):
                self.capture(filtering.replace(old, new), False, anisotropy=8)
        self.capture(filtering, False, anisotropy=3)
        self.capture(STATUS + 'Terrain filtering: requested=0 effective=0 applied_draws=0 skipped_draws=0 errors=0\n', True)


if __name__ == '__main__':
    unittest.main()
