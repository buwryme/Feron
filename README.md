# feron
feron is a hobbyist project, which aims to provide a full-featured OS.
it is written in modern C++20.

## features:
- freestanding runtime for c++
- 64-bit long mode entry
- VGA text console
- `COM1` output for debugging
- taskfile for automations
- a kernel entry point (`kmain`)
- heap initialization from the memory provided by multiboot

for now, it can safely boot exclusively from GRUB.

## building:
feron's build process is pretty straightforward.

...when building, a unique build timestamp is made.
### requirements:
- `clang++`, `nasm`, `ld.ldd` for compiling;
- `grub-mkrescue`, `xorisso` for .iso building
- [`task`](https://taskfile.dev) for task automation

to build, run `task build`.

## running:
this process runs the latest build, judging by the timestamp.
to do that, run `task run`.

## cleaning:
cleaning deletes all artifacts.
to do that, run `task clean`.
