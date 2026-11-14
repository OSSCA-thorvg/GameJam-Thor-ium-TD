# 참고 자료

기획 대화에서 조사한 공식 게임 소개와 개발자·프로젝트 자료를 모았습니다. 명일방주 세부 규칙에는 커뮤니티 위키도 사용하며, 자료 성격은 해당 조사에 표시했습니다. 아래의 참고 포인트는 Thor-ium TD에 적용할지 판단하기 위한 해석이며, 해당 게임을 직접 플레이해 검증한 평가나 기능 전체를 구현하겠다는 약속은 아닙니다.

## 타워 디펜스와 성장

| 자료 | 확인한 구성 | 참고할 점 |
| --- | --- | --- |
| [Rogue Tower](https://store.steampowered.com/app/1843760/Rogue_Tower/) | 경로 확장, 랜덤 강화·해금 카드, 판 사이 경험치 | 맵 변화에 따른 배치 선택과 성장 |
| [Core Defense](https://store.steampowered.com/app/1203360/) | 웨이브 방어와 보상 선택의 반복 | 짧고 읽기 쉬운 핵심 루프 |
| [Towerful Defense](https://minifun.games/towerful-press-kit/) | 단일 타워, 최대 4개 스킬, 특성·아이템 조합 | 생존형 성장과 스킬 빌드; 경로형 TD와 구조 차이도 비교 |
| [Heretic’s Fork](https://store.steampowered.com/app/2181610/Heretics_Fork/) | 타워 디펜스와 카드 덱 구성 | 여러 방어 수단의 조합과 전투 연출 |
| [Emberward](https://store.steampowered.com/app/2459550/Emberward/) | 블록 미로, 타워, 룬·유물·성장 | 지형·배치와 강화 효과 연결 |
| [Tower Tactics: Liberation](https://store.steampowered.com/app/1709900/Tower_Tactics%3A_Liberation/?l=english) | 타워·주문 카드, 유물, 로그라이크 진행 | 카드 간 상호작용과 빌드 변화 |
| [Isle of Arrows](https://store.steampowered.com/app/1946970/Isle_of_Arrows/) | 랜덤 타일 배치와 보상; 필수 타일 수량 보장 | 랜덤성 속에서도 필요한 선택지를 제공하는 장치 |
| [Clash Royale의 코스트](https://support.clashroyale.com/hc/en-us/articles/49484936933403-Elixir) | 시간에 따라 생성되는 자원을 카드 배치에 소비 | 자원 보존과 즉시 대응 사이의 선택 |

## 채굴과 방어

[명일방주 참고 조사](archive/arknights-reference.md)에는 유닛의 저지 수, 생존연산의 자원 확보·거점 방어, 통합전략의 모집권과 편성 예산을 정리했습니다. 세부 적용안은 검토 단계입니다.

광물별 제작·도구 등급의 추가 사례와 적용 해석은 [채굴·제작 조사](archive/mining-and-crafting-research.md)에 정리했습니다. Minecraft·Terraria·Core Keeper·SteamWorld Dig 2·A Game About Digging A Hole·Deep Rock Galactic: Survivor와 Factorio의 게임 내 지원 공격 사례를 포함합니다.

| 자료 | 확인한 구성 | 참고할 점 |
| --- | --- | --- |
| [Dome Keeper](https://store.steampowered.com/app/1637320/Dome_Keeper/) | 공격 사이 채굴·귀환, 자원으로 장비·방어 강화 | 탐색 욕심과 방어 준비 사이의 시간 관리 |
| [Wall World](https://store.steampowered.com/app/2187290/Wall_World/) | 이동 거점, 광산 탐색, 자원·기술 발견과 전투 | 수직 이동과 광산 개척을 연결하는 방식 |
| [Drill Core](https://store.steampowered.com/app/2821800/Drill_Core/) | 채굴·운반·방어 인력과 포탑 운영 | 경제 효율과 방어 투자 배분 |

## 기술과 행사

2026-11-03의 실제 코드·공식 엔진 비교는 [Rubiks·엔진 조사](archive/architecture-research.md)에 출처와 관찰·해석을 구분해 기록했습니다. Unity·Unreal·Godot의 범용 개체·에디터 체계를 도입하는 근거로 사용하지 않고 상태·수명·시간·좌표의 원리를 비교합니다.

- [ThorVG](https://github.com/thorvg/thorvg): 도형·장면·Lottie와 렌더링 백엔드의 공식 프로젝트 설명.
- [ThorVG C++ API](https://github.com/thorvg/thorvg/blob/main/inc/thorvg.h): 프레임·구간 제어와 그래픽 객체 API.
- [ThorVG Lottie 예제](https://github.com/thorvg/thorvg.example/blob/main/src/Lottie.cpp): 여러 애니메이션의 프레임 제어와 한 캔버스 합성. 성능 보장 수치로 해석하지 않습니다.
- [ThorVG Web](https://github.com/thorvg/thorvg.web): WebCanvas와 WASM 구조.
- [ThorVG Web 튜토리얼](https://www.thorvg.org/web-tutorial): 장면·변환·합성·애니메이션. 과도한 합성의 추가 처리 비용도 설명합니다.
- [Emscripten Best Practices](https://emscripten.org/docs/compiling/best_practices.html): JS↔WASM 고빈도 호출을 줄이는 지침.
- [JSON Schema 객체 검증](https://json-schema.org/understanding-json-schema/reference/object): 후속 유저 콘텐츠의 필드·필수 값·미지원 필드 정책 검토. 검증기·스키마 버전은 미정입니다.
- [WebAssembly 보안 모델](https://webassembly.org/docs/security/): 모듈 격리와 호스트 API 경계. 사용자 스크립트 모드의 실행 정책은 별도 설계 사항입니다.
- [게임잼 안내](https://ossca-thorvg.github.io/site/project/#event): 일정·렌더링·스플래시·배포·제출 기준.
- [Thor-ium TD 등록 이슈](https://github.com/OSSCA-thorvg/site/issues/198): 공개 등록 상태와 소개.

행사와 등록 상태는 2026-10-31에 확인한 기록입니다. Rubiks·엔진 문서는 2026-11-03 조사했으며 행사·이슈의 재확인과 구분합니다. 기술 API는 실제 사용할 ThorVG 버전을 정한 뒤 해당 버전의 헤더·예제로 재확인합니다.
