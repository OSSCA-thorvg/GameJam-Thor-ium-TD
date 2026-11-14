"""Check that interface text looked up by key exists in every language table."""

from __future__ import annotations

from pathlib import Path
import json
import re
import unittest


ROOT: Path = Path(__file__).resolve().parents[1]
I18N: Path = ROOT / 'content' / 'i18n'
LOOKUP: re.Pattern[str] = re.compile(r'\b(?:tr|Message)\(\s*"([^"]+)"\s*[,)]')
REF: re.Pattern[str] = re.compile(r'\bref\(\s*"([^"]+)"\s*\)')
LANGUAGE_NAMES: str = 'src/game/presentation/Text.cpp'
HANGUL: re.Pattern[str] = re.compile(r'"[^"\n]*[가-힣][^"\n]*"')


def table(code: str) -> dict[str, str]:
    """Read one language table."""
    return json.loads((I18N / f'{code}.json').read_text(encoding='utf-8'))


class I18nKeyTests(unittest.TestCase):
    """Keep literal lookups resolvable and migrated screens free of inline Korean text."""

    def test_literal_keys_exist_in_every_language(self) -> None:
        """Every literal key passed to `tr`, `Message` or `ref` in the sources names a string each table defines."""
        tables: dict[str, dict[str, str]] = {path.stem: table(path.stem) for path in sorted(I18N.glob('*.json'))}
        self.assertIn('ko', tables)
        for path in sorted((ROOT / 'src').rglob('*')):
            if path.suffix not in {'.cpp', '.h'}:
                continue
            text: str = path.read_text(encoding='utf-8')
            for key in LOOKUP.findall(text) + REF.findall(text):
                for code, strings in tables.items():
                    with self.subTest(path=path.name, key=key, language=code):
                        self.assertIn(key, strings)

    def test_sources_have_no_inline_korean(self) -> None:
        """Display text lives in the tables; only the language picker names each language in its own script."""
        for path in sorted((ROOT / 'src').rglob('*')):
            if path.suffix not in {'.cpp', '.h'}:
                continue
            found: list[str] = HANGUL.findall(path.read_text(encoding='utf-8'))
            if path.relative_to(ROOT).as_posix() == LANGUAGE_NAMES:
                found = [item for item in found if item != '"한국어"']
            with self.subTest(path=path.relative_to(ROOT).as_posix()):
                self.assertEqual(found, [])

if __name__ == '__main__':
    unittest.main()
