# 현재 구현 아키텍처

2026-11-04 아키텍처 검토 후 승인된 네 단계(엔진 정책 분리 → 콘텐츠 정의 → 앱 흐름/입력 분리 → 규칙 내부 분리)를 반영했습니다. 이 문서는 **현재 코드**를 설명합니다. 이전 [구조 제안](architecture.md)과 [엔진 패턴 조사](architecture-research.md)는 당시의 비교·계획 기록입니다.

승인된 후속으로 [Lottie 애니메이션 계층](animation-system.md)을 추가했습니다. 엔진 플레이어·게임 상태머신·사망 연출 수명은 해당 문서가 최신 설명입니다.

반복 몹의 최신 렌더링 경계·기본 정책·실측은 [공유 프레임·물량 측정](shared-animation-cache.md)에 정리했습니다.

콘텐츠 분류 통일·명시적 애니메이션 연결 검증·광역 피해 배치·전체 출력 측정은 [아키텍처 검토 후속](architecture-follow-up.md)에 반영했습니다. 해당 문서의 측정 범위와 최신 검증 기록을 함께 읽습니다.

2026-11-06 [콘텐츠 검증·카드 오퍼·앱 접근 경계](content-offers-boundary.md)를 추가했습니다. 현재 생성 API와 모집·보상 소유권, 플랫폼의 접근 범위는 해당 문서가 최신입니다.

같은 날 [카드 배치 사전 판정](placement-preflight.md)을 추가했습니다. `Run::previewPlay`와 `play`가 내부 판정·가격 계산을 공유하고, 화면은 결과를 읽어 대상의 색·비용·불가 사유를 표시합니다. 실행은 현재 상태를 다시 판정합니다.

2026-11-08 [독립 코드 검토 후속 수정](cold-review-fixes.md)을 반영했습니다. 결과 화면의 재시작 허용 조건은 화면·실행에서 공유하고, 규칙 계층은 배치 칸 중심과 안전한 광부 위치 계산을 보장합니다.

같은 날 [카드 정보·획득 비교 UI](card-information.md)를 추가했습니다. 엔진 UI는 일반 검사 제스처만 관리하고, 카드 데이터·설명·모집·교체 화면은 게임 표현 계층이 담당합니다.

## 의존성과 소유권

화살표는 코드 의존 방향입니다. `Application`은 객체 수명을 조합하고, `Run`은 한 판의 상태와 명령을 소유합니다. 엔진과 규칙에서 화면·플랫폼을 역참조하지 않습니다.

~~~mermaid
flowchart TB
    SDL["플랫폼 · SDL3 / native / WASM"] --> App["Application · 수명 조합 / 고정 시간 / 출력"]
    App --> Flow["AppFlow · 메뉴 / 설정 / 새 원정"]
    App --> Input["GameplayController · 선택 / 배치 / 게임 명령"]
    App --> Feedback["FeedbackChannel · 확정 사실을 시각·사운드로 전달"]
    App --> View["View · 메인 / 전장 / 지하 / HUD"]
    Flow --> Run["Run · 한 판의 명령과 상태 소유자"]
    Input --> Run
    Feedback --> Run
    View --> Run
    Run --> State["private RunState · 외부에는 const 참조"]
    Run --> Content["Catalog · 카드 / 적 / 공세 / 맵 / 광물 / 비용"]
    Input --> Layout["Layout · 화면↔게임 좌표"]
    View --> Layout
    View --> UI["engine/ui · 히트 / 캡처 / 명시적 포커스"]
    View --> Render["engine/render · ThorVG / 텍스트 / Lottie"]
    Feedback --> Sound["Soundscape · 게임 효과음 의미 / MP3·비프음 바인딩"]
    Sound --> Audio["engine/audio · PCM / 믹싱 / 볼륨 / 음성 수 제한"]
    SDL --> AudioDevice["SDL AudioDevice · 장치 / 스트림"]
    AudioDevice --> Audio
    classDef core fill:#142d38,stroke:#58c4dd,color:#fff;
    classDef game fill:#23372f,stroke:#83c167,color:#fff;
    classDef engine fill:#352e3d,stroke:#c895e8,color:#fff;
    class App,Flow,Input,Feedback core;
    class Run,State,Content,View,Layout,Sound game;
    class UI,Render,Audio engine;
~~~

`AppFlow`와 `GameplayController`는 구체적인 `Run`·UI 상태·오디오 소비자를 참조하지만 렌더러를 만들거나 `View`를 호출하지 않습니다. 이 경계는 SDL·ThorVG를 링크하지 않는 테스트 실행 파일로 검증합니다. 모든 서비스를 인터페이스로 감싸거나 전역 서비스 조회기를 만들지는 않았습니다.

## 파일 배치

