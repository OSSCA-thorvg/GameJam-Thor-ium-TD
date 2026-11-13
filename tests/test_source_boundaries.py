"""Protect architectural dependency directions rather than particular folder contents."""

from __future__ import annotations

from pathlib import Path
import re
import unittest


ROOT: Path = Path(__file__).resolve().parents[1]
INCLUDES: re.Pattern[str] = re.compile(r'^\s*#include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def sources(relative: str) -> list[Path]:
    """Collect handwritten C++ headers and implementation files under one boundary."""
    return sorted(path for path in (ROOT / relative).rglob('*') if path.suffix in {'.h', '.cpp'})


class SourceBoundaryTests(unittest.TestCase):
    """Reject dependency regressions that would prevent headless rules or reusable engine code."""

    def test_engine_has_no_game_or_platform_dependencies(self) -> None:
        """Keep game assets, widget policy and SDL ownership outside generic engine modules."""
        for path in sources('src/engine'):
            text: str = path.read_text(encoding='utf-8')
            for include in INCLUDES.findall(text):
                with self.subTest(path=path, include=include):
                    self.assertFalse(include.startswith(('game/', 'app/', 'platform/', 'SDL')))
            self.assertNotIn('THORIUM_CONTENT_DIR', text)
            self.assertNotIn('oreling', text)
            self.assertNotIn('NotoSansKR', text)

    def test_rules_and_content_stay_headless(self) -> None:
        """Keep drawing, input devices and screen navigation out of gameplay state mutations."""
        for boundary in ('src/game/rules', 'src/game/content', 'src/game/model', 'src/game/autoplay'):
            for path in sources(boundary):
                for include in INCLUDES.findall(path.read_text(encoding='utf-8')):
                    with self.subTest(path=path, include=include):
                        self.assertFalse(include.startswith(('engine/', 'platform/', 'app/', 'game/presentation/', 'SDL', 'thorvg')))

    def test_controllers_do_not_need_a_view_or_render_device(self) -> None:
        """Keep app-flow and placement contracts testable without initializing graphics."""
        for name in ('AppFlow', 'GameplayController', 'FeedbackChannel', 'GameInput', 'TutorialDirector', 'MapCamera'):
            for suffix in ('.h', '.cpp'):
                path: Path = ROOT / 'src/app' / (name + suffix)
                if not path.exists():
                    continue
                for include in INCLUDES.findall(path.read_text(encoding='utf-8')):
                    with self.subTest(path=path, include=include):
                        self.assertNotIn(include, {'Application.h', 'game/presentation/View.h', 'engine/render/Renderer.h'})
                        self.assertFalse(include.startswith(('SDL', 'thorvg', 'platform/')))

    def test_consumers_cannot_read_global_card_classification(self) -> None:
        """Prevent authored card behavior from drifting between a run and its input or UI."""
        for boundary in ('src/app', 'src/game/presentation'):
            for path in sources(boundary):
                text: str = path.read_text(encoding='utf-8')
                with self.subTest(path=path):
                    self.assertNotIn('game::isBuilding(', text)
                    self.assertNotIn('game::isUnit(', text)
                    if path.name != 'Application.h':
                        self.assertNotIn('defaultCatalog()', text)

    def test_privileged_scenarios_never_ship_in_runtime_sources(self) -> None:
        """Keep test fixture mutation helpers out of application compilation units."""
        for path in sources('src'):
            with self.subTest(path=path):
                self.assertFalse(any(include.endswith('Scenario.h') for include in INCLUDES.findall(path.read_text(encoding='utf-8'))))


if __name__ == '__main__':
    unittest.main()
