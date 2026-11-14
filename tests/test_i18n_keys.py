"""Check that interface text looked up by key exists in every language table."""

from __future__ import annotations

from pathlib import Path
import json
import re
import unittest


ROOT: Path = Path(__file__).resolve().parents[1]
I18N: Path = ROOT / 'content' / 'i18n'
LOOKUP: re.Pattern[str] = re.compile(r'\btr\(\s*"([^"]+)"\s*[,)]')
HANGUL: re.Pattern[str] = re.compile(r'"[^"\n]*[가-힣][^"\n]*"')


def table(code: str) -> dict[str, str]:
    """Read one language table."""
    return json.loads((I18N / f'{code}.json').read_text(encoding='utf-8'))


class I18nKeyTests(unittest.TestCase):
    """Keep literal lookups resolvable and migrated screens free of inline Korean text."""

    def test_literal_keys_exist_in_every_language(self) -> None:
        """Every `tr("key")` in the sources names a string each table defines."""
        tables: dict[str, dict[str, str]] = {path.stem: table(path.stem) for path in sorted(I18N.glob('*.json'))}
        self.assertIn('ko', tables)
        for path in sorted((ROOT / 'src').rglob('*.cpp')):
            for key in LOOKUP.findall(path.read_text(encoding='utf-8')):
                for code, strings in tables.items():
                    with self.subTest(path=path.name, key=key, language=code):
                        self.assertIn(key, strings)

    def test_migrated_screens_have_no_inline_korean(self) -> None:
        """Screens whose text moved to the tables keep it there."""
        for relative in ('src/game/presentation/FrontEnd.cpp',):
            text: str = (ROOT / relative).read_text(encoding='utf-8')
            with self.subTest(path=relative):
                self.assertEqual(HANGUL.findall(text), [])


if __name__ == '__main__':
    unittest.main()
