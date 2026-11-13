"""Write the game's still SVG pictures into content/art from shared palettes and shape helpers."""

from __future__ import annotations

from pathlib import Path

ART: Path = Path(__file__).resolve().parents[1] / 'content' / 'art'
Points = list[tuple[float, float]]


def polygon(points: Points, fill: str, extra: str = '') -> str:
    """Return one filled polygon element."""
    joined: str = ' '.join(f'{x:g},{y:g}' for x, y in points)
    return f'<polygon points="{joined}" fill="{fill}"{extra}/>'


def circle(cx: float, cy: float, radius: float, fill: str, extra: str = '') -> str:
    """Return one filled circle element."""
    return f'<circle cx="{cx:g}" cy="{cy:g}" r="{radius:g}" fill="{fill}"{extra}/>'


def path(data: str, fill: str = 'none', stroke: str = 'none', width: float = 0, extra: str = '') -> str:
    """Return one path element with round joins and caps."""
    stroked: str = f' stroke="{stroke}" stroke-width="{width:g}" stroke-linecap="round" stroke-linejoin="round"' if width else ''
    return f'<path d="{data}" fill="{fill}"{stroked}{extra}/>'


def svg(body: list[str]) -> str:
    """Wrap elements in a 64 by 64 picture."""
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">\n' + '\n'.join(body) + '\n</svg>\n'


def scaled(points: Points, cx: float, cy: float, factor: float) -> Points:
    """Scale points about a centre."""
    return [(cx + (x - cx) * factor, cy + (y - cy) * factor) for x, y in points]


def chunk(points: Points, dark: str, base: str, light: str, glint: tuple[float, float] | None = None) -> list[str]:
    """Return a faceted mineral chunk: dark rim, base body, a lit upper facet and an optional glint."""
    cx: float = sum(x for x, _ in points) / len(points)
    cy: float = sum(y for _, y in points) / len(points)
    upper: Points = sorted(points, key=lambda point: point[1])[:3]
    parts: list[str] = [polygon(points, dark), polygon(scaled(points, cx, cy, .82), base),
                        polygon(scaled(sorted(upper, key=lambda point: point[0]) + [(cx, cy)], cx, cy - 2, .72), light)]
    if glint:
        gx, gy = glint
        parts.append(path(f'M{gx - 4:g} {gy:g}L{gx + 4:g} {gy:g}M{gx:g} {gy - 4:g}L{gx:g} {gy + 4:g}', stroke='#ffffff', width=1.6,
                          extra=' opacity="0.85"'))
    return parts


def ore(dark: str, base: str, light: str, shapes: list[Points], glint: tuple[float, float]) -> str:
    """Return an ore picture of several chunks, the last one carrying the glint."""
    body: list[str] = []
    for index, shape in enumerate(shapes):
        body += chunk(shape, dark, base, light, glint if index == len(shapes) - 1 else None)
    return svg(body)


