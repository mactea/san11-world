"""Integration check of the x86 watcher against a disposable, windowless fixture."""
import json
import ctypes
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

BUILD = Path(__file__).resolve().parents[1] / 'build'


class CrashWatchTest(unittest.TestCase):
    def test_watcher_exit_leaves_child_alive(self):
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        kernel.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        with tempfile.TemporaryDirectory(prefix='crash-watch-', dir=BUILD) as temporary:
            env = os.environ.copy()
            env.pop('WORLDMOD_TEST_CRASH', None)
            env['WORLDMOD_TEST_WAIT'] = '1'
            with subprocess.Popen(
                [str(BUILD / 'render_crash_watch.exe'),
                 str(BUILD / 'crash_watch_fixture.exe'), str(Path(temporary) / 'detach')],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                creationflags=subprocess.CREATE_NO_WINDOW) as watcher:
                line = watcher.stdout.readline()
                self.assertTrue(line.startswith('START pid='), line)
                pid = int(line.split()[1].split('=')[1])
                handle = kernel.OpenProcess(0x100000 | 0x1000, False, pid)
                self.assertTrue(handle)
                try:
                    watcher.kill()
                    watcher.communicate(timeout=10)
                    self.assertEqual(kernel.WaitForSingleObject(handle, 100), 258)
                    self.assertEqual(kernel.WaitForSingleObject(handle, 10000), 0)
                    code = ctypes.c_ulong()
                    self.assertTrue(kernel.GetExitCodeProcess(handle, ctypes.byref(code)))
                    self.assertEqual(code.value, 0)
                finally:
                    kernel.CloseHandle(handle)

    def test_handled_and_unhandled_exceptions(self):
        with tempfile.TemporaryDirectory(prefix='crash-watch-', dir=BUILD) as temporary:
            root = Path(temporary)
            for crash in (False, True):
                output = root / ('crash' if crash else 'normal')
                env = os.environ.copy()
                env.pop('WORLDMOD_TEST_CRASH', None)
                env.pop('WORLDMOD_TEST_WAIT', None)
                if crash:
                    env['WORLDMOD_TEST_CRASH'] = '1'
                run = subprocess.run(
                    [str(BUILD / 'render_crash_watch.exe'),
                     str(BUILD / 'crash_watch_fixture.exe'), str(output)],
                    env=env, capture_output=True, text=True, timeout=30,
                    creationflags=subprocess.CREATE_NO_WINDOW)
                self.assertEqual(run.returncode, int(crash), run.stdout + run.stderr)
                dumps = list(output.glob('*.dmp'))
                self.assertEqual(len(dumps), int(crash), run.stdout)
                if not crash:
                    self.assertIn('EXIT code=00000000', run.stdout)
                    self.assertNotIn('UNHANDLED', run.stdout)
                    continue
                self.assertIn('success=1', run.stdout)
                self.assertIn('EXIT code=c0000005', run.stdout)
                data = dumps[0].read_bytes()
                magic, _, count, directory = struct.unpack_from('<4sIII', data)
                self.assertEqual(magic, b'MDMP')
                streams = dict((kind, (size, offset)) for kind, size, offset in
                               (struct.unpack_from('<III', data, directory + i * 12)
                                for i in range(count)))
                self.assertIn(9, streams, 'Full memory stream required')
                _, offset = streams[6]  # MINIDUMP_EXCEPTION_STREAM
                code = struct.unpack_from('<I', data, offset + 8)[0]
                fault = struct.unpack_from('<Q', data, offset + 48)[0]
                self.assertEqual(code, 0xc0000005)
                self.assertEqual(fault, 0x14)
                context_size, context_rva = struct.unpack_from('<II', data, offset + 160)
                self.assertGreaterEqual(context_size, 204)
                eip = struct.unpack_from('<I', data, context_rva + 184)[0]
                self.assertNotEqual(eip, 0, 'Exception thread x86 context required')
                print(json.dumps({'exception': hex(code), 'fault_address': hex(fault),
                                  'eip': hex(eip), 'dump_bytes': len(data)}))


if __name__ == '__main__':
    unittest.main()
