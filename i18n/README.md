# Installer translations

`install.sh` (with `tools/linux-mode.sh` and `tools/self-update.sh`) and `install.ps1` show their messages through
`t` / `T`, which look them up here: `<lang>.tsv` has one line per message,

    English message with {1}<TAB>translation with {1}

where `{1}`..`{6}` are the arguments, in any order the language needs. Lines starting with `#` are comments. A
message without a line stays in English, so an incomplete translation never breaks the installer.

To add a language:

1. `python3 tools/check-i18n.py --keys` prints every message; translate them into `<lang>.tsv` (UTF-8, a real tab
   between the two columns). Keep commands, paths and the words to type (`INSTALL`, `ERASE`, `overwrite`,
   `update`/`wipe`) as they are.
2. Add the language to `choose_language` in `tools/i18n.sh` and to the menu in `install.ps1` (write its name there
   with `[char]` code points: the script has to stay ASCII for Windows PowerShell 5.1), and its words for yes/no
   to `normalize_answer` / `NormalizeAnswer`.
3. `python3 tools/check-i18n.py` must report nothing missing and no placeholder mismatches.

When a message changes in a script, its old line here turns up as "stale" and the new one as "missing".
