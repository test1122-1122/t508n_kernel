"""wifi-start loads the WCN modules without modules.dep (K55); mu300-hw restarts the AP once the country is live (K15)."""
import subprocess
import unittest

from helpers import BIN, TOP, ShellTest

HW = TOP / 'openwrt' / 'overlay' / 'etc' / 'init.d' / 'mu300-hw'


class WifiStart(ShellTest):
    def run_start(self, shell, flat=(), extra=(), modprobe_ok=()):
        kmod = self.tmp / 'kmod'
        (kmod / 'extra').mkdir(parents=True, exist_ok=True)
        for m in flat:
            (kmod / (m + '.ko')).write_text('')
        for m in extra:
            (kmod / 'extra' / (m + '.ko')).write_text('')
        log = self.tmp / 'calls'
        log.write_text('')
        self.stub('depmod', 'true')
        self.stub('sleep', 'true')
        self.stub('ls', 'true')
        self.stub('modprobe', 'echo "modprobe $*" >> "$STUBLOG/calls"\n'
                  'case " pcie-sprd-misc pcie-sprd %s " in *" $1 "*) exit 0;; esac\nexit 1' % ' '.join(modprobe_ok))
        self.stub('insmod', 'echo "insmod $*" >> "$STUBLOG/calls"')
        env = {'PATH': '%s:/usr/bin:/bin' % self.stubs, 'STUBLOG': str(self.tmp), 'MU300_KMOD': str(kmod)}
        r = subprocess.run(shell + [str(BIN / 'wifi-start')], capture_output=True, text=True, env=env)
        return r, log.read_text()

    def test_modprobe_first(self):
        for shell in self.shells:
            with self.subTest(shell=shell):
                r, calls = self.run_start(shell, modprobe_ok=['wcn_bsp', 'sprd_wlan_combo'])
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertNotIn('insmod', calls)

    def test_insmod_flat_when_modprobe_fails(self):
        for shell in self.shells:
            with self.subTest(shell=shell):
                r, calls = self.run_start(shell, flat=['wcn_bsp', 'sprd_wlan_combo'])
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertIn('insmod %s/kmod/wcn_bsp.ko' % self.tmp, calls)
                self.assertIn('insmod %s/kmod/sprd_wlan_combo.ko' % self.tmp, calls)

    def test_insmod_extra_dir(self):
        for shell in self.shells:
            with self.subTest(shell=shell):
                r, calls = self.run_start(shell, extra=['wcn_bsp', 'sprd_wlan_combo'])
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertIn('insmod %s/kmod/extra/wcn_bsp.ko' % self.tmp, calls)

    def test_missing_module_exits_1(self):
        for shell in self.shells:
            with self.subTest(shell=shell):
                r, calls = self.run_start(shell, flat=['sprd_wlan_combo'])
                self.assertEqual(r.returncode, 1)
                self.assertIn('missing wcn_bsp.ko', r.stderr)
                self.assertNotIn('insmod', calls)


class HwBringup(unittest.TestCase):
    def test_wifi_restarted_once_after_country(self):
        s = HW.read_text()
        down, up = s.index('wifi down'), s.index('wifi up')
        self.assertLess(s.index('iw reg set'), down)
        self.assertLess(down, up)
        self.assertEqual(s.count('wifi down'), 1)
        self.assertEqual(s.count('wifi up'), 1)
        self.assertLess(up, s.index('\nstart()'))


if __name__ == '__main__':
    unittest.main()
