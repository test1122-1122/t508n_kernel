# Tests for install.ps1's own functions, under Windows PowerShell 5.1 and PowerShell 7 (no Pester needed):
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\installer.Tests.ps1
# The functions are taken out of install.ps1 with the PowerShell parser and defined here, so the installer itself
# never runs. Exit code: the number of failed checks.
$ErrorActionPreference = 'Stop'
$Top = Split-Path -Parent $PSScriptRoot
$script:failed = 0
$script:passed = 0
function Check($name, $got, $want) {
    if ([string]$got -ceq [string]$want) { $script:passed++ }
    else { $script:failed++; Write-Host "FAIL $name`n  got:  [$got]`n  want: [$want]" -ForegroundColor Red }
}

# the functions under test, straight from install.ps1
$ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $Top "install.ps1"), [ref]$null, [ref]$null)
$want = 'LoadLanguage', 'T', 'NormalizeAnswer', 'Gib', 'ChooseStorage', 'StorageDefault', 'InternalOverCard', 'SdState', 'SdKernelOk', 'InstallEnvText', 'ChooseOpenWrt'
$defs = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $want -contains $n.Name }, $true)
foreach ($d in $defs) { . ([scriptblock]::Create($d.Extent.Text)) }
foreach ($w in 'LoadLanguage', 'T', 'NormalizeAnswer') {
    if (-not (Get-Command $w -CommandType Function -ErrorAction SilentlyContinue)) { Write-Host "FAIL: $w not found in install.ps1"; exit 99 }
}
$script:Msg = New-Object 'System.Collections.Generic.Dictionary[string,string]'

# ---- T: every translation, every placeholder --------------------------------------------------------------------
$argsT = 'A1', 'B2', 'C3', 'D4', 'E5', 'F6'
foreach ($lang in 'tr', 'zh') {
    LoadLanguage $lang
    $lines = [IO.File]::ReadAllLines((Join-Path (Join-Path $Top "i18n") "$lang.tsv"), [Text.Encoding]::UTF8)
    foreach ($line in $lines) {
        $i = $line.IndexOf("`t")
        if ($i -le 0 -or $line.StartsWith('#')) { continue }
        $key = $line.Substring(0, $i); $val = $line.Substring($i + 1)
        for ($k = 1; $k -le 6; $k++) { $val = $val.Replace("{$k}", $argsT[$k - 1]) }
        Check "T $lang $key" (T $key @argsT) $val
    }
}
LoadLanguage 'en'
Check 'T english' (T 'device: {1}' 'F50 / MU300') 'device: F50 / MU300'
Check 'T untranslated' (T 'nobody translated {1} {2}' 'x' 'y') 'nobody translated x y'
Check 'T no args' (T 'plain {1}') 'plain {1}'
# the dictionary is case-sensitive: two messages may differ only in case
LoadLanguage 'tr'
Check 'T case' (T 'DEVICE: {1}' 'x') 'DEVICE: x'

# ---- NormalizeAnswer --------------------------------------------------------------------------------------------
$cases = @{
    'evet' = 'yes'; 'e' = 'yes'; 'y' = 'yes'; "$([char]0x662F)" = 'yes'
    ('hay' + [char]0x131 + 'r') = 'no'; 'hayir' = 'no'; 'n' = 'no'; "$([char]0x5426)" = 'no'
    ('g' + [char]0xFC + 'ncelle') = 'update'; 'guncelle' = 'update'; "$([char]0x66F4)$([char]0x65B0)" = 'update'
    'sil' = 'wipe'; 'INSTALL' = 'INSTALL'; 'overwrite' = 'overwrite'; '6.18' = '6.18'
}
foreach ($k in $cases.Keys) { Check "NormalizeAnswer $k" (NormalizeAnswer $k) $cases[$k] }

# ---- Gib (the sizes the free-space check prints) ----------------------------------------------------------------
if (Get-Command Gib -ErrorAction SilentlyContinue) {
    Check 'Gib' ((Gib 34828075008) -replace ',', '.') '32.4 GiB'
}

