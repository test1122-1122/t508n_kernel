# uwe5622-5ghz-fix

Patched `sprdwl_ng` kernel module for the Unisoc UWE5622 Wi-Fi chip on the
Orange Pi Zero 2W, Zero 3 and 3 LTS, running Armbian or DietPi.

If this looks familiar, you're in the right place:

```
$ iw reg get
global
country FR: DFS-ETSI
...
phy#0
country 00: DFS-UNSET
```

The phy stays on the world domain no matter what you set, and on a 5 GHz DFS
channel (52 and up) the board associates, shows a perfect link, and then passes
almost nothing. The 4-way handshake times out, or traffic crawls with most
packets lost, and the board falls back to 2.4 GHz.

This is a patched kernel module, not a program. Install it once, reboot, forget
it.

## What's wrong with the driver

Two bugs, and they stack.

The driver ignores the kernel's regulatory database. It marks its wiphy
`REGULATORY_CUSTOM_REG`, installs its own world table at setup, and answers
country hints from a private table of seven entries: `00 US CN NL CZ JP TR`.
Set `FR`, or let your router's country IE say `DE`, and the lookup fails, falls
back to the world table, and never tells cfg80211. Adding your country to that
table would fix one country. The patch removes the table from the decision and
lets cfg80211 own the domain, the way every in-tree driver does.

The rule table it sends to the firmware is corrupt. Since Linux 4.18 the driver
`memcpy()`s the kernel's `struct ieee80211_reg_rule` (96 bytes on 6.18) into a
buffer the firmware reads as 28-byte entries. The first rule's frequencies
survive. From the second entry on, the firmware is reading WMM parameters where
it expects flags. The patch copies the four fields the firmware knows, one by
one.

There is a third problem that has nothing to do with regulatory domains. On the
first association after a cold boot the driver lets the firmware power down
right after CONNECT. The AP's EAPOL frames go unanswered and it deauthenticates
about eleven seconds later. Every boot lost 12 to 25 s to this. The fix is a
module parameter, `disable_powersave=1`, no rebuild needed.

With all three: handshake in 30 to 80 ms, DHCP lease about 16 s after power-on,
and a ten-minute soak on channel 52 at VHT80 with zero packet loss.

## Install the prebuilt module

The prebuilt module matches exactly one kernel:

```
6.18.45-current-sunxi64   (DietPi linux-image-current-sunxi64 26.08.0-trunk-dietpi3)
sha256  e38bb92616fca952b1d11c6fa08d722ee07b51773135e2228257c93af0480d4f  sprdwl_ng-patched.ko.xz
```

Check `uname -r` first. If it doesn't match, build your own (next section).

The module itself is not in the git tree; download it from the
[Releases](../../releases) page, then:

```sh
sha256sum sprdwl_ng-patched.ko.xz
scp sprdwl_ng-patched.ko.xz wifi-install.sh uwe5622-disable-powersave.conf root@<board>:/root/
ssh root@<board> 'cp /root/uwe5622-disable-powersave.conf /etc/modprobe.d/ && /root/wifi-install.sh /root/sprdwl_ng-patched.ko.xz && reboot'
```

`wifi-install.sh` keeps the original as `sprdwl_ng.ko.xz.orig` next to the new
one and runs `depmod`. Reboot to load it. Don't `modprobe -r` the running module
over the Wi-Fi you're logged in through: the board goes dark and needs a power
cycle.

A kernel update overwrites the module, so run `wifi-install.sh` again after
one. The modprobe option survives updates on its own.

If you want a safety net while trying an untested build, `wifi-module-guard.sh`
and its `.service` restore the original and reboot when the gateway isn't
reachable three minutes after boot, and dump dmesg and the journal to `/root`
first. Set `GW=` in the unit if your gateway isn't `192.168.1.1`. Disable it
once you're happy; a router outage at boot would otherwise trigger it.

## Build it yourself

The build runs in a Debian container against the DietPi kernel headers and
needs Docker on an x86-64 machine. Nothing is compiled on the board.

```sh
git clone https://github.com/armbian/uwe5622 build-patched
git -C build-patched am ../0001-unisocwifi-regdomain-from-cfg80211.patch
docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w debian:trixie /w/build-in-docker.sh build-patched
```

The module lands in `build-patched/unisocwifi/sprdwl_ng.ko.xz`. For another
kernel, set `HDR_DEB`, `HDR_SHA` and `KVER`; the headers packages are listed at
https://dietpi.com/apt/dists/all/orangepizero2w/binary-arm64/Packages. Armbian
users can apply the patch through the build framework instead; the driver
source is fetched from `armbian/uwe5622` in `lib/functions/compilation/patch/drivers_network.sh`.

The prebuilt module in this repo was built from `armbian/uwe5622` commit
`cc2835a` plus the patch, with this script.

## Just the boot fix

If 5 GHz already works for you but the first connection after boot takes 20 to
30 seconds, you may only need the module parameter:

```sh
echo 'options sprdwl_ng disable_powersave=1' > /etc/modprobe.d/uwe5622-disable-powersave.conf
```

It requires a driver built from current `armbian/uwe5622` (the parameter isn't
in older DietPi builds; `modinfo sprdwl_ng | grep parm` tells you).

## Things that cost a reboot each

The kernel decompresses modules itself and only accepts an xz stream with a
CRC32 check. Plain `xz` produces CRC64 and the module fails to load with a bare
`Invalid argument` and nothing in dmesg. The build script uses
`--check=crc32`.

`/etc/network/interfaces` on DietPi sources `interfaces.d/*`. A backup file
left in that directory becomes a second `wlan0` stanza, ifup runs the whole
bring-up twice, and the supplicant gets killed right after DHCP.

Your router's country IE overrides any `country=` in wpa_supplicant at
association time, so there's no supplicant-side setting that works around the
driver bug.

`/var/log` is a tmpfs on DietPi. Anything you want to read after a reboot goes
in `/root`.

DietPi ships with Bluetooth off (`/etc/modprobe.d/dietpi-disable_bluetooth.conf`
blacklists `sprdbt_tty`), which looks exactly like "this board has no Bluetooth
controller". `dietpi-set_hardware bluetooth enable` fixes it. Bluetooth and
Wi-Fi share the chip; I measured no coexistence effect on 5 GHz.

## Upstream

The patch is a `git am`-able format-patch against
https://github.com/armbian/uwe5622. Once it's merged there, Armbian and DietPi
kernels will carry it and this repo is only useful for the boot fix.

## Licence

GPL-2.0, same as the driver it patches. The prebuilt module is a derived work
of `armbian/uwe5622`; its source is that repository at the commit above plus
the patch in this one.
