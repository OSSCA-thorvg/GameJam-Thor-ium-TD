"""Check the proposed first-playable tables without simulating or implementing combat."""

from __future__ import annotations

import re
import unittest
from collections import Counter
from fractions import Fraction
from typing import TypeAlias

from test_planning_docs import read_document, table_rows


Point: TypeAlias = tuple[int, int]


def points_in(value: str) -> list[Point]:
    """Read document coordinate pairs without interpreting them as executable code."""
    return [(int(x), int(y)) for x, y in re.findall(r'\((\d+),(\d+)\)', value)]


def route_cells(points: list[Point]) -> list[Point]:
    """Expand cardinal segments to inspect the specification's map geometry."""
    if len(points) < 2:
        raise ValueError('A paper route needs at least two vertices.')
    cells: list[Point] = [points[0]]
    for start, end in zip(points, points[1:]):
        dx: int = end[0] - start[0]
        dy: int = end[1] - start[1]
        if (dx == 0) == (dy == 0):
            raise ValueError('A segment must move along exactly one axis.')
        length: int = abs(dx) + abs(dy)
        step_x: int = (dx > 0) - (dx < 0)
        step_y: int = (dy > 0) - (dy < 0)
        cells.extend(
            (start[0] + step_x * offset, start[1] + step_y * offset)
            for offset in range(1, length + 1)
        )
    return cells


