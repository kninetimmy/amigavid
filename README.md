# AmigaVid

AmigaVid is an early-setup project to create a native, offline, 32-bit x86
Win32 port of Almathera's Amiga CD32 Video Creator for Windows 98 and XP. No
Amiga emulator or ROM is required at runtime.

See [Video-Creator-Win32-PRD.md](Video-Creator-Win32-PRD.md) for detailed
requirements and acceptance evidence.

## Expected phases

0. **Establish the baseline:** verified archive and source inventory.
1. **Prove the Windows toolchain:** compatibility probe.
2. **Open a real asset:** real asset display.
3. **Edit, save and replay one sequence:** edit, save and replay a sequence.
4. **Port effects and special screens:** effects and special screens.
5. **Complete media, editor and persistence:** media, editor and persistence.
6. **Qualify and package:** qualification and portable packaging.

## Current focus

The project is in early setup. The next step is pinned archive verification
and a source inventory.

## First useful demo and release acceptance

The first useful demo opens real editor artwork on Windows 98, adds two
pictures and a timed transition, saves the project, and plays that file on XP
with the same executable.

Release acceptance is stricter: the same portable ZIP must pass the edit,
save, reopen and playback workflow on both Windows families. A modern-host
smoke test does not qualify either target.
