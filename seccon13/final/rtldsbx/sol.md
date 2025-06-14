# rtldsbx

Solves: 2

[official archive](https://github.com/SECCON/SECCON13_final_CTF/tree/main/jail/rtldsbx) (with official solution in it)

## Problem

Given: a patched interpreter ([ld-linux-x86-64-sbx.so.2](https://github.com/SECCON/SECCON13_final_CTF/blob/main/jail/rtldsbx/build/ld-linux-x86-64-sbx.so.2)) and a patched `libc.so.6`. The interpreter (or is it the libc?) bans `execve` and `execveat` syscalls by seccomp.

The server receives a binary file. It will first run `ldd <binary_file>` to get the dynamic libraries used and check whether it is exactly the list

```python
WHITELIST = {'linux-vdso.so.1', 'libc.so.6', 'lib/ld-linux-x86-64-sbx.so.2'}
```

If it is, run the binary file without any constraints.

The goal is to run `/readflag` which has `111` permission.

## Solution

During the contest, I tried several methods to escape seccomp. [ref1](https://blog.ssrf.in/post/bypass-seccomp-with-ptrace/) [ref2](https://ptr-yudai.hatenablog.com/?page=1577875543#pwn-993pts-adult-seccomp)

However, these methods only work before a certain glibc version.

-----

One difference between the blog post and this challenge is that the blog uses seccomp in docker but this challenge only checks `ldd <binary_file>`. Note that `ldd` outputs the soname and the resolved so file, but only the first is captured by the regex group. After the check, the program will run without any seccomp.

The idea is to trick `ldd` into believing we are using the provided libs but actually resolves to something else.  If we can use a statically linked elf and pretend like it is dynamic, the solution would be simple. However, it requires a lot of work to change a static binary into a dynamic one. Instead, we will start with the provided `lib/ld-linux-x86-64-sbx.so.2`, call it `exp`. The `ld-linux-x86-64.so.2` is also an executable. Running `ldd exp` prints that it is static. With `patchelf --add-needed 'libc.so.6' exp`, we can get `ldd` to output

```
	linux-vdso.so.1 (0x00007f21bb086000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f21bae00000)
```

If we run `exp` now it will throw a segmentation fault, but that's not a big deal. At least it gets executed. Then we add another whitelist: `patchelf --add-needed 'lib/ld-linux-x86-64-sbx.so.2' exp` and `ldd exp` outputs

```
	linux-vdso.so.1 (0x00007f472381c000)
	lib/ld-linux-x86-64-sbx.so.2 (0x00007f47237a0000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f4723400000)
```

which is exactly the whitelist but doesn't resolves to `./lib`. Then, modify the 8-th byte in the ELF header (also 8 byte into the file) from `03` to `00` to match the magic number, and patch the entrypoint with execve assembly. This will be the ELF submitted to the server.

Note that this only works in glibc 2.39 not in glibc 2.41. The official solution works in both versions.

## Takeaways

Think out of the box of using `ldd` to restrict dynamic library. The provided sample ELF is intentionally compiled with `rpath` set to `./lib`, but `rpath` is not present in the solution.

Also almost always use the same version as in the challenge.

## Credits

Thanks P1G SEKAI for discussing the solution.

## Official sol

The official solution adds another interpreter in the program header. As `man elf` says, there should be only one interpreter in the program headers. If we add another one after all the important ones for runtime, `ldd` will take into accounts of the last one interpreter, but when it gets executed, only the first one is used.
