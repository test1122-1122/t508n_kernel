"""Portable adapter boundary of luci-app-mu300 (K81).

Ported from kanoqwq/clean-tf-7.2 (tests/test_unisoc_plugin.py) onto our tree. Ours differs on purpose: the adapters
read their uci options through the shared allow-lists of lib.sh (S1), so a saved-lock directory must be under
/etc/unisoc-modem or /etc/mu300 and a custom AT program must sit in the platform directory. The tests that need a
state directory therefore run a copy of the plugin's libexec/share tree whose /etc/unisoc-modem is the scratch
directory; the allow-list itself is tested on the unmodified lib.sh."""
import shutil
import threading

from helpers import ShellTest, TOP


APP = TOP / 'openwrt' / 'luci-app-mu300' / 'root'
AT = APP / 'usr' / 'libexec' / 'unisoc-modem' / 'at'
REPLAY = APP / 'usr' / 'libexec' / 'unisoc-modem' / 'boot-replay'
LOCK = APP / 'usr' / 'libexec' / 'unisoc-modem' / 'lock'
LIB = APP / 'usr' / 'share' / 'unisoc-modem' / 'lib.sh'


class Adapter(ShellTest):
    def tree(self):
        """A copy of the plugin's scripts with /etc/unisoc-modem and the runtime directory /var/run/unisoc-modem
        moved into the scratch directory; returns the scripts (at, boot-replay, lock) and the state directory the
        copy uses as its default."""
        root = self.tmp / 'tree'
        shutil.copytree(APP / 'usr' / 'libexec' / 'unisoc-modem', root / 'libexec' / 'unisoc-modem')
        shutil.copytree(APP / 'usr' / 'share' / 'unisoc-modem', root / 'share' / 'unisoc-modem')
        etc = self.tmp / 'etc' / 'unisoc-modem'
        for f in root.rglob('*'):
            if f.is_file():
                f.write_text(f.read_text().replace('/etc/unisoc-modem', str(etc))
                             .replace('/var/run/unisoc-modem', str(self.tmp / 'var-run' / 'unisoc-modem')))
        base = root / 'libexec' / 'unisoc-modem'
        return base / 'at', base / 'boot-replay', base / 'lock', etc / 'lock-state.d'

    def custom_program(self, name, body):
        """A program in the platform directory (the only place a custom AT command may live)."""
        plat = self.tmp / 'platform'
        plat.mkdir(exist_ok=True)
        p = plat / name
        p.write_text('#!/bin/sh\n' + body)
        p.chmod(0o755)
        return p

    def custom_uci(self, program, extra=''):
        self.stub('uci', f'''case "$3" in
unisoc_modem.main.at_backend) echo custom ;;
unisoc_modem.main.at_command) echo "{program}" ;;
{extra}esac''')


    def test_custom_at_contract(self):
        at, _, _, _ = self.tree()
        custom = self.custom_program('platform-at', 'printf "%s\\n" "$*" > "$STUBLOG/adapter.args"\nprintf "AT\\nOK\\n"\n')
        self.custom_uci(custom)
        for shell in self.each_shell():
            result = self.script(shell, at, '-t', '3', 'AT+CFUN?', MU300_PLATFORM_DIR=custom.parent)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('OK', result.stdout)
            self.assertEqual((self.tmp / 'adapter.args').read_text().strip(), '3 AT+CFUN?')

    def test_custom_at_outside_the_platform_directory_is_refused(self):
        # the fork let any executable be named; ours takes only a program directly in the platform directory (S1)
        elsewhere = self.tmp / 'elsewhere-at'
        elsewhere.write_text('#!/bin/sh\ntouch "$STUBLOG/was-called"\n')
        elsewhere.chmod(0o755)
        self.custom_uci(elsewhere)
        for shell in self.each_shell():
            result = self.script(shell, AT, 'AT', MU300_PLATFORM_DIR=self.tmp / 'platform')
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((self.tmp / 'was-called').exists())

    def test_available_does_not_send_at(self):
        at, _, _, _ = self.tree()
        custom = self.custom_program('platform-at', 'touch "$STUBLOG/was-called"\n')
        self.custom_uci(custom)
        for shell in self.each_shell():
            result = self.script(shell, at, '--available', MU300_PLATFORM_DIR=custom.parent)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((self.tmp / 'was-called').exists())

    def test_state_dir_outside_etc_is_ignored(self):
        # unmodified lib.sh: a state directory the UI could set under /tmp falls back to /etc/unisoc-modem (S1)
        state = self.tmp / 'state'
        state.mkdir()
        (state / 'mode').write_text('4g')
        self.stub('uci', f'case "$3" in unisoc_modem.main.state_dir) echo "{state}" ;; esac')
        marker = self.tmp / 'replayed'
        for shell in self.each_shell():
            result = self.sh(shell, f'. {LIB}; safe_state_dir "$(uci_get state_dir)" /etc/unisoc-modem/lock-state.d')
            self.assertEqual(result.stdout.strip(), '/etc/unisoc-modem/lock-state.d', result.stderr)
            result = self.script(shell, REPLAY, UNISOC_REPLAY_MARKER=marker,
                                 UNISOC_AT_BIN=self.custom_program('at', 'touch "$STUBLOG/at-probed"\n'))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((self.tmp / 'at-probed').exists())

    def test_disabled_replay_has_zero_at_traffic(self):
        _, replay, _, state = self.tree()
        state.mkdir(parents=True)
        (state / 'auto_apply').write_text('off')
        (state / 'mode').write_text('4g')
        self.stub('uci', 'exit 0')
        self.stub('grep', 'touch "$STUBLOG/at-was-probed"; exit 1')
        probe = self.custom_program('at', 'touch "$STUBLOG/at-probed"\n')
        for shell in self.each_shell():
            result = self.script(shell, replay, UNISOC_AT_BIN=probe)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((self.tmp / 'at-was-probed').exists())
            self.assertFalse((self.tmp / 'at-probed').exists())

    def test_ready_at_replays_saved_locks_immediately(self):
        _, replay, _, state = self.tree()
        state.mkdir(parents=True)
        (state / 'auto_apply').write_text('on')
        (state / 'mode').write_text('4g')
        at = self.custom_program('at', 'printf "AT\\nOK\\n"\n')
        lock = self.custom_program('lock', 'printf "%s\\n" "$*" > "$STUBLOG/replay.args"\n')
        self.stub('uci', '''case "$3" in
unisoc_modem.main.replay_phase) echo early ;; # obsolete values must not weaken the generic fallback
unisoc_modem.main.replay_timeout) echo 10 ;;
esac''')
        for shell in self.each_shell():
            result = self.script(shell, replay, UNISOC_AT_BIN=at, UNISOC_LOCK_BIN=lock)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.tmp / 'replay.args').read_text().strip(), 'replay late')

    def test_platform_early_window_suppresses_probe_and_late_replay(self):
        _, replay, _, state = self.tree()
        state.mkdir(parents=True)
        (state / 'mode').write_text('4g')
        marker = self.tmp / 'replayed'
        pending = self.tmp / 'pending'
        at = self.custom_program('at', 'touch "$STUBLOG/at-probed"\necho OK\n')
        lock = self.custom_program('lock', 'touch "$STUBLOG/late-called"\n')
        self.stub('uci', 'case "$3" in unisoc_modem.main.replay_timeout) echo 10 ;; esac')
        for shell in self.each_shell():
            marker.unlink(missing_ok=True)
            pending.touch()

            def finish_early():
                pending.unlink(missing_ok=True)
                marker.touch()
            timer = threading.Timer(0.1, finish_early)
            timer.start()
            try:
                result = self.script(shell, replay, UNISOC_AT_BIN=at, UNISOC_LOCK_BIN=lock,
                                     UNISOC_EARLY_PENDING=pending, UNISOC_REPLAY_MARKER=marker)
            finally:
                timer.join()
            self.assertEqual(result.returncode, 0,
                             f'{result.stderr}; pending={pending.exists()} marker={marker.exists()}')
            self.assertFalse((self.tmp / 'at-probed').exists())
            self.assertFalse((self.tmp / 'late-called').exists())

    def test_early_replay_restores_every_saved_lock_without_sfun(self):
        _, _, lock, state = self.tree()
        state.mkdir(parents=True)
        for k, v in (('auto_apply', 'on'), ('mode', 'nsa'), ('endc', 'on'), ('lte', '1,3,5'), ('nr', '41,78'),
                     ('cell', 'lte:1650,211\nnr:627264,393\n')):
            (state / k).write_text(v)
        at = self.tmp / 'at'
        at.write_text('''#!/bin/sh
printf "%s\\n" "$*" >> "$STUBLOG/at.commands"
case "$*" in
  *'AT+SPTESTMODE?'*) printf '+SPTESTMODE: 131,134,0\\nOK\\n' ;;
  *'AT+SP5GRAN?'*) printf '+SP5GRAN: 0\\nOK\\n' ;;
  *'AT+SPENDC?'*) printf '+ENDC: 1\\nOK\\n' ;;
  *'AT+SPLBAND=0'*) printf '+SPLBAND: 0,0,0,21,0\\nOK\\n' ;;
  *'AT+SPLBAND=3'*) printf '+SPLBAND: 0,0,272\\nOK\\n' ;;
  *'AT+SPFORCEFRQ=12,3'*) printf '+SPFORCEFRQ: 12,3,1650,211\\nOK\\n' ;;
  *'AT+SPFORCEFRQ=16,3'*) printf '+SPFORCEFRQ: 16,3,627264,393\\nOK\\n' ;;
  *) printf 'OK\\n' ;;
esac
''')
        at.chmod(0o755)
        self.stub('uci', 'case "$3" in unisoc_modem.main.data_interface) echo cellular ;; esac')
        # Band masks use POSIX awk arithmetic, so the host awk must work too.
        apply_dir = self.tmp / 'apply'
        marker = self.tmp / 'replayed'
        for shell in self.each_shell():
            (self.tmp / 'at.commands').unlink(missing_ok=True)
            marker.unlink(missing_ok=True)
            result = self.script(shell, lock, 'replay', 'early', MU300_AT=at,
                                 UNISOC_APPLY_DIR=apply_dir, UNISOC_REPLAY_MARKER=marker)
            self.assertEqual(result.returncode, 0, result.stderr)
            commands = (self.tmp / 'at.commands').read_text()
            self.assertIn('AT+SP5GRAN=0', commands)
            self.assertIn('AT+SPTESTMODE=131,134,0', commands)
            self.assertIn('AT+SPENDC=1', commands)
            self.assertIn('AT+SPLBAND=1,0,0,0,21,0', commands, result.stderr)
            self.assertIn('AT+SPLBAND=2,0,0,272,0', commands)
            self.assertIn('AT+SPFORCEFRQ=12,6,1650,211', commands)
            self.assertIn('AT+SPFORCEFRQ=16,6,627264,393', commands)
            self.assertNotIn('AT+SFUN=', commands)
            self.assertTrue(marker.exists())

    def test_the_stack_restart_takes_the_radio_lock(self):
        """Final review minor 3: lock apply's and a late replay's SFUN restart run under mobile-data's radio lock
        (mobile-data radio-locked, waiting up to 120 s for the dial or the watchdog); when it stays busy, no SFUN is
        sent and the replay still finishes (the saved settings were sent; they apply at the radio's next restart)."""
        _, _, lock, state = self.tree()
        at = self.tmp / 'at'
        at.write_text('#!/bin/sh\necho "$* locked=${MU300_RADIO_LOCKED:-}" >> "$STUBLOG/at.commands"\n'
                      'case "$*" in *"AT+SPTESTMODE?"*) echo "+SPTESTMODE: 134,134,0" ;; *"AT+CFUN?"*) echo "+CFUN: 1" ;; esac\n'
                      'echo OK\n')
        at.chmod(0o755)
        md = self.tmp / 'mobile-data'
        md.write_text('#!/bin/sh\necho "$* wait=${MU300_RADIO_LOCK_WAIT:-}" >> "$STUBLOG/md"\n'
                      '[ "$1" = radio-locked ] || exit 0\n'
                      '[ -n "${MD_BUSY:-}" ] && exit 75\nshift; exec "$@"\n')
        md.chmod(0o755)
        self.stub('uci', 'exit 0')
        self.stub('ifup', 'exit 0')
        self.stub('sleep', 'exit 0')
        marker = self.tmp / 'replayed'
        for shell in self.each_shell():
            for busy in ('', '1'):
                with self.subTest(busy=busy):
                    state.mkdir(parents=True, exist_ok=True)
                    (state / 'mode').write_text('4g')
                    for f in ('at.commands', 'md'):
                        (self.tmp / f).unlink(missing_ok=True)
                    marker.unlink(missing_ok=True)
                    r = self.script(shell, lock, 'replay', MU300_AT=at, MU300_MOBILE_DATA=md, MD_BUSY=busy,
                                    UNISOC_APPLY_DIR=self.tmp / 'apply', UNISOC_REPLAY_MARKER=marker)
                    self.assertEqual(r.returncode, 0, r.stderr)
                    self.assertTrue(marker.exists())
                    self.assertEqual((self.tmp / 'md').read_text().splitlines(),
                                     [f'radio-locked {lock} sfun-restart late wait=120'])
                    sent = (self.tmp / 'at.commands').read_text().splitlines()
                    sfun = [l for l in sent if 'AT+SFUN=' in l]
                    if busy:
                        self.assertEqual(sfun, [])
                    else:
                        self.assertEqual(sfun[:2], ['-t 10 AT+SFUN=5 locked=1', '-t 30 AT+SFUN=4 locked=1'])

    def test_early_replay_readback_failure_keeps_late_fallback(self):
        _, _, lock, state = self.tree()
        state.mkdir(parents=True)
        (state / 'mode').write_text('4g')
        at = self.tmp / 'at'
        at.write_text('#!/bin/sh\ncase "$*" in *"AT+SPTESTMODE?"*) echo "+SPTESTMODE: 134,134,0" ;; esac\n')
        at.chmod(0o755)
        self.stub('uci', 'exit 0')
        marker = self.tmp / 'replayed'
        for shell in self.each_shell():
            result = self.script(shell, lock, 'replay', 'early', MU300_AT=at,
                                 UNISOC_APPLY_DIR=self.tmp / 'apply', UNISOC_REPLAY_MARKER=marker)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(marker.exists())


if __name__ == '__main__':
    import unittest
    unittest.main()