def pictures() -> dict[str, str]:
    """Return every picture by file stem."""
    blocky: list[Points] = [
        [(8, 30), (18, 16), (32, 14), (38, 28), (30, 42), (14, 42)],
        [(36, 36), (46, 24), (58, 30), (58, 46), (46, 54), (36, 48)],
        [(12, 48), (22, 44), (30, 52), (24, 60), (12, 58)],
    ]
    nuggets: list[Points] = [
        [(10, 34), (14, 22), (26, 16), (36, 22), (38, 34), (28, 44), (16, 44)],
        [(34, 44), (40, 34), (52, 32), (58, 42), (54, 54), (42, 56)],
        [(40, 22), (46, 12), (56, 16), (56, 26), (48, 30)],
    ]
    shards: list[Points] = [
        [(14, 56), (10, 34), (18, 12), (26, 32), (24, 56)],
        [(26, 58), (26, 26), (36, 4), (44, 26), (42, 58)],
        [(42, 58), (44, 38), (52, 22), (58, 40), (54, 58)],
    ]
    result: dict[str, str] = {
        'ore-iron': ore('#3d4c55', '#8fa6ad', '#d6e6e8', blocky, (24, 24)),
        'ore-copper': ore('#6b3a22', '#d98a57', '#f6c39c', nuggets, (24, 26)),
        'ore-gold': ore('#7a5412', '#f1c066', '#fff0b8', nuggets, (46, 40)),
        'ore-crystal': ore('#4a2f6e', '#bb95e0', '#efdcff', shards, (35, 20)),
    }
    result['nest'] = svg([
        path('M8 40C8 22 20 10 32 10C44 10 56 22 56 40C56 52 46 58 32 58C18 58 8 52 8 40Z', fill='#3a1220'),
        path('M13 40C13 26 22 16 32 16C42 16 51 26 51 40C51 49 43 53 32 53C21 53 13 49 13 40Z', fill='#6e1f2e'),
        path('M18 30C22 22 28 19 32 19', stroke='#a03a48', width=2.5),
        circle(24, 38, 6.5, '#1d0a10'), circle(40, 38, 6.5, '#1d0a10'),
        circle(24, 38, 4, '#ff5a3c'), circle(40, 38, 4, '#ff5a3c'),
        circle(23, 37, 1.5, '#ffd9a0'), circle(39, 37, 1.5, '#ffd9a0'),
        path('M24 48L28 45L32 48L36 45L40 48', stroke='#1d0a10', width=2.5),
    ])
    result['chest'] = svg([
        path('M8 30C8 18 16 12 32 12C48 12 56 18 56 30L56 52C56 54 54 56 52 56L12 56C10 56 8 54 8 52Z', fill='#3a2412'),
        path('M12 30C12 21 18 16 32 16C46 16 52 21 52 30L52 32L12 32Z', fill='#a8683a'),
        '<rect x="12" y="34" width="40" height="18" rx="1" fill="#8a5330"/>',
        '<rect x="18" y="16" width="5" height="36" fill="#f1c066"/>',
        '<rect x="41" y="16" width="5" height="36" fill="#f1c066"/>',
        '<rect x="12" y="30" width="40" height="4" fill="#c99438"/>',
        '<rect x="27" y="27" width="10" height="12" rx="2" fill="#fff0b8"/>',
        circle(32, 32, 2, '#3a2412'), '<rect x="31" y="32" width="2" height="5" fill="#3a2412"/>',
        path('M16 22C20 19 26 18 30 18', stroke='#d99a6c', width=2),
    ])
    result['fossil'] = svg([
        path('M10 34C10 20 20 10 34 10C46 10 54 20 54 32C54 46 44 56 30 56C18 56 10 46 10 34Z', fill='#4c4338'),
        path('M32 32C32 29 36 29 36 33C36 38 28 39 27 32C26 24 38 21 42 30C46 41 32 49 23 41C14 32 22 16 36 17',
             stroke='#e8d6b2', width=4.5),
        path('M32 32C32 29 36 29 36 33C36 38 28 39 27 32C26 24 38 21 42 30C46 41 32 49 23 41C14 32 22 16 36 17',
             stroke='#fff6e0', width=1.5),
        path('M40 20L44 15M46 28L52 26M45 39L50 43M33 47L33 53M21 43L16 47', stroke='#e8d6b2', width=2),
    ])
    result['energy'] = svg([
        circle(32, 32, 24, '#78f0d2', ' opacity="0.18"'), circle(32, 32, 17, '#78f0d2', ' opacity="0.22"'),
        polygon([(32, 6), (46, 26), (38, 56), (26, 56), (18, 26)], '#0f5a55'),
        polygon([(32, 11), (42, 27), (36, 52), (28, 52), (22, 27)], '#3fd4b6'),
        polygon([(32, 11), (42, 27), (32, 30)], '#c9fff0'),
        polygon([(34, 22), (27, 35), (32, 35), (29, 47), (38, 31), (33, 31)], '#ffffff'),
    ])
    result['cracked'] = svg([
        polygon([(10, 20), (26, 8), (46, 10), (58, 26), (54, 48), (36, 58), (16, 52), (6, 36)], '#3f342c'),
        polygon([(13, 22), (27, 12), (44, 14), (54, 27), (50, 45), (35, 54), (18, 49), (10, 36)], '#8d7763'),
        polygon([(13, 22), (27, 12), (44, 14), (32, 26)], '#b39a82'),
        path('M32 14L28 26L36 32L30 42L34 54M28 26L16 30M36 32L50 30M30 42L20 46', stroke='#2a1c14', width=4),
        path('M32 14L28 26L36 32L30 42L34 54M28 26L16 30M36 32L50 30M30 42L20 46', stroke='#ff966e', width=1.6),
    ])
    result['tool-pick'] = svg([
        path('M22 44L46 20', stroke='#5a3a20', width=7), path('M22 44L46 20', stroke='#b07a45', width=4),
        path('M30 12C40 10 52 16 56 28C50 22 42 20 36 22Z', fill='#d6e6e8', stroke='#3d4c55', width=2.5),
        path('M30 12C24 16 20 22 20 30C24 24 30 20 36 22Z', fill='#8fa6ad', stroke='#3d4c55', width=2.5),
        path('M12 54L22 44', stroke='#5a3a20', width=7), path('M12 54L22 44', stroke='#b07a45', width=4),
    ])
    result['tool-blast'] = svg([
        '<rect x="14" y="26" width="12" height="30" rx="3" fill="#b53a2c" stroke="#4a140e" stroke-width="2.5"/>',
        '<rect x="26" y="22" width="12" height="34" rx="3" fill="#d24a36" stroke="#4a140e" stroke-width="2.5"/>',
        '<rect x="38" y="26" width="12" height="30" rx="3" fill="#b53a2c" stroke="#4a140e" stroke-width="2.5"/>',
        '<rect x="12" y="36" width="40" height="7" rx="2" fill="#f1c066" stroke="#4a140e" stroke-width="2"/>',
        path('M32 22C32 14 40 16 42 10', stroke='#e8d6b2', width=2.5),
        polygon([(44, 2), (46, 7), (51, 7), (47, 10), (49, 15), (44, 12), (39, 15), (41, 10), (37, 7), (42, 7)], '#ffd76a'),
    ])
    result['tool-scan'] = svg([
        circle(32, 32, 25, 'none', ' stroke="#6db8dd" stroke-width="3" opacity="0.45"'),
        circle(32, 32, 16, 'none', ' stroke="#6db8dd" stroke-width="3" opacity="0.75"'),
        circle(32, 32, 7, '#bfe9ff'),
        path('M32 32L50 14', stroke='#bfe9ff', width=3),
        polygon([(32, 32), (50, 14), (54, 26)], '#6db8dd', ' opacity="0.35"'),
        circle(46, 42, 3, '#f1c066'),
    ])
    return result


def main() -> None:
    """Write every picture, replacing earlier output."""
    ART.mkdir(parents=True, exist_ok=True)
    for stem, source in pictures().items():
        (ART / f'{stem}.svg').write_text(source, encoding='utf-8')
        print(f'wrote content/art/{stem}.svg ({len(source)} bytes)')


if __name__ == '__main__':
    main()
