"""tools/android-boot-image.sh: the parts of the Linux boot image only the device can make - its misc blocks, its
Android files, the device ramdisk segment - checked against boot/build-boot-image.py on the same inputs."""
import importlib.util
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import tarfile
import unittest

from helpers import BIN, TOP, ShellTest
from test_boot_image import HAVE_LZ4, LIVE_A, cpio_files, fake_misc, fake_stock, unlz4_legacy

LIB = TOP / 'tools' / 'android-boot-image.sh'
spec = importlib.util.spec_from_file_location('bbi', TOP / 'boot' / 'build-boot-image.py')
bbi = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bbi)

# magiskboot compress=lz4_legacy IN OUT and decompress IN OUT, done by the builder's own lz4_legacy and the tests'
# unlz4_legacy (the lz4 command or module)
MAGISKBOOT = f'''#!/bin/sh
case $1 in
    compress=lz4_legacy) f='m.lz4_legacy' ;;
    decompress) f='unlz4_legacy' ;;
    *) exit 1 ;;
esac
exec "{sys.executable}" -c "import importlib.util,sys; sys.path.insert(0,'{TOP}/tests'); from test_boot_image import unlz4_legacy; s=importlib.util.spec_from_file_location('b','{TOP}/boot/build-boot-image.py'); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); open(sys.argv[2],'wb').write($f(open(sys.argv[1],'rb').read()))" "$2" "$3"
'''

# the same, but compress ends the frame with the uncompressed size, as Magisk's lz4_lg does
MAGISKBOOT_SIZE_WORD = f'''"$STUBLOG/stubs/magiskboot" "$@" || exit 1
case $1 in compress=*) exec "{sys.executable}" -c "import os,struct,sys; open(sys.argv[2],'ab').write(struct.pack('<I', os.path.getsize(sys.argv[1])))" "$2" "$3" ;; esac
'''


def cpio_all(data):
    """every newc archive of a byte string (as the kernel reads concatenated segments), later files win"""
    files, i = {}, 0
    while i < len(data):
        if data[i:i + 6] != b'070701':
            i += 1
            continue
        part = cpio_files(data[i:])
        files.update(part)
        i = data.index(b'TRAILER!!!', i) + 10
    return files


def subset_paths():
    lines = (TOP / 'android-vendor' / 'subset-files.txt').read_text().splitlines()
    return [l for l in lines if l.strip() and not l.startswith('#')]


def fake_android_root(root):
    """a / with every file of the subset list (dirs get one file inside) and the firmware"""
    for p in subset_paths():
        f = root / p
        if p.endswith(('bionic', '__properties__')):
            f.mkdir(parents=True, exist_ok=True)
            (f / 'u:object_r:x:s0' if '__properties__' in p else f / 'libc.so').write_bytes(b'data ' + p.encode())
        else:
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_bytes(b'data ' + p.encode())
    os.chmod(root / 'vendor/bin/modem_control', 0o755)
    fw = root / 'vendor/firmware'
    fw.mkdir(parents=True, exist_ok=True)
    for n in ('wcnmodem.bin', 'gnssmodem.bin', 'wifi_board_config.ini', 'wifi_board_config_ab.ini',
              'bt_configure_pskey.ini', 'bt_configure_rf.ini'):
        (fw / n).write_bytes(b'fw ' + n.encode())


def ramdisk_of(img):
    ksz, rsz = struct.unpack_from('<II', img, 8)
    roff = 4096 + (ksz + 4095) // 4096 * 4096
    return img[roff:roff + rsz]


def leaves(files):
    """the regular files and symlinks of cpio_all's result: kind, executable or not, contents"""
    return {n: (stat.S_IFMT(m), bool(m & 0o111), d) for n, (m, d) in files.items()
            if stat.S_ISREG(m) or stat.S_ISLNK(m)}


class Lists(unittest.TestCase):
    def test_subset_list_is_extract_subsets(self):
        sh = (TOP / 'android-vendor' / 'extract-subset.sh').read_text()
        m = re.search(r'tar -cf - (apex/.*?) \| tar -xf -', sh, re.S)
        self.assertEqual(m.group(1).replace('\\\n', ' ').split(), subset_paths())


