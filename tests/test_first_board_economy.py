"""Check the proposed board's paper ledgers without running game or rendering code."""

from __future__ import annotations

import re
import unittest
from dataclasses import dataclass

from test_planning_docs import read_document, table_rows


RESOURCE_NAMES: tuple[str, ...] = ('철', '구리', '수정', '금')


@dataclass(frozen=True)
class TileNumbers:
    """Hold the tool requirement and yields transcribed from one symbol table row."""

    minimum_tool: int
    yields: tuple[int, ...]
    nest: bool


@dataclass(frozen=True)
class RecipeNumbers:
    """Hold the resource costs and tool condition declared by one proposed recipe."""

    costs: tuple[int, ...]
    minimum_tool: int


def coordinate(label: str) -> tuple[int, int]:
    """Convert a bounded document coordinate into column and depth integers."""
    match: re.Match[str] | None = re.fullmatch(r'([A-E])([0-5])', label)
    if match is None:
        raise ValueError(f'Invalid paper-board coordinate: {label}')
    return ord(match.group(1)) - ord('A'), int(match.group(2))


class FirstBoardEconomyTests(unittest.TestCase):
    """Verify example routes, not combat balance or the future game's implementation."""

    def setUp(self) -> None:
        """Read the canonical Markdown data before checking its paper calculations."""
        self.content: str = read_document('docs/first-board-economy.md')
        self.rows: list[list[str]] = table_rows(self.content)
        self.symbols: dict[str, TileNumbers] = {
            row[0]: TileNumbers(int(row[2]), tuple(map(int, row[3:7])), row[7] == '1')
            for row in self.rows
            if len(row) == 8 and row[2].isdigit() and row[7] in {'0', '1'}
        }
        self.board: dict[tuple[int, int], str] = {
            (column, int(row[0])): symbol
            for row in self.rows
            if len(row) == 6 and row[0].isdigit()
            for column, symbol in enumerate(row[1:])
        }
        self.recipes: dict[str, RecipeNumbers] = {
            row[0]: RecipeNumbers(tuple(map(int, row[1:5])), int(row[5]))
            for row in self.rows
            if len(row) == 7 and all(cell.isdigit() for cell in row[1:6])
        }
        self.routes: dict[str, list[list[str]]] = {}
        for row in self.rows:
            if len(row) == 8 and row[1] in {'1', '2'} and row[2].startswith('dig:'):
                self.routes.setdefault(row[0], []).append(row)

    def test_board_definitions_and_initial_safety(self) -> None:
        """Reject missing symbols, malformed resource vectors, or an unsafe initial gallery."""
        self.assertEqual(len(self.board), 30)
        self.assertEqual(set(self.board.values()) - set(self.symbols), set())
        self.assertEqual(set(self.recipes), {'T2', 'armor', 'coil', 'recruit', 'slot8', 'heal'})
        for symbol, numbers in self.symbols.items():
            with self.subTest(symbol=symbol):
                self.assertEqual(len(numbers.yields), len(RESOURCE_NAMES))
                self.assertTrue(all(value >= 0 for value in numbers.yields))
        initial: set[tuple[int, int]] = {
            cell for cell, symbol in self.board.items() if symbol == '.'
        }
        nests: set[tuple[int, int]] = {
            cell for cell, symbol in self.board.items() if self.symbols[symbol].nest
        }
        self.assertEqual(initial, {coordinate(label) for label in ('B0', 'C0', 'D0')})
        self.assertEqual(nests, {coordinate(label) for label in ('A3', 'E2')})
        for column, depth in initial:
            with self.subTest(initial_cell=(column, depth)):
                self.assertFalse(any(
                    abs(nest_column - column) <= 1 and abs(nest_depth - depth) <= 1
                    for nest_column, nest_depth in nests
                ))

    def test_example_routes_fit_dig_budgets_and_declared_balances(self) -> None:
        """Calculate four ledgers and check adjacency, tools, spending, and delayed threats."""
        self.assertEqual(set(self.routes), {'도구·모집', '장갑·부대', '장갑·도구', '위험·모집'})
        threat_match: re.Match[str] | None = re.search(
            r'다음 웨이브 일반 적 \+(\d+)', self.content
        )
        self.assertIsNotNone(threat_match)
        if threat_match is None:
            return
        extra_enemies: int = int(threat_match.group(1))
        for name, phases in self.routes.items():
            balances: list[int] = [0] * len(RESOURCE_NAMES)
            opened: set[tuple[int, int]] = {
                cell for cell, symbol in self.board.items() if symbol == '.'
            }
            tool: int = 1
            one_time_purchases: set[str] = set()
            threats: dict[int, int] = {}
            self.assertEqual([row[1] for row in phases], ['1', '2'])
            for row in phases:
                phase: int = int(row[1])
                digs: int = 0
                recruits: int = 0
                for action in row[2].split():
                    verb: str
                    target: str
                    verb, target = action.split(':', 1)
                    with self.subTest(route=name, phase=phase, action=action):
                        if verb == 'dig':
                            cell: tuple[int, int] = coordinate(target)
                            self.assertIn(cell, self.board)
                            self.assertNotIn(cell, opened)
                            self.assertTrue(any(
                                abs(cell[0] - other[0]) + abs(cell[1] - other[1]) == 1
                                for other in opened
                            ), 'The target must touch the connected open gallery.')
                            tile: TileNumbers = self.symbols[self.board[cell]]
                            self.assertGreaterEqual(tool, tile.minimum_tool)
                            balances = [
                                balance + value for balance, value in zip(balances, tile.yields)
                            ]
                            opened.add(cell)
                            digs += 1
                            if tile.nest:
                                threats[phase + 1] = threats.get(phase + 1, 0) + extra_enemies
                        else:
                            self.assertEqual(verb, 'use')
                            self.assertIn(target, self.recipes)
                            recipe: RecipeNumbers = self.recipes[target]
                            self.assertGreaterEqual(tool, recipe.minimum_tool)
                            self.assertTrue(all(
                                balance >= cost for balance, cost in zip(balances, recipe.costs)
                            ), f'Insufficient paper resources for {target}: {balances}')
                            balances = [
                                balance - cost for balance, cost in zip(balances, recipe.costs)
                            ]
                            if target == 'T2':
                                self.assertEqual(tool, 1)
                                tool = 2
                            elif target == 'recruit':
                                recruits += 1
                                self.assertLessEqual(recruits, 1)
                            elif target in {'armor', 'coil', 'slot8'}:
                                self.assertNotIn(target, one_time_purchases)
                                one_time_purchases.add(target)
                                if target == 'coil':
                                    self.assertEqual(phase, 2, 'Lightning is assumed acquired in prep 2.')
                        self.assertTrue(all(value >= 0 for value in balances))
                with self.subTest(route=name, phase=phase, result=True):
                    self.assertEqual(digs, 5)
                    self.assertEqual(balances, list(map(int, row[3:7])))
                    self.assertEqual(tool, int(row[7]))
            with self.subTest(route=name, delayed_threats=True):
                self.assertEqual(threats, {3: extra_enemies} if name == '위험·모집' else {})


if __name__ == '__main__':
    unittest.main(verbosity=2)
