#!/usr/bin/env python3
"""vk-dict — interactive dictionary CLI for VietKey."""

import argparse
import ctypes
import os
import sys

def load_lib():
    for path in ["/usr/lib/libvietkey.so", "/usr/local/lib/libvietkey.so"]:
        if os.path.exists(path):
            return ctypes.CDLL(path)
    print("Error: libvietkey.so not found", file=sys.stderr)
    sys.exit(1)

def cmd_lookup(lib, word, dict_path):
    lib.vk_dict_load.argtypes = [ctypes.c_char_p]
    lib.vk_dict_load.restype = ctypes.c_void_p
    lib.vk_dict_lookup.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.vk_dict_lookup.restype = ctypes.c_int
    lib.vk_dict_free.argtypes = [ctypes.c_void_p]

    d = lib.vk_dict_load(dict_path.encode())
    if not d:
        print(f"Error: cannot load dictionary from {dict_path}", file=sys.stderr)
        sys.exit(1)

    freq = lib.vk_dict_lookup(d, word.encode("utf-8"))
    if freq:
        print(f"{word}\tfrequency={freq}")
    else:
        print(f"{word}\tnot found")
    lib.vk_dict_free(d)

def cmd_add(word, freq, user_dict):
    os.makedirs(os.path.dirname(user_dict), exist_ok=True)
    with open(user_dict, "a", encoding="utf-8") as f:
        f.write(f"{word}\t{freq}\n")
    print(f"Added: {word} (freq={freq})")

def cmd_remove(word, user_dict):
    if not os.path.exists(user_dict):
        print("User dictionary is empty")
        return
    lines = open(user_dict, encoding="utf-8").readlines()
    with open(user_dict, "w", encoding="utf-8") as f:
        removed = False
        for line in lines:
            if line.startswith(word + "\t"):
                removed = True
                continue
            f.write(line)
    print(f"Removed: {word}" if removed else f"Not found: {word}")

def cmd_import(src, user_dict):
    os.makedirs(os.path.dirname(user_dict), exist_ok=True)
    count = 0
    with open(src, encoding="utf-8") as inf, open(user_dict, "a", encoding="utf-8") as outf:
        for line in inf:
            line = line.strip()
            if line and not line.startswith("#"):
                outf.write(line + "\n")
                count += 1
    print(f"Imported {count} entries")

def cmd_export(dest, user_dict):
    if not os.path.exists(user_dict):
        print("User dictionary is empty")
        return
    import shutil
    shutil.copy(user_dict, dest)
    print(f"Exported to {dest}")

def cmd_stats(user_dict):
    if not os.path.exists(user_dict):
        print("User dictionary: 0 entries")
        return
    lines = [l for l in open(user_dict, encoding="utf-8") if l.strip() and not l.startswith("#")]
    print(f"User dictionary: {len(lines)} entries")

def main():
    parser = argparse.ArgumentParser(description="VietKey dictionary CLI")
    sub = parser.add_subparsers(dest="command")

    p_lookup = sub.add_parser("lookup")
    p_lookup.add_argument("word")
    p_lookup.add_argument("--dict", default="/usr/share/vietkey/dict/vi.dict")

    p_add = sub.add_parser("add")
    p_add.add_argument("word")
    p_add.add_argument("freq", type=int)

    p_remove = sub.add_parser("remove")
    p_remove.add_argument("word")

    p_import = sub.add_parser("import")
    p_import.add_argument("file")

    p_export = sub.add_parser("export")
    p_export.add_argument("file")

    sub.add_parser("stats")

    args = parser.parse_args()
    user_dict = os.path.expanduser("~/.local/share/vietkey/user-dict.txt")

    if args.command == "lookup":
        lib = load_lib()
        cmd_lookup(lib, args.word, args.dict)
    elif args.command == "add":
        cmd_add(args.word, args.freq, user_dict)
    elif args.command == "remove":
        cmd_remove(args.word, user_dict)
    elif args.command == "import":
        cmd_import(args.file, user_dict)
    elif args.command == "export":
        cmd_export(args.file, user_dict)
    elif args.command == "stats":
        cmd_stats(user_dict)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
