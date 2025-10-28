w# Mouse Emulator
If your mouse is a potato or you just don’t have one, this project lets you emulate a mouse using your keyboard!

## Requirements
* A keyboard (mechanical recommended)
* uinput kernel module (use `modprobe uinput`)

## Key Mappings
* W/A/S/D: Move the mouse around.
* Q: Left-click.
* E: Right-click.
* Right Ctrl: Activate mouse emulation (the potato key!).
* Shift: Slow things down by 5x.

## Building
```
make
make install
```

## Usage
First, list devices:
```bash
/usr/libexec/mouse-emu
```

Example output:
```
Searching:
/dev/input/event21 => 2.4G Composite Device
/dev/input/event3 => SEMICO USB Gaming Keyboard
```

Then, use your device:
```bash
/usr/libexec/mouse-emu /dev/input/event3
```

