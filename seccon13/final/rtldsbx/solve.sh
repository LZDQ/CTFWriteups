cp lib/ld-linux-x86-64-sbx.so.2 exp
patchelf --add-needed 'libc.so.6' exp
patchelf --add-needed 'lib/ld-linux-x86-64-sbx.so.2' exp
python patch.py
