"""check_array_literals.py — 找出一维数组初始化列表长度与声明长度不符的地方（MSVC C2078 的根因）

用法: python check_array_literals.py <src_root>
"""
import os, re, sys


def count_values(text):
    depth = 0
    n = 0
    for ch in text:
        if ch in '({':
            depth += 1
            if depth == 1:
                n += 1  # 顶层第一个元素
        elif ch in ')}':
            depth -= 1
        elif ch == ',' and depth == 1:
            n += 1
    return n


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else '.'
    pat = re.compile(r'(\w+)\s*\[\s*(\d+)\s*\]\s*=\s*\{', re.S)
    hits = 0
    for dirpath, _, files in os.walk(root):
        for fn in files:
            if not fn.endswith(('.c', '.h', '.inc', '.cc')):
                continue
            p = os.path.join(dirpath, fn)
            with open(p, encoding='utf-8', errors='replace') as f:
                src = f.read()
            for m in pat.finditer(src):
                name, decl = m.group(1), int(m.group(2))
                start = m.end() - 1
                depth = 0
                i = start
                while i < len(src):
                    if src[i] == '{':
                        depth += 1
                    elif src[i] == '}':
                        depth -= 1
                        if depth == 0:
                            break
                    i += 1
                body = src[start:i + 1]
                n = count_values(body)
                if n != decl:
                    line = src[:m.start()].count('\n') + 1
                    rel = os.path.relpath(p, root)
                    print("%-28s:%-5d %-14s decl[%d]  init=%d  %s"
                          % (rel, line, name, decl, n, "TOO MANY" if n > decl else "too few"))
                    hits += 1
    print("---- mismatches: %d ----" % hits)


if __name__ == '__main__':
    main()