class FirstPlayableSpecTests(unittest.TestCase):
    """Catch inconsistent definitions, budgets, timing, and proposed map coordinates."""

    def setUp(self) -> None:
        """Read semantic sections so unrelated tables cannot silently become content data."""
        self.content: str = read_document('docs/first-playable-spec.md')
        all_rows: list[list[str]] = table_rows(self.content)
        self.metrics: dict[str, Fraction] = {
            row[0]: Fraction(row[1]) for row in all_rows
            if len(row) == 3 and re.fullmatch(r'\d+(?:\.\d+)?', row[1])
        }
        card_section: str = self.content.split('## P-02 —', 1)[1].split('## P-03 —', 1)[0]
        self.cards: dict[str, list[str]] = {
            row[0]: row for row in table_rows(card_section)
            if len(row) == 5 and row[3].isdigit() and row[4].isdigit()
        }
        attack_section: str = self.content.split('## P-03 —', 1)[1].split('## P-04 —', 1)[0]
        self.towers: dict[str, list[str]] = {
            row[0]: row for row in table_rows(attack_section)
            if len(row) == 8 and row[1] in {'물리', '전격'}
        }
        self.spells: dict[str, list[str]] = {
            row[0]: row for row in table_rows(attack_section)
            if len(row) == 6 and row[5] == '허용'
        }
        self.levels: dict[int, Fraction] = {
            int(row[0]): Fraction(row[1]) for row in table_rows(attack_section)
            if len(row) == 2 and row[0].isdigit()
        }
        unit_section: str = self.content.split('## P-04 —', 1)[1].split('## P-05 —', 1)[0]
        self.units: dict[str, list[str]] = {
            row[0]: row for row in table_rows(unit_section)
            if len(row) == 7 and row[1].isdigit()
        }
        wave_section: str = self.content.split('## P-09 —', 1)[1].split('## P-10 —', 1)[0]
        self.enemies: dict[str, list[str]] = {
            row[0]: row for row in table_rows(wave_section)
            if len(row) == 8 and row[2].isdigit()
        }
        self.waves: list[list[str]] = [
            row for row in table_rows(wave_section)
            if len(row) == 6 and row[0].isdigit()
        ]

    def test_card_definitions_and_opening_budget(self) -> None:
        """Require eight starting cards, valid combat definitions, and an affordable opener."""
        self.assertEqual(len(self.cards), 8)
        self.assertEqual(sum(int(row[3]) for row in self.cards.values()), 8)
        self.assertEqual(set(self.towers), {
            identifier for identifier, row in self.cards.items() if row[2] == '건물'
        })
        self.assertEqual(set(self.units), {
            identifier for identifier, row in self.cards.items() if row[2] == '유닛'
        })
        self.assertEqual(set(self.spells), {
            identifier for identifier, row in self.cards.items() if row[2] == '마법'
        })
        opening: re.Match[str] | None = re.search(r'첫 활성 덱 순서는 `([^`]+)`', self.content)
        self.assertIsNotNone(opening)
        if opening is None:
            return
        instances: list[str] = opening.group(1).split(', ')
        self.assertEqual(len(instances), 8)
        self.assertEqual(len(set(instances)), 8)
        self.assertEqual(Counter(item.split('#')[0] for item in instances), Counter({
            identifier: int(row[3]) for identifier, row in self.cards.items() if int(row[3]) > 0
        }))
        self.assertEqual({item.split('#')[0] for item in instances[:4]}, {'gun', 'guard', 'cannon', 'frost'})
        for identifier, row in self.cards.items():
            with self.subTest(card=identifier):
                self.assertGreater(int(row[4]), 0)
                self.assertLessEqual(int(row[4]), self.metrics['에너지 상한'])
        self.assertEqual(int(self.cards['gun'][4]) + int(self.cards['guard'][4]), self.metrics['전투 시작 에너지'])
        self.assertEqual(set(self.levels), set(range(1, int(self.metrics['건물 최대 레벨']) + 1)))

    def test_wave_counts_and_times_fit_the_proposed_clock(self) -> None:
        """Verify explicit spawn times, group gaps, fixed-tick durations, and leak budgets."""
        tick_rate: Fraction = self.metrics['시뮬레이션 주기']
        counts: Counter[int] = Counter()
        last_spawn: dict[int, Fraction] = {}
        leak_damage: Counter[int] = Counter()
        for row in self.waves:
            wave: int = int(row[0])
            enemy: str = row[1]
            count: int = int(row[2])
            first: Fraction = Fraction(row[3])
            interval: Fraction = Fraction(row[4])
            last: Fraction = Fraction(row[5])
            with self.subTest(wave=wave, enemy=enemy):
                self.assertIn(enemy, self.enemies)
                self.assertGreater(count, 0)
                self.assertGreater(interval, 0)
                self.assertEqual(last, first + (count - 1) * interval)
                self.assertEqual(first, last_spawn[wave] + Fraction('1.2') if wave in last_spawn else self.metrics['각 전투 첫 적 등장'])
                for duration in (first, interval, last):
                    self.assertEqual((duration * tick_rate).denominator, 1)
            counts[wave] += count
            leak_damage[wave] += count * int(self.enemies[enemy][7])
            last_spawn[wave] = last
        self.assertEqual(counts, Counter({1: 16, 2: 28, 3: 44}))
        self.assertEqual(sum(counts.values()), 88)
        self.assertLess(leak_damage[1], self.metrics['거점 최대 HP'])
        self.assertGreater(leak_damage[2], self.metrics['거점 최대 HP'])
        periods: list[Fraction] = [Fraction(row[3]) for row in self.towers.values()]
        periods.extend(Fraction(row[4]) for row in self.units.values())
        periods.extend(Fraction(row[6]) for row in self.enemies.values())
        periods.extend(Fraction(row[3]) for row in self.spells.values())
        periods.extend(value for key, value in self.metrics.items() if key in {
            '에너지 회복 간격', '광부 1칸 이동 시간', '일반 타일 굴착 시간', '단단한 암반 굴착 시간',
        })
        self.assertTrue(all((period * tick_rate).denominator == 1 for period in periods))
        economy: str = read_document('docs/first-board-economy.md')
        extra: re.Match[str] | None = re.search(r'다음 웨이브 일반 적 \+(\d+)', economy)
        self.assertIsNotNone(extra)
        if extra is not None:
            self.assertEqual(sum(counts.values()) + 2 * int(extra.group(1)), 96)

    def test_surface_expansions_preserve_routes_and_placement_cells(self) -> None:
        """Check cardinal routes, stated lengths, legal unit cells, and unobstructed pads."""
        section: str = self.content.split('## P-08 —', 1)[1].split('## P-09 —', 1)[0]
        routes: dict[str, list[str]] = {
            row[0]: row for row in table_rows(section)
            if len(row) == 4 and row[0] in {'core', 'long', 'short'}
        }
        areas: dict[str, list[str]] = {
            row[0]: row for row in table_rows(section)
            if len(row) == 3 and row[0] in {'core', 'long', 'short'}
        }
        self.assertEqual(set(routes), {'core', 'long', 'short'})
        self.assertEqual(set(areas), set(routes))
        core: list[Point] = route_cells(points_in(routes['core'][1]))
        self.assertEqual(len(core) - 1, int(routes['core'][2]))
        for name, row in routes.items():
            segment: list[Point] = route_cells(points_in(row[1]))
            combined: list[Point] = segment if name == 'core' else segment[:-1] + core
            with self.subTest(area=name):
                if name != 'core':
                    self.assertEqual(segment[-1], core[0])
                self.assertEqual(len(segment) - 1, int(row[2]))
                self.assertEqual(len(combined) - 1, int(row[3]))
                self.assertEqual(len(combined), len(set(combined)))
                self.assertEqual(combined[-1], (16, 5))
                pads: list[Point] = points_in(areas['core'][1])
                units: list[Point] = points_in(areas['core'][2])
                if name != 'core':
                    pads += points_in(areas[name][1])
                    units += points_in(areas[name][2])
                self.assertEqual(len(pads), len(set(pads)))
                self.assertEqual(len(units), len(set(units)))
                self.assertTrue(set(units) <= set(combined))
                self.assertFalse(set(pads) & set(combined))
                self.assertTrue(all(0 <= x <= 17 and 0 <= y <= 9 for x, y in combined + pads + units))
        self.assertGreater(int(routes['long'][3]), int(routes['short'][3]))
        self.assertLess(len(points_in(areas['long'][1])), len(points_in(areas['short'][1])))
        self.assertEqual(len(points_in(areas['core'][2])), 8)
        first_pad: Point = points_in(areas['core'][1])[0]
        first_unit: Point = points_in(areas['core'][2])[0]
        self.assertLessEqual(
            (first_pad[0] - first_unit[0]) ** 2 + (first_pad[1] - first_unit[1]) ** 2,
            Fraction(self.towers['gun'][4]) ** 2,
        )

    def test_level_equipment_and_healing_arithmetic(self) -> None:
        """Check proposed integer rounding and compatibility with the existing economy effects."""
        economy: str = read_document('docs/first-board-economy.md')
        armor: re.Match[str] | None = re.search(r'최대 HP \+(\d+)%', economy)
        coil: re.Match[str] | None = re.search(r'전격탑 피해 \+(\d+)%', economy)
        heal: re.Match[str] | None = re.search(r'최대 HP (\d+)%까지', economy)
        spec_coil: re.Match[str] | None = re.search(r'코일 대상 전격탑은 추가 (\d+(?:\.\d+)?)배', self.content)
        spec_heal: re.Match[str] | None = re.search(r'치료는[^\n]+최대 HP (\d+)%를', self.content)
        self.assertIsNotNone(armor)
        self.assertIsNotNone(coil)
        self.assertIsNotNone(heal)
        self.assertIsNotNone(spec_coil)
        self.assertIsNotNone(spec_heal)
        if armor is None or coil is None or heal is None or spec_coil is None or spec_heal is None:
            return
        coil_multiplier: Fraction = 1 + Fraction(int(coil.group(1)), 100)
        self.assertEqual(Fraction(spec_coil.group(1)), coil_multiplier)
        self.assertEqual(heal.group(1), spec_heal.group(1))
        gun_hits: list[int] = [int(int(self.towers['gun'][2]) * value) for value in self.levels.values()]
        self.assertEqual(gun_hits, [12, 15, 18])
        self.assertEqual([
            int(int(self.towers['lightning'][2]) * value * coil_multiplier)
            for value in self.levels.values()
        ], [12, 15, 18])
        self.assertEqual(max(1, gun_hits[0] - int(self.enemies['armored'][3])), 8)
        guard_max: int = int(int(self.units['guard'][1]) * (1 + Fraction(int(armor.group(1)), 100)))
        self.assertEqual(guard_max, 150)
        healing: Fraction = guard_max * Fraction(int(heal.group(1)), 100)
        healing_ceiling: int = -(-healing.numerator // healing.denominator)
        self.assertEqual(healing_ceiling, 75)
        self.assertEqual(min(guard_max, 140 + healing_ceiling), 150)
        self.assertEqual(self.spells['frost'][1], '0')
        self.assertEqual(Fraction(self.spells['frost'][4]), Fraction('0.6'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
