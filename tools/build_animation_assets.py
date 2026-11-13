"""Author original creature movement, attack and death segments, one Lottie asset per enemy role."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from make_assets import CREATURES

ROOT: Path = Path(__file__).resolve().parents[1]


def channel(values: list[tuple[int, list[float]]], holds: set[int]) -> dict[str, Any]:
    """Build explicit transform keyframes with holds at semantic clip boundaries."""
    keys: list[dict[str, Any]] = []
    for index, (frame, value) in enumerate(values):
        key: dict[str, Any] = {'t': frame, 's': value}
        if index + 1 < len(values):
            key.update({'e': values[index + 1][1], 'i': {'x': [.67], 'y': [1]},
                        'o': {'x': [.33], 'y': [0]}})
            if frame in holds:
                key['h'] = 1
        keys.append(key)
    return {'a': 1, 'k': keys}


def build(key: str) -> None:
    """Extend one original vector character while preserving its hand-authored shapes."""
    path: Path = ROOT / f'content/animations/{key}.json'
    asset: dict[str, Any] = json.loads(path.read_text(encoding='utf-8'))
    asset['op'] = 75
    asset['markers'] = [
        {'cm': 'idle', 'tm': 0, 'dr': 1},
        {'cm': 'move', 'tm': 0, 'dr': 45},
        {'cm': 'attack', 'tm': 45, 'dr': 12},
        {'cm': 'death', 'tm': 57, 'dr': 18},
    ]
    layer: dict[str, Any] = asset['layers'][0]
    layer['op'] = 75
    transform: dict[str, Any] = layer['ks']
    transform['s'] = channel([(0, [100, 100, 100]), (22, [106, 93, 100]),
                              (44, [100, 100, 100]), (45, [100, 100, 100]),
                              (49, [115, 85, 100]), (52, [90, 115, 100]),
                              (56, [100, 100, 100]), (57, [100, 100, 100]),
                              (64, [115, 70, 100]), (74, [125, 12, 100])], {44, 56})
    transform['p'] = channel([(0, [50, 80, 0]), (44, [50, 80, 0]),
                              (45, [50, 80, 0]), (50, [58, 78, 0]),
                              (56, [50, 80, 0]), (57, [50, 80, 0]),
                              (74, [50, 94, 0])], {0, 44, 56})
    transform['r'] = channel([(0, [0]), (57, [0]), (65, [-18]), (74, [-55])], {0})
    transform['o'] = channel([(0, [100]), (57, [100]), (64, [100]), (74, [0])], {0, 57})
    path.write_text(json.dumps(asset, ensure_ascii=False, separators=(',', ':')) + '\n', encoding='utf-8')


if __name__ == '__main__':
    for creature in CREATURES:
        build(creature)
