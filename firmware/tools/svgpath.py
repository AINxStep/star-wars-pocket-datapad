"""Minimal SVG path reader shared by the generators: single absolute M/C/L/Z
path data flattened into polylines. No dependencies."""
import re


def _bezier(p0, p1, p2, p3, n=24):
    out = []
    for k in range(1, n + 1):
        t = k / n
        u = 1 - t
        out.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                    u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    return out


def load_polys(svg_file):
    """Returns a list of closed polylines [(x, y), ...] from the first path's d attribute."""
    d = re.search(r'\sd="([^"]+)"', open(svg_file).read()).group(1)
    tokens = re.findall(r'[MCLZ]|-?\d*\.?\d+', d)

    polys, cur, pos, i, cmd = [], [], (0, 0), 0, None
    while i < len(tokens):
        t = tokens[i]
        if t in "MCLZ":
            cmd = t
            i += 1
            if cmd == "Z":
                if cur:
                    polys.append(cur)
                    cur = []
                continue
        if cmd == "M":
            if cur:
                polys.append(cur)
            pos = (float(tokens[i]), float(tokens[i + 1]))
            i += 2
            cur = [pos]
            cmd = "L"
        elif cmd == "L":
            pos = (float(tokens[i]), float(tokens[i + 1]))
            i += 2
            cur.append(pos)
        elif cmd == "C":
            p = [(float(tokens[i + k]), float(tokens[i + k + 1])) for k in (0, 2, 4)]
            i += 6
            cur += _bezier(pos, *p)
            pos = p[2]
    if cur:
        polys.append(cur)
    return polys