class DeviceSide(ShellTest):
    def setUp(self):
        super().setUp()
        self.stub('magiskboot', MAGISKBOOT)

    def lib(self, shell, code, magiskboot='magiskboot', **env):
        pre = f'MU300_LIB=1 . "{BIN}/mu300-update"; . "{LIB}"; '
        return self.sh(shell, pre + code, MAGISKBOOT=self.stubs / magiskboot, **env)

    def test_blocks_equal_the_python_builders(self):
        want = dict(zip(('slot-a', 'slot-b-trial', 'slot-b', 'slot-a-trial'),
                        bbi.bootloader_control(bytes(0x800) + LIVE_A)))
        for shell in self.each_shell():
            d = self.tmp / 'etc'
            shutil.rmtree(d, ignore_errors=True); d.mkdir()
            r = self.lib(shell, f'bc_valid {LIVE_A.hex()} && bc_files {LIVE_A.hex()} "{d}"')
            self.assertEqual(r.returncode, 0, r.stderr)
            for n, data in want.items():
                self.assertEqual((d / f'misc-bc-{n}.bin').read_bytes(), data, (shell, n))

    def test_invalid_live_block(self):
        bad = bytearray(LIVE_A); bad[20] ^= 1
        for shell in self.each_shell():
            self.assertNotEqual(self.lib(shell, f'bc_valid {bytes(bad).hex()}').returncode, 0)
            self.assertNotEqual(self.lib(shell, 'bc_valid 00').returncode, 0)

    def test_subset_and_firmware_from_the_device(self):
        root = self.tmp / 'root'
        fake_android_root(root)
        for shell in self.each_shell():
            s, fw = self.tmp / 'subset', self.tmp / 'fw'
            r = self.lib(shell, f'collect_subset "{TOP}/android-vendor/subset-files.txt" "{s}" && collect_firmware "{fw}"',
                         MU300_ANDROID_ROOT=root, MU300_FW_DIRS=f'{root}/odm/firmware {root}/vendor/firmware')
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertTrue((s / 'vendor/bin/modem_control').is_file())
            self.assertEqual(os.readlink(s / 'system/bin/linker64'), '/apex/com.android.runtime/bin/linker64')
            self.assertEqual((s / 'linkerconfig/ld.config.txt').read_bytes(), b'')
            self.assertEqual(sorted(os.listdir(fw)), sorted(os.listdir(root / 'vendor/firmware')))
            shutil.rmtree(fw)
            (root / 'vendor/firmware/gnssmodem.bin').rename(root / 'gnss.bak')
            r = self.lib(shell, f'collect_firmware "{fw}"', MU300_FW_DIRS=f'{root}/vendor/firmware')
            self.assertNotEqual(r.returncode, 0)
            self.assertIn('gnssmodem.bin', r.stdout)
            (root / 'gnss.bak').rename(root / 'vendor/firmware/gnssmodem.bin')
            shutil.rmtree(s); shutil.rmtree(fw, ignore_errors=True)

    def test_vendor_overlay_layout(self):
        # the layout of tools/vendor-overlay.py: firmware, the subset with its property area moved out of dev/
        root = self.tmp / 'root'
        fake_android_root(root)
        for shell in self.each_shell():
            for os_name, fwdir in (('ubuntu', 'usr/lib/firmware'), ('openwrt', 'lib/firmware')):
                out = self.tmp / f'v-{os_name}.tar.gz'
                r = self.lib(shell, f'collect_subset "{TOP}/android-vendor/subset-files.txt" "{self.tmp}/s" && '
                                    f'collect_firmware "{self.tmp}/fw" && vendor_overlay {os_name} "{self.tmp}/fw" "{self.tmp}/s" "" "{out}"',
                             MU300_ANDROID_ROOT=root, MU300_FW_DIRS=f'{root}/vendor/firmware')
                self.assertEqual(r.returncode, 0, r.stderr)
                names = subprocess.run(['tar', '-tzf', str(out)], capture_output=True, text=True).stdout.split()
                names = [n.lstrip('./') for n in names]
                self.assertIn(f'{fwdir}/wcnmodem.bin', names)
                self.assertIn('opt/mu300/android/vendor/bin/modem_control', names)
                self.assertTrue(any(n.startswith('opt/mu300/android/dev-properties/') for n in names))
                self.assertFalse([n for n in names if n.startswith('opt/mu300/android/dev/')])
                # no entry for a directory the system already has: Ubuntu's lib is a link to usr/lib
                self.assertFalse([n for n in names if n.rstrip('/') in ('lib', 'usr', 'usr/lib', 'opt', 'opt/mu300')])
                shutil.rmtree(self.tmp / 's'); shutil.rmtree(self.tmp / 'fw')

    def bundle_inputs(self):
        """one set of inputs: Android's root, its boot image and misc, a kernel bundle (Image and generic segment)
        built from fake modules and tools; returns the builder's command line arguments they share"""
        self.root = self.tmp / 'root'
        fake_android_root(self.root)
        self.stock, self.misc = self.tmp / 'stock.img', self.tmp / 'misc.bin'
        fake_stock(self.stock)
        fake_misc(self.misc)
        mods, self.bundle = self.tmp / 'modules', self.tmp / 'bundle'
        mods.mkdir(); self.bundle.mkdir()
        for n in (TOP / 'boot' / 'module-order.txt').read_text().split():
            (mods / n).write_bytes(b'\x7fELF base ' + n.encode())
        for f in ('busybox', 'logdw'):
            (self.tmp / f).write_bytes(b'\x7fELF ' + f.encode())
        (self.bundle / 'Image').write_bytes(b'\x7fkernel' * 1000)
        self.common = ['--modules', str(mods), '--busybox', str(self.tmp / 'busybox'),
                       '--logdw', str(self.tmp / 'logdw'), '--ueventd-perms', str(TOP / 'android-vendor' / 'ueventd-perms.sh')]
        self.builder = [sys.executable, str(TOP / 'boot' / 'build-boot-image.py')]
        r = subprocess.run(self.builder + ['--generic-ramdisk', '--out', str(self.bundle / 'ramdisk-generic.lz4')] +
                           self.common, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)

    def device_image(self, shell, magiskboot='magiskboot'):
        """the device side, as the installer does it: work/dev.lz4 and work/out/{new,vbmeta,newfooter}"""
        work = self.tmp / 'work'
        shutil.rmtree(work, ignore_errors=True)
        seg, out = work / 'seg', work / 'out'
        seg.mkdir(parents=True); out.mkdir()
        r = self.lib(shell, f'mkdir -p "{seg}/etc" && echo f50 > "{seg}/etc/mu300-device" && '
                            f'echo b > "{seg}/etc/mu300-linux-slot" && bc_files {LIVE_A.hex()} "{seg}/etc" && '
                            f'collect_subset "{TOP}/android-vendor/subset-files.txt" "{seg}/android" && '
                            f'device_segment "{seg}" "{work}/dev.lz4" && '
                            f'linux_boot_image "{self.stock}" "{self.bundle}" "{work}/dev.lz4" "{out}"',
                     magiskboot=magiskboot, MU300_ANDROID_ROOT=self.root)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse((work / 'dev.lz4.cpio').exists())
        return work

    @unittest.skipIf(not HAVE_LZ4, 'no lz4')
    def test_whole_image_holds_what_the_python_builder_puts_in(self):
        self.bundle_inputs()
        bundle = self.bundle
        for shell in self.each_shell():
            work = self.device_image(shell)
            seg, out = work / 'seg', work / 'out'
            # the same on a computer, with the subset the device collected
            py = work / 'py.img'
            r = subprocess.run(self.builder + ['--stock-boot', str(self.stock), '--misc-head', str(self.misc),
                                               '--kernel', str(bundle / 'Image'),
                                               '--append-ramdisk', str(bundle / 'ramdisk-generic.lz4'),
                                               '--device', 'f50', '--linux-slot', 'b',
                                               '--android-subset', str(seg / 'android'), '--out', str(py)] + self.common,
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)

            pyimg, new = py.read_bytes(), (out / 'new').read_bytes()
            got = leaves(cpio_all(unlz4_legacy(ramdisk_of(new))))
            self.assertEqual(got, leaves(cpio_all(unlz4_legacy(ramdisk_of(pyimg)))))
            self.assertTrue(got['android/vendor/bin/modem_control'][1], 'modem_control is not executable')
            self.assertEqual(got['etc/mu300-device'][2], b'f50\n')

            # the kernel does not create the parents of what it unpacks: the device segment names every one
            devseg = cpio_files(unlz4_legacy((work / 'dev.lz4').read_bytes()))
            dirs = {n for n, (m, _) in devseg.items() if stat.S_ISDIR(m)}
            for n in devseg:
                parts = n.split('/')
                for i in range(1, len(parts)):
                    self.assertIn('/'.join(parts[:i]), dirs, n)

            # the header page: Android's, with this kernel's and ramdisk's sizes and the marker the switch looks for
            ksz, rsz = struct.unpack_from('<II', new, 8)
            self.assertEqual(ksz, (bundle / 'Image').stat().st_size)
            self.assertEqual(rsz, (work / 'dev.lz4').stat().st_size + (bundle / 'ramdisk-generic.lz4').stat().st_size)
            self.assertEqual(new[44:44 + 1536], b'loglevel=5'.ljust(1536, b'\0'))
            self.assertEqual(new[:8] + new[16:4096], pyimg[:8] + pyimg[16:4096])
            self.assertEqual(len(new) % 4096, 0)
            # Android's vbmeta and its footer, with the sizes of this image
            osz, _, vbs = struct.unpack_from('>QQQ', pyimg, len(pyimg) - 64 + 12)
            self.assertEqual((out / 'vbmeta').read_bytes(), pyimg[osz:osz + vbs])
            foot = (out / 'newfooter').read_bytes()
            self.assertEqual(struct.unpack_from('>QQQ', foot, 12), (len(new), len(new), vbs))
            self.assertEqual(foot[:12] + foot[36:], pyimg[-64:-52] + pyimg[-28:])

    @unittest.skipIf(not HAVE_LZ4, 'no lz4')
    def test_a_size_word_behind_the_frame_does_not_hide_the_generic_segment(self):
        # in front of the generic segment, the kernel would read a trailing size word as a chunk size
        self.stub('magiskboot-lg', MAGISKBOOT_SIZE_WORD)
        self.bundle_inputs()
        generic = cpio_files(unlz4_legacy((self.bundle / 'ramdisk-generic.lz4').read_bytes()))
        for shell in self.each_shell():
            work = self.device_image(shell, 'magiskboot-lg')
            files = cpio_all(unlz4_legacy(ramdisk_of((work / 'out' / 'new').read_bytes())))
            self.assertEqual(files['init'][1], (TOP / 'boot' / 'init').read_bytes())
            for n in generic:
                self.assertIn(n, files)
            self.assertIn('android/vendor/bin/modem_control', files)
            self.assertEqual(files['etc/mu300-device'][1], b'f50\n')

    def test_device_segment_refuses_a_bad_frame_and_leaves_no_copy(self):
        d = self.tmp / 'd'
        (d / 'etc').mkdir(parents=True)
        (d / 'etc' / 'x').write_bytes(b'proprietary')
        # no whole chunk behind the magic; a frame that does not give back the cpio
        self.stub('mb-short', 'case $1 in compress=*) printf "\\002\\041\\114\\030\\377\\000\\000\\000ab" > "$3" ;; esac')
        self.stub('mb-wrong', 'case $1 in compress=*) printf "\\002\\041\\114\\030\\002\\000\\000\\000ab" > "$3" ;; '
                              'decompress) printf other > "$3" ;; esac')
        # magiskboot's own reason, kept: on the device it is the only clue
        self.stub('mb-err', 'echo "magiskboot: No space left on device" >&2; exit 1')
        for shell in self.each_shell():
            for mb, why in (('mb-short', 'no whole LZ4 chunk'), ('mb-wrong', 'does not decompress'),
                            ('mb-err', 'No space left on device')):
                r = self.lib(shell, f'device_segment "{d}" "{self.tmp}/seg.lz4"', magiskboot=mb)
                self.assertNotEqual(r.returncode, 0, mb)
                self.assertIn(why, r.stderr)
                self.assertEqual([p.name for p in self.tmp.glob('seg.lz4*')], [], mb)

    def gpu_root(self):
        root = self.tmp / 'groot'
        paths = ['vendor/lib64/libOpenCL.so', 'vendor/lib64/egl/libGLES_mali.so', 'system/lib64/libc.so']
        for p in paths:
            (root / p).parent.mkdir(parents=True, exist_ok=True)
            (root / p).write_bytes(b'gpu ' + p.encode())
        lst = self.tmp / 'gpu-files.txt'
        lst.write_text('# a header\n' + '\n'.join(paths) + '\n')
        return root, lst, paths

    def test_collect_gpu_all_or_nothing(self):
        root, lst, paths = self.gpu_root()
        g = self.tmp / 'gpu'
        for shell in self.each_shell():
            r = self.lib(shell, f'collect_gpu "{lst}" "{g}"', MU300_ANDROID_ROOT=root)
            self.assertEqual(r.returncode, 0, r.stderr)
            for p in paths:
                self.assertEqual((g / p).read_bytes(), b'gpu ' + p.encode())
            (root / paths[1]).rename(self.tmp / 'gone')
            r = self.lib(shell, f'collect_gpu "{lst}" "{g}"', MU300_ANDROID_ROOT=root)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(g.exists())
            (self.tmp / 'gone').rename(root / paths[1])

    def test_vendor_overlay_gpu_files(self):
        root = self.tmp / 'root'
        fake_android_root(root)
        g = self.tmp / 'gpu'
        (g / 'vendor/lib64').mkdir(parents=True)
        (g / 'vendor/bin').mkdir(parents=True)
        (g / 'vendor/lib64/libOpenCL.so').write_bytes(b'opencl')
        (g / 'vendor/bin/modem_control').write_bytes(b'not the subset one')
        out = self.tmp / 'v.tar.gz'
        pre = (f'collect_subset "{TOP}/android-vendor/subset-files.txt" "{self.tmp}/s" && '
               f'collect_firmware "{self.tmp}/fw" && ')
        for shell in self.each_shell():
            r = self.lib(shell, pre + f'vendor_overlay ubuntu "{self.tmp}/fw" "{self.tmp}/s" "{g}" "{out}"',
                         MU300_ANDROID_ROOT=root, MU300_FW_DIRS=f'{root}/vendor/firmware')
            self.assertEqual(r.returncode, 0, r.stderr)
            with tarfile.open(out) as t:
                files = {m.name.lstrip('./'): t.extractfile(m).read() for m in t.getmembers() if m.isreg()}
            self.assertEqual(files['opt/mu300/android/vendor/lib64/libOpenCL.so'], b'opencl')
            # the subset's file wins
            self.assertEqual(files['opt/mu300/android/vendor/bin/modem_control'], b'data vendor/bin/modem_control')
            self.assertFalse(list(self.tmp.glob('v.tar.gz.*')))
            # a GPU file that cannot be copied fails the overlay (here its directory is a file of the subset), and
            # leaves neither the archive nor the copies behind
            bad = self.tmp / 'gpu-bad'
            (bad / 'vendor/bin/modem_control').mkdir(parents=True)
            (bad / 'vendor/bin/modem_control/x').write_bytes(b'x')
            r = self.lib(shell, pre + f'vendor_overlay ubuntu "{self.tmp}/fw" "{self.tmp}/s" "{bad}" "{out}"',
                         MU300_ANDROID_ROOT=root, MU300_FW_DIRS=f'{root}/vendor/firmware')
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(list(self.tmp.glob('v.tar.gz*')))
            shutil.rmtree(bad); shutil.rmtree(self.tmp / 's'); shutil.rmtree(self.tmp / 'fw')

if __name__ == '__main__':
    unittest.main()
