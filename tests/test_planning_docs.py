"""Check navigation, decision status, and deck examples in the planning documents."""

from __future__ import annotations

import re
import unittest
from pathlib import Path
from urllib.parse import SplitResult, unquote, urlsplit


PROJECT: Path = Path(__file__).resolve().parents[1]
DOCUMENTS: tuple[Path, ...] = (PROJECT / 'README.md', *sorted((PROJECT / 'docs').rglob('*.md')))
LINK_PATTERN: re.Pattern[str] = re.compile(r'\[[^\]\n]+\]\(([^\n]+?)\)')


def read_document(relative_path: str) -> str:
    """Read one project document using the expected UTF-8 encoding."""
    return (PROJECT / relative_path).read_text(encoding='utf-8')


def local_targets(document: Path) -> set[Path]:
    """Resolve local Markdown links without accessing external websites."""
    targets: set[Path] = set()
    content: str = document.read_text(encoding='utf-8')
    for destination in LINK_PATTERN.findall(content):
        parsed: SplitResult = urlsplit(destination.strip('<>'))
        if not parsed.scheme and parsed.path:
            targets.add((document.parent / unquote(parsed.path)).resolve())
    return targets


def table_rows(content: str) -> list[list[str]]:
    """Return Markdown table cells for the simple tables used by this project."""
    return [
        [cell.strip() for cell in line.strip('|').split('|')]
        for line in content.splitlines()
        if line.startswith('|')
    ]


def question_records() -> dict[str, tuple[str, str]]:
    """Read each question's status and explicit answer from the decision registry."""
    section: str = read_document('docs/decisions.md').split(
        '## 질문 기록과 남은 결정\n', 1
    )[1].split('\n해결된 질문', 1)[0]
    return {
        row[0]: (row[1], row[2])
        for row in table_rows(section)
        if len(row) == 6 and re.fullmatch(r'Q-\d+', row[0])
    }


