# Troubleshooting

## ADB Issues

### `adb devices` shows nothing

1. Re-enable USB debugging in the F50 web UI:
   `http://192.168.0.1/index.html#usb_port`
2. Restart the ADB server on the PC:
   ```
   adb kill-server
   adb start-server
   adb devices
   ```
3. Try a different USB port and cable.

### `error: closed` during `adb push` or `magisk --install-module`

This is a USB connection drop. Fix:

1. Switch to network ADB:
   ```
   adb tcpip 5555
   adb connect <DEVICE_IP>:5555
   ```
2. Or restart ADB and retry the command.

### `more than one device/emulator`

You have both USB and TCP connected. Target the TCP device explicitly:
