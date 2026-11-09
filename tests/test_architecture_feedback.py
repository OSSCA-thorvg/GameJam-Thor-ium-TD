"""Preserve explicit architecture feedback without approving unchosen details."""

from __future__ import annotations

import unittest

from test_planning_docs import read_document, table_rows


class ArchitectureFeedbackTests(unittest.TestCase):
    """Check selection status and the documented engine/game dependency boundary."""

    def test_cross_platform_ui_stays_a_proposal_with_separate_platform_validation(self) -> None:
        """Keep investigated capabilities separate from adopted scope and executed tests."""
        records: list[list[str]] = [
            row for row in table_rows(read_document('docs/decisions.md'))
            if len(row) == 4 and row[0] == 'D-62'
        ]
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0][1], '검토 방향')
        content: str = read_document('docs/cross-platform-ui-proposal.md')
        self.assertIn('미확정 제안', content)
        self.assertIn('SDL2 기반', content)
        self.assertIn('SDL3로 그대로 복사 가능한 코드는 아니므로', content)
        self.assertIn('웹 배포는 유지', content)
        self.assertIn('앞으로 수행할 게임·플랫폼 테스트', content)
        self.assertIn('게임 개체에 동일 UI 베이스 클래스를 강제하지 않습니다', content)

    def test_sdl_diagram_distinguishes_dependency_and_runtime_event_directions(self) -> None:
        """Prevent generic UI from gaining a dependency on game-specific screens or SDL."""
        content: str = read_document('docs/cross-platform-ui-proposal.md')
        section: str = content.split('## SDL 반영 구조도 — 2026-11-04\n', 1)[1].split('## 라이브러리와 우리 코드의 역할', 1)[0]
        graph: str = section.split('~~~mermaid\n', 1)[1].split('~~~', 1)[0]
        self.assertIn('view --> ui', graph)
        self.assertIn('view --> render', graph)
        self.assertIn('ui --> render', graph)
        self.assertNotIn('ui --> view', graph)
        self.assertNotIn('rules -->', graph)
        self.assertIn('엔진 UI·그래픽에서 게임 코드나 SDL 이벤트 타입을 참조하지 않으며', section)
        self.assertIn('엔진 UI에서 게임 화면을 include하거나 카드 규칙을 직접 호출하는 의미는 아닙니다', section)

    def test_selected_folder_principle_preserves_engine_boundary(self) -> None:
        """Retain the chosen principle, an independent engine, and pending detail review."""
        records: dict[str, list[str]] = {
            row[0]: row for row in table_rows(read_document('docs/decisions.md'))
            if len(row) == 4 and row[0] in {'D-60', 'D-61'}
        }
        self.assertEqual(set(records), {'D-60', 'D-61'})
        for record in records.values():
            self.assertEqual(record[1], '확정 방향')

        overview: str = read_document('docs/architecture-overview.md')
        current_tree: str = overview.split('### 폴더 B · 실행 경계 + 기능별\n', 1)[1].split('### 폴더 C', 1)[0]
        self.assertIn('\n  engine/', current_tree)
        self.assertIn('\n  game/\n    rules/', current_tree)
        self.assertIn('\n    presentation/', current_tree)
        self.assertIn('엔진에서 게임으로 향하는 의존은 두지 않습니다', overview)
        self.assertIn('엔진 세부 범위·정확한 폴더 이름·빌드 타깃·게임 구현은 아직 검토 대상', overview)


if __name__ == '__main__':
    unittest.main()
