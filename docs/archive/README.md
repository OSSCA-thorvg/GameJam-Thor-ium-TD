# 보관 문서

개발 전 기획·조사와 첫 구현 기록입니다. 2026-10-31~11-04 기준으로 멈춘 문서이고 지금 게임과 다른 수치·규칙·구조가 들어 있습니다. 현재 내용은 [문서 안내](../README.md)의 문서를 확인합니다. 아래 문서의 결정 기록·질문 번호·덱 예시는 `tests/test_planning_docs.py` 등이 서로 맞는지 계속 검사합니다.

## 기획

| 문서 | 내용 |
| --- | --- |
| [개발 전 시작 계획](preproduction-plan.md) | 조사 결과, 먼저 고를 규칙, 첫 범위와 구현 전 통과 조건 |
| [규칙 선택지·답변란](rule-options.md) | 규칙별 A/B/C·추천·영향·검증과 답변·상태 |
| [결정 및 미정 사항](decisions.md) | 확정 방향, 검토 아이디어, 미정 질문, 수치 가안 |
| [통합 기획안](game-design.md) | 조사 비교, 게임 정체성, 플레이 흐름, 시스템 연결 |
| [프로토타입 상세안 v0.2](prototype-plan.md) | 유닛·채굴·제작·카드 획득을 잇는 3웨이브 추천 범위 |
| [프로토타입 v0.1 보관본](prototype-plan-v0.1.md) | 단일 광석·고정 덱·룬·진화 수치 |
| [첫 3웨이브 플레이 사양 초안](first-playable-spec.md) | 시작·카드 순환·전투·부대 정비·경로·공세의 초깃값 |
| [카드 시스템 제안](card-system.md) | 마법·건물·유닛, 플레이 중 획득, 발견 도감 |
| [광물·금 재화·부대 정비 경제안](economy-plan.md) | 제작 광물과 재화 광물의 역할, 슬롯·모집·치료 |
| [첫 채굴 보드·경제 수치 초안](first-board-economy.md) | 5굴착×2정비 보드의 배치·가격·투자 경로 계산 |
| [데스크톱·모바일 UI·UX 계획](interface-plan.md) | 단계별 화면, 손패 4장, 터치·마우스 조작 |
| [검증 계획](validation-plan.md) | 규칙 테스트, 밸런스 실험, 렌더링 성능 검증 계획 |

## 구조·기술

| 문서 | 내용 |
| --- | --- |
| [기술 계획](technical-plan.md) | C++/WASM 후보 구조, ThorVG·Lottie 활용 |
| [코드 구조 제안](architecture.md) | 기능 모듈, 상태 소유권, 입력·시간·렌더링 경계 |
| [큰 구조·폴더 배치 비교](architecture-overview.md) | 엔진·게임·앱 경계 수정안과 선택한 폴더 구성 |
| [SDL·데스크톱·공통 UI 제안](cross-platform-ui-proposal.md) | SDL3·엔진 UI·게임 화면의 구조도와 입력→출력 흐름 |
| [Rubiks·엔진 패턴 조사](architecture-research.md) | 로컬 코드와 Unity·Unreal·Godot 문서, 채택·보류 근거 |

## 조사

| 문서 | 내용 |
| --- | --- |
| [채굴·제작·등급 조사](mining-and-crafting-research.md) | 채굴 게임 사례, 광물별 제작·도구 등급 제안 |
| [명일방주 참고 조사](arknights-reference.md) | 저지 수, 자원·방어 연결, 모집·편성 참고 |

## 첫 구현

| 문서 | 내용 |
| --- | --- |
| [첫 구현 v0.1](implementation-v0.1.md) | 2026-11-04 3웨이브 시연판의 조작·코드 경계·검증 |