class PlanningDocumentTests(unittest.TestCase):
    """Protect the integrated plan from broken references and inconsistent examples."""

    def test_local_links_exist(self) -> None:
        """Check current documents, preserved history, and image links together."""
        for document in DOCUMENTS:
            for target in local_targets(document):
                with self.subTest(document=document.name, target=str(target)):
                    self.assertTrue(target.is_file(), str(target))

    def test_all_documents_are_reachable_from_readme(self) -> None:
        """Require a navigation path from the project entry point to every document."""
        pending: list[Path] = [PROJECT / 'README.md']
        visited: set[Path] = set()
        while pending:
            document: Path = pending.pop()
            if document in visited:
                continue
            visited.add(document)
            pending.extend(
                target for target in local_targets(document)
                if target.suffix == '.md' and target.is_file() and target not in visited
            )
        self.assertEqual(set(DOCUMENTS) - visited, set())

    def test_decision_definitions_and_current_references(self) -> None:
        """Keep decision IDs unique and current-plan references tied to defined records."""
        registry: str = read_document('docs/decisions.md').split(
            '## 구체 규칙 제안의 검토 상태', 1
        )[0]
        identifiers: list[str] = re.findall(r'^\| ([DQ]-\d+) \|', registry, re.MULTILINE)
        self.assertTrue(identifiers)
        self.assertEqual(len(identifiers), len(set(identifiers)))
        defined: set[str] = set(identifiers)
        for name in (
            'docs/game-design.md', 'docs/prototype-plan.md',
            'docs/preproduction-plan.md', 'docs/rule-options.md',
            'docs/architecture.md', 'docs/interface-plan.md', 'docs/first-playable-spec.md',
        ):
            referenced: set[str] = set(re.findall(r'\b[DQ]-\d+\b', read_document(name)))
            with self.subTest(document=name):
                self.assertEqual(referenced - defined, set())

    def test_integrated_status_matches_decision_registry(self) -> None:
        """Prevent research proposals from being presented as confirmed decisions."""
        statuses: dict[str, str] = {
            row[0]: row[1]
            for row in table_rows(read_document('docs/decisions.md'))
            if len(row) >= 2 and re.fullmatch(r'D-\d+', row[0])
        }
        checked: int = 0
        for row in table_rows(read_document('docs/game-design.md')):
            if len(row) != 3 or row[0] not in {'확정 방향', '검토 방향', '실험안'}:
                continue
            for identifier in re.findall(r'D-\d+', row[2]):
                with self.subTest(decision=identifier):
                    self.assertEqual(row[0], statuses[identifier])
                checked += 1
        self.assertGreater(checked, 0)

    def test_prototype_deck_has_eight_cards_and_three_actions(self) -> None:
        """Check deck counts, card categories, and the distinction from acquired content."""
        content: str = read_document('docs/prototype-plan.md')
        section: str = content.split('## 카드 덱과 에너지', 1)[1].split('## 유닛과 건물', 1)[0]
        cards: list[list[str]] = [
            row for row in table_rows(section)
            if len(row) == 4 and row[1].isdigit()
        ]
        self.assertEqual(sum(int(row[1]) for row in cards), 8)
        self.assertEqual(len({row[0] for row in cards}), len(cards))
        self.assertEqual(len(cards), 8)
        self.assertEqual({row[2] for row in cards if int(row[1]) > 0}, {'건물', '유닛', '마법'})
        self.assertTrue(any(int(row[1]) == 0 for row in cards))

    def test_starting_budget_examples_are_consistent(self) -> None:
        """Verify the card-system examples obey their declared count and point budget."""
        examples: list[tuple[str, str, str]] = re.findall(
            r'^\| (일반[^|]+) \| (\d+) \| (\d+) \|',
            read_document('docs/card-system.md'), re.MULTILINE,
        )
        points: dict[str, int] = {'일반': 1, '희귀': 2, '영웅': 3}
        self.assertTrue(examples)
        for expression, stated_count, stated_points in examples:
            components: list[tuple[str, str]] = re.findall(r'(일반|희귀|영웅)\s*(\d+)장', expression)
            count: int = sum(int(quantity) for _, quantity in components)
            score: int = sum(points[grade] * int(quantity) for grade, quantity in components)
            with self.subTest(example=expression):
                self.assertEqual(count, int(stated_count))
                self.assertEqual(count, 8)
                self.assertEqual(score, int(stated_points))
                self.assertLessEqual(score, 10)

    def test_current_prototype_is_distinct_from_preserved_history(self) -> None:
        """Keep the original numeric proposal available without labeling it current."""
        current: str = read_document('docs/prototype-plan.md')
        archived: str = read_document('docs/archive/prototype-plan-v0.1.md')
        self.assertTrue(current.startswith('# 프로토타입 상세안 v0.2\n'))
        self.assertTrue(archived.startswith('# 프로토타입 상세안 v0.1 보관본\n'))
        self.assertIn('archive/prototype-plan-v0.1.md', current)
        self.assertIn('../prototype-plan.md', archived)

    def test_rule_options_cover_every_recorded_question_once(self) -> None:
        """Keep pending questions and answered alternatives discoverable without duplicates."""
        recorded_questions: set[str] = set(question_records())
        option_ids: list[str] = re.findall(
            r'^## (Q-\d+) — ', read_document('docs/rule-options.md'), re.MULTILINE
        )
        self.assertTrue(recorded_questions)
        self.assertEqual(len(option_ids), len(set(option_ids)))
        self.assertEqual(set(option_ids), recorded_questions)

    def test_question_choices_and_status_match_registry(self) -> None:
        """Prevent the questionnaire from contradicting recorded selections or pending scope."""
        records: dict[str, tuple[str, str]] = question_records()
        blocks: list[str] = re.split(
            r'^## (Q-\d+) — [^\n]+\n', read_document('docs/rule-options.md'),
            flags=re.MULTILINE,
        )
        for index in range(1, len(blocks), 2):
            identifier: str = blocks[index]
            content: str = blocks[index + 1]
            choice: re.Match[str] | None = re.search(r'^- 선택: (.+)$', content, re.MULTILINE)
            status: re.Match[str] | None = re.search(r'^- 상태: (.+)$', content, re.MULTILINE)
            with self.subTest(question=identifier):
                self.assertIsNotNone(choice)
                self.assertIsNotNone(status)
                if choice is not None and status is not None:
                    self.assertEqual((status.group(1), choice.group(1)), records[identifier])

    def test_latest_explicit_answers_are_preserved_without_extra_confirmation(self) -> None:
        """Preserve explicit answers without promoting unresolved details to requirements."""
        records: dict[str, tuple[str, str]] = question_records()
        expected: dict[str, tuple[str, str]] = {
            'Q-01': ('확정', 'A'),
            'Q-04': ('확정', 'A'),
            'Q-06': ('일부 확정', 'A'),
            'Q-08': ('확정', 'A'),
            'Q-09': ('일부 확정', 'A'),
            'Q-10': ('확정', 'A'),
            'Q-22': ('확정', 'A'),
            'Q-03': ('일부 확정', 'A'),
            'Q-19': ('일부 확정', 'B'),
            'Q-07': ('일부 확정', '직접입력'),
            'Q-14': ('일부 확정', 'A'),
            'Q-16': ('확정', 'A'),
            'Q-18': ('확정', 'A'),
            'Q-20': ('일부 확정', 'A'),
            'Q-23': ('확정', 'A'),
        }
        for identifier, record in expected.items():
            with self.subTest(question=identifier):
                self.assertEqual(records[identifier], record)
        decisions: dict[str, str] = {
            row[0]: row[1]
            for row in table_rows(read_document('docs/decisions.md'))
            if len(row) >= 2 and re.fullmatch(r'D-\d+', row[0])
        }
        for identifier in (
            'D-09', 'D-10', 'D-13', 'D-26', 'D-31', 'D-32', 'D-33', 'D-34', 'D-35', 'D-36', 'D-37', 'D-38', 'D-39',
            'D-40', 'D-41', 'D-42', 'D-43', 'D-44', 'D-45',
            'D-46', 'D-47', 'D-48', 'D-49',
            'D-50', 'D-51', 'D-52',
            'D-53', 'D-54', 'D-55',
            'D-56', 'D-57', 'D-58',
        ):
            self.assertEqual(decisions[identifier], '확정 방향')
        self.assertEqual(decisions['D-08'], '검토 방향')
        self.assertEqual(decisions['D-25'], '실험안')
        self.assertEqual(decisions['D-27'], '실험안')
        self.assertEqual(decisions['D-59'], '실험안')

    def test_playable_recommendation_pack_covers_ten_sections(self) -> None:
        """Keep a reviewable complete proposal separate from the user's confirmed answers."""
        content: str = read_document('docs/first-playable-spec.md')
        identifiers: list[str] = re.findall(r'^## (P-\d{2}) — ', content, re.MULTILINE)
        self.assertEqual(identifiers, [f'P-{number:02d}' for number in range(1, 11)])
        options: str = read_document('docs/rule-options.md').split(
            '## 남은 운영 규칙의 추천 묶음', 1
        )[1].split('## Q-22 —', 1)[0]
        self.assertEqual(set(re.findall(r'P-\d{2}', options)), set(identifiers))
        self.assertIn('검토 대기 실험안', content)

    def test_fixed_world_rules_keep_free_placement_as_a_later_transition(self) -> None:
        """Keep the selected fixed rules separate from the requested future migration."""
        section: str = read_document('docs/rule-options.md').split(
            '## Q-04 —', 1
        )[1].split('## Q-06 —', 1)[0]
        answers: dict[str, tuple[str, str]] = {
            row[0]: (row[1], row[2])
            for row in table_rows(section)
            if len(row) == 3 and row[0] in {'첫 검증', '후속 전환'}
        }
        self.assertEqual(answers['첫 검증'], (
            'A: 고정 경로·배치 칸 + 지하 4방향 이동', '확정',
        ))
        self.assertEqual(answers['후속 전환'], (
            'B: 자유 배치·경로 재계산 + 지하 중력·사다리',
            '전환 가능성 설계, 실행 기능은 후속',
        ))
        self.assertEqual(question_records()['Q-04'], ('확정', 'A'))

    def test_mining_assistance_preserves_selected_scan_and_finish_rules(self) -> None:
        """Keep scan disclosure separate from excavation and preserve idle-only finish."""
        section: str = read_document('docs/rule-options.md').split(
            '## Q-09 —', 1
        )[1].split('## Q-11 —', 1)[0]
        answers: dict[str, tuple[str, str]] = {
            row[0]: (row[1], row[2])
            for row in table_rows(section)
            if len(row) == 3 and row[0] in {'시작', '스캔', '종료'}
        }
        self.assertEqual(answers, {
            '시작': ('안전한 시작 구역', '확정, 크기·초기 공개 미정'),
            '스캔': ('정비당 무료 1회·접근 가능한 경계 1칸의 광물·둥지·숫자 공개·굴착 횟수와 자원 불변', '확정'),
            '종료': ('현재 이동·굴착 완료 뒤 남은 굴착을 포기하고 정비 조기 종료', '확정'),
        })
        self.assertEqual(question_records()['Q-09'], ('일부 확정', 'A'))
        followups: dict[str, str] = {
            row[0]: row[3] for row in table_rows(section)
            if len(row) == 4 and row[0] in {'굴착 후 숫자 0', '둥지 실제 굴착'}
        }
        self.assertEqual(followups, {
            '굴착 후 숫자 0': '추천안 확정',
            '둥지 실제 굴착': '추천안 확정',
        })

    def test_mobile_ux_scope_is_separate_from_unmeasured_performance(self) -> None:
        """Keep mobile UI preparation visible without claiming a measured mobile baseline."""
        interface_section: str = read_document('docs/interface-plan.md').split(
            '## 확정 범위와 제안의 경계', 1
        )[1].split('## 단계별 화면 구성', 1)[0]
        scope: dict[str, tuple[str, str]] = {
            row[0]: (row[1], row[2])
            for row in table_rows(interface_section)
            if len(row) == 3
        }
        self.assertEqual(scope['데스크톱 성능'][0], '방향 확정')
        self.assertIn('60 FPS', scope['데스크톱 성능'][1])
        self.assertEqual(scope['모바일 화면·입력'][0], '준비 범위 확정')
        self.assertEqual(scope['모바일 성능'][0], '기준 미정')
        self.assertEqual(scope['모바일 화면 방향'][0], '제안')
        environment_section: str = read_document('docs/validation-plan.md').split(
            '## 데스크톱·모바일 검증 환경', 1
        )[1].split('## 성능 실험', 1)[0]
        environments: dict[str, str] = {
            row[0]: row[2]
            for row in table_rows(environment_section)
            if len(row) == 4 and row[0] in {'데스크톱 성능', '모바일 UI·UX', '모바일 성능'}
        }
        self.assertEqual(environments, {
            '데스크톱 성능': '미정',
            '모바일 UI·UX': '미정',
            '모바일 성능': '미정',
        })

    def test_unit_followup_answers_preserve_capacity_and_survivors(self) -> None:
        """Protect the six-slot growth and surviving-unit HP choices from stale retreat defaults."""
        section: str = read_document('docs/rule-options.md').split(
            '## Q-19 —', 1
        )[1].split('## Q-18 —', 1)[0]
        recorded: dict[str, tuple[str, str]] = {
            row[0]: (row[1], row[2])
            for row in table_rows(section)
            if len(row) == 3
        }
        expected: dict[str, tuple[str, str]] = {
            '기본 슬롯': ('유닛당 1슬롯·기본 6슬롯', '확정'),
            '슬롯 성장': ('재화 광물(금 가칭)을 써서 슬롯 상한 강화', '방향 확정, 비용·상한 미정'),
            '웨이브 종료': ('생존 유닛과 남은 체력 유지', '확정'),
            '이동·교전': ('지정 위치에서 자동 교전·저지', '확정'),
        }
        for item, answer in expected.items():
            with self.subTest(item=item):
                self.assertEqual(recorded[item], answer)

    def test_currency_and_care_answers_preserve_card_recruitment(self) -> None:
        """Record the chosen card offer alongside targeted treatment and free movement."""
        section: str = read_document('docs/rule-options.md').split(
            '## Q-19 —', 1
        )[1].split('## Q-18 —', 1)[0]
        recorded: dict[str, tuple[str, str]] = {
            row[0]: (row[1], row[2])
            for row in table_rows(section)
            if len(row) == 3
        }
        expected: dict[str, tuple[str, str]] = {
            '유닛 뽑기': ('금으로 유닛 카드 후보 3개 중 1개 획득', '확정, 가격·후보 풀·교체 예외 미정'),
            '선택 치료': ('정비 중 재료를 써 원하는 유닛 치료', '방향 확정, 재료·회복량 미정'),
            '정비 재배치': ('정비 중 무료 위치 변경, 현재 HP·점유 슬롯 유지', '확정'),
        }
        for item, answer in expected.items():
            with self.subTest(item=item):
                self.assertEqual(recorded[item], answer)
        self.assertEqual(question_records()['Q-23'], ('확정', 'A'))

    def test_prototype_has_a_unit_pool_for_three_card_choices(self) -> None:
        """Catch a prototype that still contains only one unit after three-choice recruitment."""
        section: str = read_document('docs/prototype-plan.md').split(
            '## 카드 덱과 에너지', 1
        )[1].split('## 유닛과 건물', 1)[0]
        unit_definitions: set[str] = {
            row[0]
            for row in table_rows(section)
            if len(row) == 4 and row[1].isdigit() and row[2] == '유닛'
        }
        self.assertGreaterEqual(len(unit_definitions), 3)
        self.assertEqual(question_records()['Q-23'], ('확정', 'A'))

    def test_deck_replacement_and_hand_rebuild_answers_are_preserved(self) -> None:
        """Preserve deck, hand, and recruitment payment choices while other offer rules remain open."""
        section: str = read_document('docs/rule-options.md').split(
            '## Q-20 —', 1
        )[1].split('## Q-14 —', 1)[0]
        answers: dict[str, str] = {
            row[0]: row[4]
            for row in table_rows(section)
            if len(row) == 5
        }
        self.assertEqual(answers['획득 카드 반영'], 'A 확정')
        self.assertEqual(answers['정비 후 손패'], 'A 확정')
        self.assertEqual(answers['모집 결제·취소'], 'A 확정')
        self.assertEqual(answers['모집 후보 풀'], 'A 확정')
        payment_row: list[str] = next(
            row for row in table_rows(section)
            if len(row) == 5 and row[0] == '모집 결제·취소'
        )
        self.assertEqual(payment_row[1], '후보 확인 무료·교체 확정 결제·취소 무소모·같은 후보 유지')
        self.assertEqual(question_records()['Q-18'], ('확정', 'A'))
        self.assertEqual(question_records()['Q-20'], ('일부 확정', 'A'))

    def test_each_rule_question_has_choices_recommendation_and_answer(self) -> None:
        """Catch missing alternatives or a recommendation that does not name an offered choice."""
        blocks: list[str] = re.split(
            r'^## (Q-\d+) — [^\n]+\n', read_document('docs/rule-options.md'),
            flags=re.MULTILINE,
        )
        self.assertGreater(len(blocks), 1)
        for index in range(1, len(blocks), 2):
            identifier: str = blocks[index]
            content: str = blocks[index + 1]
            choices: list[str] = re.findall(r'^\| ([ABC]) \|', content, re.MULTILINE)
            recommendation: re.Match[str] | None = re.search(
                r'^- 추천: ([ABC])\.', content, re.MULTILINE
            )
            answer: re.Match[str] | None = re.search(r'^- 선택: (.+)$', content, re.MULTILINE)
            with self.subTest(question=identifier):
                self.assertIn(len(choices), (2, 3))
                self.assertEqual(len(choices), len(set(choices)))
                self.assertIsNotNone(recommendation)
                if recommendation is not None:
                    self.assertIn(recommendation.group(1), choices)
                self.assertIsNotNone(answer)
                if answer is not None:
                    self.assertTrue(answer.group(1).strip())
                self.assertRegex(content, r'(?m)^- 검증: .+')

    def test_markdown_structure(self) -> None:
        """Check document encoding, top-level titles, and matching fenced code blocks."""
        for document in DOCUMENTS:
            content: str = document.read_text(encoding='utf-8')
            opened: str | None = None
            with self.subTest(document=str(document.relative_to(PROJECT))):
                self.assertNotIn('\x00', content)
                self.assertTrue(content.endswith('\n'))
                self.assertEqual(len(re.findall(r'^# ', content, re.MULTILINE)), 1)
                for line in content.splitlines():
                    match: re.Match[str] | None = re.match(r'^(`{3,}|~{3,})', line)
                    if match is None:
                        continue
                    fence: str = match.group(1)
                    if opened is None:
                        opened = fence
                    elif fence[0] == opened[0] and len(fence) >= len(opened):
                        opened = None
                self.assertIsNone(opened, 'Unclosed code fence')


if __name__ == '__main__':
    unittest.main(verbosity=2)