# ---- ChooseStorage: internal region or SD card -------------------------------------------------------------------
Check 'no card'            (ChooseStorage '' 0 30GB '' '') 'internal'
Check 'card, default'      (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' '') 'internal'
Check 'card, answer sd'    (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' 'sd') 'sd'
Check 'small internal'     (ChooseStorage '/dev/block/mmcblk1p1' 62GB 100MB '' '') 'sd'
Check 'forced internal'    (ChooseStorage '/dev/block/mmcblk1p1' 62GB 100MB 'internal' '') 'internal'
Check 'forced sd'          (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB 'sd' '') 'sd'
$threw = $false; try { ChooseStorage '' 0 30GB 'sd' '' | Out-Null } catch { $threw = $true }
Check 'forced sd, no card' $threw $true
$threw = $false; try { ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' 'usb' | Out-Null } catch { $threw = $true }
Check 'invalid answer'     $threw $true
$threw = $false; try { ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB 'usb' '' | Out-Null } catch { $threw = $true }
Check 'invalid forced'     $threw $true

# ---- ChooseOpenWrt: plain OpenWrt or with the MU300 control panel (install.sh's choose_systems) --------------------
Check 'owrt 3 + 2'         ((ChooseOpenWrt @('ubuntu', 'openwrt') '' '2') -join ' ') 'ubuntu openwrt-luci'
Check 'owrt 2 + 1'         ((ChooseOpenWrt @('openwrt') '' '1') -join ' ') 'openwrt'
Check 'owrt default'       ((ChooseOpenWrt @('openwrt') '' '') -join ' ') 'openwrt'
Check 'owrt preset luci'   ((ChooseOpenWrt @('openwrt') 'luci' '') -join ' ') 'openwrt-luci'
Check 'owrt preset plain'  ((ChooseOpenWrt @('ubuntu', 'openwrt') 'plain' '2') -join ' ') 'ubuntu openwrt'
Check 'owrt one element'   (@(ChooseOpenWrt @('openwrt') 'luci' '').Count) 1
$threw = $false; try { ChooseOpenWrt @('openwrt') 'x' '' | Out-Null } catch { $threw = $true }
Check 'owrt bad preset'    $threw $true
$threw = $false; try { ChooseOpenWrt @('openwrt') '' '7' | Out-Null } catch { $threw = $true }
Check 'owrt bad answer'    $threw $true
# a one-system result unrolls to a plain string, so the call site must wrap it in @() or $OSES[0] is one letter
$bare = ChooseOpenWrt @('openwrt') 'luci' ''
Check 'owrt bare result unrolls'  ($bare -is [string]) $true
$site = [IO.File]::ReadAllText((Join-Path $Top 'install.ps1'))
Check 'owrt call site wraps in @()'  ($site -match '\$OSES = @\(ChooseOpenWrt ') $true
foreach ($pair in @(@('', 'openwrt'), @('luci', 'openwrt-luci'))) {
    $OSES = @(ChooseOpenWrt @('openwrt') $pair[0] '')
    Check "owrt single [$($pair[1])] first"  $OSES[0] $pair[1]
    Check "owrt single [$($pair[1])] count"  $OSES.Count 1
}
# the boot question accepts the names that were chosen: 'openwrt' is not one of them after the second answer
$o = ChooseOpenWrt @('ubuntu', 'openwrt') '' '2'
Check 'boot openwrt-luci ok'  ('openwrt-luci' -in $o) $true
Check 'boot openwrt refused'  ('openwrt' -in $o) $false
# a card that holds an installation already is the default: init starts it before anything internal
Check 'installed card, default'    (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' '' 'yes') 'sd'
Check 'blank card, default'        (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' '' 'no') 'internal'
Check 'foreign card, default'      (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' '' 'foreign') 'internal'
Check 'installed card, internal'   (ChooseStorage '/dev/block/mmcblk1p1' 62GB 30GB '' 'internal' 'yes') 'internal'
Check 'StorageDefault installed'   (StorageDefault 30GB 'yes') 'sd'
Check 'StorageDefault small'       (StorageDefault 100MB 'no') 'sd'
Check 'StorageDefault room'        (StorageDefault 30GB 'no') 'internal'
Check 'internal over card'         (InternalOverCard 'internal' 'yes') $true
Check 'internal, blank card'       (InternalOverCard 'internal' 'no') $false
Check 'internal, foreign card'     (InternalOverCard 'internal' 'foreign') $false
Check 'sd over card'               (InternalOverCard 'sd' 'yes') $false
$src = [IO.File]::ReadAllText((Join-Path $Top 'install.ps1'))
$iO = $src.IndexOf('if (InternalOverCard $where $sdEx) {')
$iA = $src.IndexOf("if ((Ask (T 'Type internal to install to internal storage anyway') 'no') -ne 'internal') { Die (T 'cancelled') }")
Check 'internal over card asks'    ($iO -ge 0 -and $iA -gt $iO -and $iA - $iO -lt 600) $true

# ---- SdState: what is on the card already (ext4 magic at 1080, label at 1144) -------------------------------------
Check 'sd mu300sd'         (SdState '53ef' 'mu300sd') 'yes'
Check 'sd other label'     (SdState '53ef' 'data') 'foreign'
Check 'sd no label'        (SdState '53ef' '') 'foreign'
Check 'sd root label'      (SdState '53ef' 'mu300root') 'foreign'
Check 'sd not ext4'        (SdState '0000' 'mu300sd') 'no'
Check 'sd unreadable'      (SdState '' '') 'no'

# ---- SdKernelOk: a card installation needs a bundle that lists sdcard in ./features ------------------------------
$kd = Join-Path ([IO.Path]::GetTempPath()) ('mu300-k-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $kd | Out-Null
Check 'sd kernel, no features'      (SdKernelOk 1 $kd) $false
Check 'internal, no features'       (SdKernelOk 0 $kd) $true
[IO.File]::WriteAllText((Join-Path $kd 'features'), "other`n")
Check 'sd kernel, other features'   (SdKernelOk 1 $kd) $false
[IO.File]::WriteAllText((Join-Path $kd 'features'), "sdcard`n")
Check 'sd kernel, sdcard'           (SdKernelOk 1 $kd) $true
Remove-Item -Recurse -Force $kd
$src = [IO.File]::ReadAllText((Join-Path $Top 'install.ps1'))
$iUnpack = $src.IndexOf('& tar -xzf "$REL\mu300-kernel-$KERNEL.tar.gz" -C $KMAIN')
$iCheck = $src.IndexOf('if (-not (SdKernelOk $SD_MODE $KMAIN))')
Check 'sd kernel check after unpack' ($iUnpack -ge 0 -and $iCheck -gt $iUnpack) $true

# ---- InstallEnvText: mu300-install.env, the same text tools/storage.sh's write_install_env writes ------------------
$common = @{ OFF = [int64]27762098176; FORMAT = 1; OSES = @('ubuntu', 'openwrt'); WIPE_LEGACY = 0; UPDATE = 0
    BOOT_OS = 'openwrt'; DEFAULT_LINUX = 1; BOOT_ATTEMPTS = 5; IMPORT_HOTSPOT = 1; KERNEL = '6.18'; PWHASH = '$6$salt$hash/x.y' }
$sdEnv = $common.Clone(); $sdEnv.SIZE = [int64]31914967040; $sdEnv.INT_SIZE = [int64]34776023040; $sdEnv.SD_MODE = 1
$sdEnv.SD_DEV = '/dev/block/mmcblk1p1'; $sdEnv.INTERNAL_EXISTS = 1
$inEnv = $common.Clone(); $inEnv.SIZE = [int64]34776023040; $inEnv.INT_SIZE = [int64]0; $inEnv.SD_MODE = 0
$inEnv.SD_DEV = ''; $inEnv.INTERNAL_EXISTS = 0; $inEnv.FORMAT = 0; $inEnv.UPDATE = 1
$t = InstallEnvText $sdEnv
Check 'env sd: region'      ($t -match "(?m)^OFF=27762098176\nSIZE=34776023040\nOFF_S=54222848\nSIZE_S=67921920\n") $true
Check 'env sd: card'        ($t -match "(?m)^SD_MODE=1\nSD_DEV=/dev/block/mmcblk1p1\nINTERNAL_EXISTS=1\n") $true
Check 'env sd: format'      ($t -match "(?m)^FORMAT=1\n" -and $t -match "(?m)^UPDATE=0\n") $true
Check 'env sd: oses'        ($t -match '(?m)^OSES="ubuntu openwrt"\n') $true
Check 'env sd: hash'        ($t.EndsWith("PWHASH='`$6`$salt`$hash/x.y'`n")) $true
Check 'env sd: no CR'       ($t.Contains("`r")) $false
$t = InstallEnvText $inEnv
Check 'env internal'        ($t -match "(?m)^OFF=27762098176\nSIZE=34776023040\nOFF_S=54222848\nSIZE_S=67921920\nFORMAT=0\n") $true
Check 'env internal: sd'    ($t -match "(?m)^UPDATE=1\n" -and $t -match "(?m)^SD_MODE=0\nSD_DEV=\nINTERNAL_EXISTS=0\n") $true
if (Get-Command sh -CommandType Application -ErrorAction SilentlyContinue) {
    foreach ($e in $sdEnv, $inEnv) {
        $vars = (($e.Keys | Sort-Object | ForEach-Object { "$_='$(@($e[$_]) -join ' ')'" }) -join '; ')
        if ($e.SD_MODE -eq 0) { $vars += "; INT_SIZE=''" }
        $shText = ((& sh -c ($vars + '; . ./tools/storage.sh; write_install_env')) -join "`n") + "`n"
        Check "env = storage.sh (SD_MODE=$($e.SD_MODE))" (InstallEnvText $e) $shText
    }
}

# ---- uninstall.ps1: what is on the card, and the command that erases it -----------------------------------------
$uast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $Top "uninstall.ps1"), [ref]$null, [ref]$null)
$udefs = $uast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and @('SdState', 'SdEraseCommand') -contains $n.Name }, $true)
foreach ($d in $udefs) { . ([scriptblock]::Create($d.Extent.Text)) }
Check 'uninstall sd mu300sd'    (SdState '53ef' 'mu300sd') 'yes'
Check 'uninstall sd other'      (SdState '53ef' 'mu300root') 'foreign'
Check 'uninstall sd not ext4'   (SdState '0000' 'mu300sd') 'no'
foreach ($dev in '/dev/block/mmcblk0p1', '/dev/block/mmcblk0', '/dev/block/sda1', '/dev/block/mmcblk1p1 x', '') {
    $threw = $false; try { SdEraseCommand $dev | Out-Null } catch { $threw = $true }
    Check "erase refuses [$dev]" $threw $true
}
$cmd = SdEraseCommand '/dev/block/mmcblk1p1'
Check 'erase command quotes'    ($cmd -match "[`"']") $false
Check 'erase command target'    ($cmd -match ' of=/dev/block/mmcblk1p1 bs=1048576 count=64 ') $true
# the same text tools/storage.sh's sd_erase_cmd builds (and tests/test_installer.py runs against stubs)
if (Get-Command sh -CommandType Application -ErrorAction SilentlyContinue) {
    foreach ($dev in '/dev/block/mmcblk1p1', '/dev/block/mmcblk1') {
        $shCmd = (& sh -c ('unset MU300_SYSFS MU300_MOUNTS; . ./tools/storage.sh; sd_erase_cmd ' + $dev)) -join "`n"
        Check "erase command = storage.sh ($dev)" (SdEraseCommand $dev) $shCmd
    }
}

Write-Host "$script:passed passed, $script:failed failed"
exit $script:failed
