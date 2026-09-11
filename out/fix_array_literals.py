"""fix_array_literals.py — 把初始化列表多于声明长度的数组截断到声明长度（MSVC C2078 的修法）

C 标准允许初始化列表多于数组长度（多余部分被静默丢弃），GCC 接受；MSVC 视为错误。
本脚本按"声明长度"截断多余项，语义与 GCC 下完全一致（多余项本来就不参与程序行为）。

用法:
  python fix_array_literals.py --check  <src_root>     # 只报告
  python fix_array_literals.py --apply  <src_root>     # 就地修复（建议先备份）
"""
import os, re, sys

PAT = re.compile(r'(\w+)\s*\[\s*(\d+)\s*\]\s*=\s*\{')


def split_top_level(body):
    """把 {a,b,c} 的顶层元素切开（尊重嵌套与字符串）"""
    items, depth, cur = [], 0, ''
    for ch in body:
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
        if ch == ',' and depth == 0:
            items.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        items.append(cur)
    return items


def find_literals(src):
    out = []
    for m in PAT.finditer(src):
        name, decl = m.group(1), int(m.group(2))
        start = m.end() - 1
        depth, i = 0, start
        while i < len(src):
            if src[i] == '{':
                depth += 1
            elif src[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = src[start + 1:i]
        items = split_top_level(body)
        out.append((m, name, decl, start, i, items))
    return out


def main():
    mode = sys.argv[1]
    root = sys.argv[2]
    total = 0
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            if not fn.endswith(('.c', '.h', '.inc')):
                continue
            p = os.path.join(dirpath, fn)
            with open(p, encoding='utf-8', errors='replace') as f:
                src = f.read()
            lits = find_literals(src)
            edits = []
            for (m, name, decl, start, end, items) in lits:
                if len(items) > decl and len(items) > 1:
                    line = src[:m.start()].count('\n') + 1
                    rel = os.path.relpath(p, root)
                    print("%-28s:%-5d %-12s decl[%d] init=%d  -> 截断到 %d"
                          % (rel, line, name, decl, len(items), decl))
                    edits.append((start + 1, end, ','.join(items[:decl])))
                    total += 1
            if mode == '--apply' and edits:
                for st, en, newbody in sorted(edits, reverse=True):
                    src = src[:st] + newbody + src[en:]
                with open(p, 'w', encoding='utf-8', newline='') as f:
                    f.write(src)
                print("    [已修复] %s" % os.path.relpath(p, root))
    print("---- 共 %d 处超长初始化列表 ----" % total)


if __name__ == '__main__':
    main()
