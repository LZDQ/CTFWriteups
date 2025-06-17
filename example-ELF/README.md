# Compilation process

[![](./gcc_flow.png)](https://medium.com/@joel.dumortier/the-steps-of-compilation-with-gcc-60661f66890e)

# [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) file structure

`man elf` has very detailed information. Refer to it when not sure about something.
Source file at `/usr/include/elf.h`

## ELF header (Ehdr)

52 or 64 bytes for 32-bit and 64-bit, always at beginning (offset 0 into the file).

```c
#define EI_NIDENT 16
typedef struct elf64_hdr {
  unsigned char	e_ident[EI_NIDENT];	/* ELF "magic number" */
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;		/* Entry point virtual address */
  Elf64_Off e_phoff;		/* Program header table file offset */
  Elf64_Off e_shoff;		/* Section header table file offset */
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;
```

## Program header = segments (Phdr)

Like `vmmap` in pwndbg. Not for object files since they are intermediate files and not meant to be loaded into execution memory. Use `readelf -l` to view them.
"It is found at file offset `e_phoff`, and consists of `e_phnum` entries, each with size `e_phentsize`." - wikipedia

```c
typedef struct {
    uint32_t   p_type;
    uint32_t   p_flags;
    Elf64_Off  p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    uint64_t   p_filesz;
    uint64_t   p_memsz;
    uint64_t   p_align;
} Elf64_Phdr;
```

Has exactly one INTERP storing the interpreter's path. Dynamic executable has one DYNAMIC for the `.dynamic` section.

Several LOADs map the file to virtual memory. The actual runtime address = base_addr + virt_addr. For PIE virt_addr is 0, while no-PIE has the exact virtual address.

## Section header (Shdr)

Define various things about the ELF. (Note that `sh_name` is offset into `.shstrtab` not index).
Use `readelf -S` to view them. The size is at `readelf -h`.

```c
typedef struct {
    uint32_t   sh_name;
    uint32_t   sh_type;
    uint64_t   sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off  sh_offset;
    uint64_t   sh_size;
    uint32_t   sh_link;
    uint32_t   sh_info;
    uint64_t   sh_addralign;
    uint64_t   sh_entsize;
} Elf64_Shdr;
```

`sh_addr` is the expected virtual address but typically not used at runtime.

## Symbol table

```c
typedef struct {
    uint32_t      st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t      st_shndx;
    Elf64_Addr    st_value;
    uint64_t      st_size;
} Elf64_Sym;
```

`st_name` is the offset into `.strtab`. For `SECTION` type the name is from the section name.
`st_value` is the value of the symbol. For `FUNC` and `OBJECT` they are the offset into the section, and the section is in `st_shndx`.
`st_shndx` displays as `Ndx` in `readelf -s`, is the associated section (NOT symbol) index of this symbol. For `SECTION` type symbol it is the index of itself in section headers.
`st_size` is the size of an object (like 4 for int) or func (the machine code length).

## Relocation entries

```c
typedef struct {
    Elf64_Addr r_offset;
    uint64_t   r_info;
} Elf64_Rel;

typedef struct {
    Elf64_Addr r_offset;
    uint64_t   r_info;
    int64_t    r_addend;
} Elf64_Rela;
```

Relocations are for object files when calling another function. The assembler doesn't know the address so leaves a relocation entry.

For example, `.text` section has relocation section `.rela.text` storing an array of `Elf64_Rela`.

* `r_offset` means the offset into the corresponding section (`.text` in this case) or offset into base_addr for runtime relocations.
* `r_info` defines type and info. Lower 4 bytes is type, higher 4 bytes is the index of **symbol**.
* `r_addend` add to the address of that symbol.

For `call` in x86,  first byte is `e8` and the rest 4 bytes is the offset of address between target PC and next PC (current+5). Thus, `r_offset` is the offset of the last 4 bytes of the `call` into `.text`, `r_info` points to the target function's index in symbol table, and `r_addend` is `-4` to tradeoff the length of the pointer.

```
  1e:	e8 00 00 00 00       	call   23
  
      Offset             Info             Type               Symbol's Value  Symbol's Name + Addend
000000000000001f  0000000300000004 R_X86_64_PLT32         0000000000000000 zoo - 4
```

For this type, the linker will minus the target address by the current address and then add the addend.

-----

# Runtime

The interpreter, `ld` means loader/dynamic linker, which dynamically loads shared libraries and resolves symbols and manage GOT PLT.

## Dynamic[ relocation]

The section `.dynamic` contains information about dynamic linking. Refer to "Dynamic tags" in `man elf`.

```c
typedef struct {
    Elf64_Sxword    d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr  d_ptr;
    } d_un;
} Elf64_Dyn;
```

The section `.rela.dyn` holds relocations during `.init` (anything excluding plt). Note that internal variables are referenced by the relative offset between the variable and the program counter, since the relative offset between some sections doesn't change.

The section `.rela.plt` holds relocations for plt, and the runtime resolver uses its indices.

## GOT & PLT

In short: GOT is a list of addresses; PLT is a list of stubs of functions (stubs points to GOT).

|         | GOT                                           | PLT                                                          |
| ------- | --------------------------------------------- | ------------------------------------------------------------ |
| Name    | Global Offset Table                           | Procedure Linkage Table                                      |
| Section | `.got`, and `.got.plt` specifically for plt   | `.plt` and `.rela.plt`                                       |
| Address | Typicall `.dynamic .got .got.plt .data .bss`  | Typically `.init .plt .text .fini`, starts at base_addr + 0x1020 |
| Data    | Addresses to (potentially unresolved) objects | Machine code, each entry typically has 0x10 bytes (details below) |

## Dynamic resolution:

1. `starti`; Some memory is mapped, including the executable file and interpreter, excluding dynamic libraries like libc. Calls to external functions points to plt stubs.

2. `b main; c`; Before main, `.init` loads required shared libraries. `.got.plt` points to the second instruction in the `.plt` entry (the next instruction). Also the first three entries are the `.dynamic` offset into file, address of `link_map` and address of `_dl_runtime_resolve`, respectively. The address of `.got.plt` is from `PLTGOT` entry in `.dynamic`.

3. Except for the first one, each entry in `.plt` has three instructions. First, load the pointer at `.got.plt` entry and jump there, and if that is already resolved, it will never come back;

   Example of entries, in pwndbg:

   ```
      0x555555555040 <printf@plt>:	jmp    QWORD PTR [rip+0x2fc2]        # 0x555555558008 <printf@got.plt>
      0x555555555046 <printf@plt+6>:	push   0x1
      0x55555555504b <printf@plt+11>:	jmp    0x555555555020
      ...
      0x555555555060 <sqrt@plt>:	jmp    QWORD PTR [rip+0x2fb2]        # 0x555555558018 <sqrt@got.plt>
      0x555555555066 <sqrt@plt+6>:	push   0x3
      0x55555555506b <sqrt@plt+11>:	jmp    0x555555555020
   ```

4. Otherwise (`.got.plt` entry not resolved), push the index of this unresolved entry (index is into `.rela.plt`), jump to the first entry in `.plt` and let it resolve.

   Example of the first entry (resolver), in objdump:

   ```
       1020:	ff 35 ca 2f 00 00    	push   0x2fca(%rip)        # 3ff0 <_GLOBAL_OFFSET_TABLE_+0x8>
       1026:	ff 25 cc 2f 00 00    	jmp    *0x2fcc(%rip)        # 3ff8 <_GLOBAL_OFFSET_TABLE_+0x10>
       102c:	0f 1f 40 00          	nopl   0x0(%rax)
   ```

   It pushes the address of `link_map` then jumps to the resolver in the interpreter. These two addresses are filled by `.init`.

One interesting thing is that `.got` and `.got.plt` are adjacent but `.got.plt` starts exactly at a new page. This is because of RELRO.

## RELRO

* Partial RELRO: `.got` is read-only after `.init`, but `.got.plt` is not. In this case it starts at a new page.
* Full RELRO: `.got.plt` is merged into `.got` and all resolved in `.init`, and they become no-write later.
