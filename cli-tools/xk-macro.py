#!/usr/bin/env python3
"""xk-macro -- manage xrnkey's abbreviations (Bamboo calls them macros).

They live in $XDG_DATA_HOME/xrnkey/macros.txt (default
~/.local/share/xrnkey/macros.txt), one `abbreviation<TAB>expansion` per line.
The Fcitx5 addon re-reads the file when it changes and expands an abbreviation
when the word ends, once the Macro option is on (Fcitx5 settings, or
Macro=True in ~/.config/fcitx5/conf/xrnkey.conf).
"""

import argparse
import os
import sys
import tempfile


def data_dir():
    """The folder of the user's files, where the Fcitx5 addon reads them
    (user_paths.cpp, userDataDir). Until 1.1.0 it was called xrnikey; when
    only that one exists it is moved to the new name."""
    data_home = os.environ.get("XDG_DATA_HOME", "")
    if not data_home.startswith("/"):
        data_home = os.path.expanduser("~/.local/share")
    new, old = os.path.join(data_home, "xrnkey"), os.path.join(data_home, "xrnikey")
    if not os.path.lexists(new) and os.path.lexists(old):
        try:
            os.rename(old, new)
        except OSError:
            if not os.path.lexists(new):
                return old
    return new


def macro_path():
    return os.path.join(data_dir(), "macros.txt")


def read_lines(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read().splitlines()
    except FileNotFoundError:
        return []


def entries(lines):
    """(abbreviation, expansion) for each valid line, in file order."""
    for line in lines:
        if not line or line.startswith("#") or "\t" not in line:
            continue
        abbr, expansion = line.split("\t", 1)
        if abbr and expansion:
            yield abbr, expansion


def write_lines(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), prefix=".macros-")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write("".join(line + "\n" for line in lines))
    os.replace(tmp, path)


def same(a, b):
    return a.lower() == b.lower()


def cmd_add(path, abbr, expansion):
    if not abbr or any(c.isspace() for c in abbr):
        sys.exit("xk-macro: an abbreviation is one word, without spaces or tabs")
    if not expansion or "\t" in expansion or "\n" in expansion:
        sys.exit("xk-macro: the expansion must not be empty or contain tabs or newlines")
    lines = [line for line in read_lines(path)
             if not any(same(a, abbr) for a, _ in entries([line]))]
    lines.append(f"{abbr}\t{expansion}")
    write_lines(path, lines)
    print(f"{abbr} -> {expansion}")


def cmd_remove(path, abbr):
    lines = read_lines(path)
    kept = [line for line in lines if not any(same(a, abbr) for a, _ in entries([line]))]
    if len(kept) == len(lines):
        sys.exit(f"xk-macro: no abbreviation {abbr!r}")
    write_lines(path, kept)
    print(f"removed {abbr}")


def cmd_list(path):
    for abbr, expansion in entries(read_lines(path)):
        print(f"{abbr}\t{expansion}")


def main(argv=None):
    parser = argparse.ArgumentParser(prog="xk-macro", description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p_add = sub.add_parser("add", help="add or replace an abbreviation")
    p_add.add_argument("abbreviation")
    p_add.add_argument("expansion", nargs="+", help="the text it expands to")
    p_remove = sub.add_parser("remove", help="remove an abbreviation")
    p_remove.add_argument("abbreviation")
    sub.add_parser("list", help="list the abbreviations")
    sub.add_parser("path", help="print the file's path")
    args = parser.parse_args(argv)
    path = macro_path()
    if args.command == "add":
        cmd_add(path, args.abbreviation, " ".join(args.expansion))
    elif args.command == "remove":
        cmd_remove(path, args.abbreviation)
    elif args.command == "list":
        cmd_list(path)
    else:
        print(path)


if __name__ == "__main__":
    main()