~~~text
src/
  platform/sdl/          # 창, 이벤트 정규화, VideoOutput, AudioDevice
  app/
    Application.*       # 조합, 고정 60Hz 갱신, 플랫폼 진입점, 읽기 전용 진단
    AppFlow.*           # 화면 이동, 설정, 새 판 확인, 시뮬레이션 허용 여부
    GameplayController.* # 카드/개체 선택, 대상 좌표, 규칙 명령, 단계 전환 후 정리
    GameInput.h         # widget ID → typed control/hand/deck/candidate 경계
    FeedbackChannel.*   # 전투 사실 1회 drain → 시각/사운드 양쪽 소비
  engine/
    diagnostics/        # FrameProfiler · 고정 크기 측정 이력과 구간별 통계
    animation/          # 클립·원본 메타데이터·독립 재생/완료 관리
    ui/                 # 게임 ID를 모르는 포인터·키보드 UI
    render/             # 호출자가 지정한 파일·에셋 키·인스턴스별 ThorVG 렌더링
    audio/              # 플랫폼·게임 미의존 PCM 합성/MP3/믹싱
  game/
    model/
      Types.h           # ID, 좌표, enum, 개체/카드 값 타입, 전투 사실
      RunState.h        # 활성 CardOffer를 포함한 live snapshot; Run이 private 멤버로 소유
      PlayPreview.h     # 행동·실패 코드·대상별 실제 가격·손패 사용 가능 요약
    content/
      CatalogValidation.* # 필드별 수치·경로·참조 검증과 현재 지원 범위
      Catalog.*         # typed 초기 데이터와 밸런스, 공세 스케줄, 비용 표시
    rules/
      Run.*             # 생성, 상태 읽기, 시간 분기, 사실 큐, 진단
      Combat.cpp        # 이동, 저지, 동시 공격, 철수·재배치
      Cards.cpp         # 설치/소환/마법, 순환, 모집, 교체, 손패 재구성
      Mining.cpp        # 접근, 자동 이동·굴착, 스캔, 0 정보 공개
      Progression.cpp   # 웨이브·승패, 정비, 제작/치료/비용, 확장/보상
      RuleGeometry.h    # 규칙 내부 좌표·인접 칸 계산
    presentation/
      State.h           # 세션 바인딩 + 판의 개체/연출 수명; typed Modal
      AnimationBindings.* # Catalog 프로필→원본→필수 클립의 원자적 검증
      ActorAnimator.*   # 개체 상태머신·시각용 사망 정보
      CardPresentation.h # 실제 CardCategory의 분류 문구·조작 설명
      CardDetails.*, CardUi.cpp # 공통 기본 성능·역할·호버/길게 누르기 설명·모집/교체 비교
      CommandPolicy.h   # 결과 화면/확인창과 실행이 공유하는 새 원정 허용 조건
      PlacementPreview.h, Placement.cpp # 한 프레임의 대상 판정·표시 문맥과 안내
      Shell.h           # 화면 탐색·뒤로 이력·설정 값
      Actions.h         # 안정적인 게임 UI control ID
      Layout.*          # 그리기와 입력이 공유하는 좌표 변환
      Assets.h          # 게임 폰트와 Lottie 파일 바인딩
      View.*, GameUi.cpp, FrontEnd.cpp, Theme.h
      Feedback.*, CombatFeedback.cpp, Soundscape.*
~~~

규칙 파일들은 같은 `Run`의 구현입니다. 파일마다 별도의 상태나 매니저를 만들지 않았습니다. 전투·광물·덱을 수정하는 명령은 하나의 소유자 안에서 검증 후 커밋합니다.

## 이번에 바뀐 경계

| 검토에서 발견한 결합 | 현재 구현 |
| --- | --- |
| Renderer가 Noto·oreling 파일명을 직접 결정 | [Assets.h](../src/game/presentation/Assets.h)가 파일을 바인딩하고 [Renderer](../src/engine/render/Renderer.h)는 `RenderAssets`만 받음 |
| Lottie 인스턴스 ID 하나로 에셋과 개체를 함께 구분 | `(asset key, instance ID)`로 캐시하여 서로 다른 몬스터 에셋이 같은 인스턴스 번호를 사용해도 충돌하지 않음 |
| UI가 숫자 ID 1을 특별 취급 | `Widget.focusable`로 명시하고 월드 위젯 등록 시 게임이 비활성화 |
| 비용·능력치·초기 보드·공세가 규칙 구현에 흩어짐 | [Catalog](../src/game/content/Catalog.h)에 카드/적 능력치·에너지·레시피·보드·경로·확장·공세를 모음; UI 비용도 같은 정의를 읽음 |
| 외부에서 Run 필드를 직접 수정 가능 | `Run::state()`·`Run::catalog()`는 const 참조만 반환; 생성 시 콘텐츠를 값으로 복사하여 외부 변경과 분리 |
| 메뉴·설정·게임 명령을 한 switch가 처리 | AppFlow와 GameplayController로 분리; GameInput에서 정수 widget 범위를 한 번 해석 |
| 새 판에서 설정을 임시 복사 후 복구 | `AppState`는 세션 동안 유지하고 `PlayState`만 교체; audio settings도 믹서 수명에 유지 |
| modal 1/2/3/4가 여러 파일에 분산 | `Modal::{None, Recruit, Replacement, Withdrawal, NewRun}`; 진단 JSON 숫자는 기존 값 유지 |
| Application이 규칙 사실을 직접 처리 | [FeedbackChannel](../src/app/FeedbackChannel.h)이 한 번 꺼내 두 소비자에게 전달 |

