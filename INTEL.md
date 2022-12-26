# Plan

A tool to embed files into ELF objects (`.o`).

The idea is to _extend_ `objcopy` to allow more than 1 file and with custom
(simpler) user-provided names.

### Files

For each file, it shall have 2 symbols: the beggining and the size.

> **NOTE**: the size/length of the text is a weird one to handle, because it's
> a symbol, so a reference?