실행 중 설정과 새 판 초기화는 다른 수명입니다. 정비·확인 창·메뉴 이동은 현재 판을 교체하지 않습니다. 카드를 순환해도 타워·유닛은 독립된 entity ID로 남습니다. 새 판에서는 선택·입력 캡처·이펙트·과거 사운드 예약을 정리합니다.

입력 등록이 바뀌는 동안 도착한 오래된 명령도 확인합니다. 손패에 없는 카드, 일시정지 뒤의 월드 입력, 확인 창 없는 철수/초기화, 유닛이 아닌 모집 후보는 컨트롤러에서 거릅니다. 규칙 명령은 자원·단계·대상을 다시 검증합니다.

## 일반 게임 엔진에서 가져온 기준

| 일반 엔진에서 보이는 역할 | 이 게임에서의 대응 |
| --- | --- |
| Unity ScriptableObject·Unreal Data Asset·Godot Resource 같은 정의 데이터 | Catalog의 읽기 전용 콘텐츠 스냅샷; 현재는 C++ typed 데이터로 작성 |
| 앱/세션 수명과 월드·판 수명의 구분 | AppState와 Run/PlayState의 초기화 범위 분리 |
| 화면 전환, 게임플레이 조작, 개체 규칙의 구분 | AppFlow, GameplayController, Run의 명령과 기능별 구현 |
| 렌더·입력·오디오 플랫폼 어댑터 | ThorVG Renderer, generic Ui/AudioMixer, SDL 진입점·AudioDevice |

범용 엔진은 에디터, 임의 장면·개체, 직렬화·리플렉션, 플러그인과 여러 게임을 지원할 이유가 있습니다. 여기서는 그 요구에 필요한 상속 계층·범용 씬 그래프·컴포넌트 스케줄러까지 복제하지 않았습니다. 현재는 한 판의 고정 갱신과 데이터 중심 개체 목록이 책임·수명·테스트 경계를 드러냅니다.

## 검증과 확장 범위

아키텍처 리팩터링 당시 macOS 네이티브·WASM 빌드, 네이티브 테스트 타깃 5종(규칙/입력 171·컨트롤러/콘텐츠 69·오디오 51·에셋 9개 검사와 렌더링 smoke), Python 문서/의존성 검사 33개, 브라우저 시나리오 5개가 통과했습니다. 에셋 검사는 호출 성공 외에 서로 다른 영역의 실제 출력 픽셀과 이전 인스턴스 제거도 확인합니다.

- [rules_test.cpp](../tests/native/rules_test.cpp): 기존 전투·카드·채굴·경제 계약과 명시적 키보드 포커스.
- [controllers_test.cpp](../tests/native/controllers_test.cpp): 그래픽 없이 새 판/계속하기·설정 유지·확인 취소·정지 게이트·선택/배치·콘텐츠 변경·가격의 일관성.
- [render_assets_test.cpp](../tests/native/render_assets_test.cpp): 실제 ThorVG에서 호출자 에셋 키·동일 번호의 다른 에셋·미사용 인스턴스 정리·누락 진단.
- [audio_test.cpp](../tests/native/audio_test.cpp): PCM·MP3·효과음 연결의 기존 계약.
- [test_source_boundaries.py](../tests/test_source_boundaries.py): 엔진→게임, 규칙→표현/플랫폼, 컨트롤러→렌더러 역의존 방지.
- 기존 브라우저 5개 시나리오: 전투/채굴, 철수, 카드/타격 피드백, 실제 오디오, 메인/설정/모바일 배치.

기존 규칙 테스트의 임의 시나리오는 테스트 전용 [Scenario.h](../tests/native/Scenario.h)에서 만듭니다. `RunTestAccess` friend는 fixture 준비에만 사용하며 앱에 mutable state 접근 API나 상태 쓰기용 웹 export를 추가하지 않았습니다. 이것은 저장 복원 API가 아닙니다.

외부 JSON/콘텐츠 팩 로더·스키마 검증·안정적인 문자열 ID 레지스트리는 후속입니다. 현재 카드/적 enum과 8장/4손패·5×6 보드 크기는 첫 시연판의 고정 범위이며, 새로운 공격 행동은 규칙 코드가 필요합니다. Catalog 추출이 유저 모드나 임의 행동 스크립트를 지원한다는 의미는 아닙니다. 저장/불러오기와 설정의 디스크 저장도 아직 없습니다.

다수 적 성능은 별도 측정이 필요합니다. 현재 선형 탐색과 메인 스레드 오디오 큐를 유지했습니다. 100/300/1000개 적의 갱신·렌더링 시간과 오디오 공급을 측정한 뒤 공간 인덱스·오디오 콜백 등의 필요를 판단합니다. Windows 실행·모바일 실제 기기 성능·설치 패키지는 이번 구조 정리의 검증 범위에 포함되지 않습니다.
