# 콘텐츠 검증·카드 오퍼·앱 접근 경계

2026-11-06 승인된 추가 구조 개선 중 1·2번과 함께 진행한 3번을 구현했습니다. 콘텐츠 정의 ID 레지스트리는 후속입니다. 원정 저장 스냅샷은 [RunSnapshot.cpp](../src/game/rules/RunSnapshot.cpp)의 `Run::snapshot`·`restore`가 담당합니다([원정 저장](expedition-save.md)). [전체 구조](current-architecture.md)와 [이전 아키텍처·성능 정리](architecture-follow-up.md)를 잇는 현재 구현 기록입니다.

## 생성 전에 콘텐츠 검증

[CatalogValidation](../src/game/content/CatalogValidation.h)은 필드 경로와 사유를 가진 `CatalogIssue` 목록을 반환합니다. 독립적인 오류를 모아 `cards[0].cost`, `expansions[0].route[7]`, `balance.recruit.gold`처럼 어느 정의를 수정해야 하는지 표시합니다. 오류가 있으면 [Run::create](../src/game/rules/Run.h)는 `run == nullptr`을 반환합니다. 부분 초기화된 판이나 기본 데이터로 바꾼 판을 전달하지 않습니다.

~~~cpp
game::RunCreation candidate = game::Run::create(authoredCatalog);
if (candidate.run) {
    Application app(std::move(*candidate.run));
    // 검증된 판으로 실행을 시작합니다.
} else {
    const std::string error = candidate.validation.summary();
    // 오류를 표시하고 생성 작업을 종료합니다.
}
~~~

실제 [SDL 진입점](../src/platform/sdl/main.cpp)은 내장 기본 데이터도 이 생성 경계를 거칩니다. 편의 생성자 `Run()`은 컴파일된 기본 정의만 사용하고 검사합니다. 임의의 `Catalog`로 `Run`·`Application`을 직접 생성할 수 없습니다. 유효한 판을 초기화할 때는 `Run::reset()`이 이미 검증한 읽기 전용 카탈로그를 보존합니다. 예외 처리 기능을 추가하지 않았습니다.

검증하는 항목은 다음과 같습니다.

- 카드·적의 이름/프로필, enum 범위, 체력·피해·비용·슬롯·시간·배율의 범위와 유한성.
- 시작 덱·스폰 적·모집 후보·분기 보상 참조. 모집 후보는 서로 다른 유닛 3종.
- 길이가 있는 직교 경로와 확장 접합부, 전장 내 정수 좌표, 기본/확장 배치 칸의 중복.
- 음수 재료 가격, 잘못된 시작 에너지, 최대 체력을 줄이는 경비병 장갑 정의.
- 광부 시작 칸 2의 개방, 초기 열린 갱도의 연결, 광물/양/도구 등급, 열린 시작 칸의 둥지.
- 스폰 그룹과 공세의 누적 예산 및 유한한 스케줄. 빈 공세·0초 스폰 간격·정지 적·무료 비용은 허용.

[CatalogLimits](../src/game/content/CatalogValidation.h)는 현재 고정 격자 시연판이 지원하는 범위입니다. 플레이 시점의 밸런스 추천 수치와는 별도로 관리합니다.

| 항목 | 지원 범위 |
| --- | --- |
| 전장 | 18×10 정수 타일, 0이 아닌 직교 경로 |
| 공세 수 | 1~100, 각 공세는 비어 있어도 됨 |
| 기본 공세 적 수 | 그룹 합계 최대 10,000, 둥지 추가분은 별도 |
| 체력·자원·일반 비용 | 최대 1,000,000, 항목에 따라 0 또는 1부터 |
| 기본 피해·적 공격·방어·누수량 | 최대 10,000 |
| 유닛 슬롯 | 1~180, 강화 목표는 시작 슬롯 이상 |
| 둥지당 추가 적 | 0~100 |
| 정비 굴착·스캔 | 0~30, 도구 등급은 T1/T2 |
| 시간·스폰 종료 시각 | 유한한 값, 최대 1,000,000초; 실제 작업·공격 간격은 양수 |
| 배율 | 개별 레벨 보너스 0~10, 코일 1~10, 둔화 비율 0~1 |

세부 공간 범위·연쇄 수·배율은 [검증 구현](../src/game/content/CatalogValidation.cpp)에 있습니다. 현재의 제한된 종류·지도·UI 식별자 범위를 벗어나는 콘텐츠는 후속 레지스트리/배치 규칙 작업 때 함께 확장합니다. 숫자 검증은 외부 JSON/팩 로더나 실제 난이도·성능 검증을 대신하지 않습니다. 애니메이션 파일·클립 참조는 기존 `AnimationBindings` 계층이 이어서 검증합니다.

## 모집과 확장 보상을 Run이 소유

[CardOffer](../src/game/model/Types.h)는 한 판 안의 식별자·출처·후보·가격을 보유합니다. `RunState.recruitmentOffer`와 `RunState.rewardOffer`가 실제 활성 상태이며, 별도의 `recruited`·`rewardPending` 불리언을 유지하지 않습니다. 가격은 함께 쓰는 값 타입 `ResourceCost`로 옮겼습니다.

~~~mermaid
flowchart LR
  Catalog["Catalog · 모집 3종 / 분기별 보상 / 가격"] --> Run["Run · 정비/확장 전환"]
  Run --> Offer["CardOffer · id / 후보 / 가격"]
  Offer --> UI["UI · 표시와 임시 선택"]
  UI --> Commit["acceptOffer · 오퍼 / 후보 / 교체 카드"]
  Commit --> Validate["활성 오퍼·멤버십·대상·모든 재료 확인"]
  Validate --> Atomic["카드 교체 + 결제 + 오퍼 소모"]
~~~

- 정비 진입 때 카탈로그의 모집 후보·가격을 복사하여 오퍼를 한 번 만듭니다. UI를 닫거나 설정·메인 화면을 방문해도 다시 생성하지 않습니다.
- 모집 후보는 `Catalog.recruitmentCandidates`, 확장 보상은 선택한 `ExpansionDefinition.reward`에서 읽습니다. 기본 모집 3종과 기본 전격탑 보상은 유지합니다.
- UI의 `selectedOffer`와 선택한 후보는 임시 상태입니다. 취소하면 이 선택만 지우고 판의 오퍼·자원은 보존합니다.
- `acceptOffer(id, candidate, replace)`는 실제로 제시한 후보인지를 검사합니다. 다른 유닛 종류라도 후보에 없으면 거절합니다. 활성 덱 카드와 모든 재료를 확인한 뒤 한 번에 교체·차감·소모합니다.
- 실패·중복 확정·이전 정비의 오퍼 ID는 자원·덱을 바꾸지 않습니다. 다음 웨이브 시작 때 미사용 모집은 만료됩니다.
- 확장 보상을 선택하거나 `skipOffer(id)`로 넘기기 전에는 모집과 다음 웨이브로 진행하지 않습니다. 보상을 넘겨도 같은 정비의 모집 오퍼는 유지합니다. 모집 대화상자 취소는 `skipOffer`를 호출하지 않습니다.
- 오퍼 ID는 한 판 안의 식별자입니다. 새 판은 선택·입력 캡처·오퍼를 함께 초기화합니다.

활성 덱 8장·손패 4장과 최근 획득 카드의 다음 웨이브 우선 포함 규칙은 유지합니다. 현재 후보 목록은 결정적 수작업 데이터이며 무작위 뽑기 로직은 추가하지 않았습니다.

읽기 전용 진단의 `offers.recruitment`와 `offers.reward`는 활성 오퍼 또는 `null`을 반환합니다. 후보·가격만 공개하고 상태를 쓰는 웹 API는 추가하지 않았습니다.

## Application의 소유권과 출력 API

[Application](../src/app/Application.h)의 Run·UI·표현 상태·View·Renderer·AudioMixer·Soundscape를 비공개로 옮겼습니다. 외부 코드는 다음 기능을 사용합니다.

| API | 권한 |
| --- | --- |
| `frame`, `pointer`, `act`, `key`, `suspend` | 기존 앱 흐름과 입력 경계를 통해 실행 |
| `game()` | const Run의 상태·정의·개체 조회, 규칙 명령 호출 불가 |
| `renderOutput()` | 완료된 프레임버퍼를 const 참조로 조회 |
| `audioOutput()` | PCM 소비와 generation/mute 조회만 허용 |
| `viewportChanged()` | 크기 변경 후 입력 캡처 취소 |
| `widgets`, `debugState`, `debugUi`, `frameTimings` | 읽기 전용 기하·상태·계측 |
| `error`, `audioAssetErrors` | 초기화·출력 실패와 오디오 대체 진단 |

[AudioOutput](../src/engine/audio/Audio.h)은 믹서를 참조하는 작은 동기 출력 핸들입니다. 설정 변경·클립 등록·소리 예약을 노출하지 않습니다. SDL `AudioDevice`는 이 핸들을 사용해 PCM을 복사하며, 포커스 손실 때는 앱이 음성을 취소하고 장치가 복사된 큐를 비웁니다. 핸들은 믹서 소유자 수명 안에서 메인 스레드의 동기 호출에만 사용합니다. 새 가상 서비스 인터페이스나 전역 매니저를 추가하지 않았습니다.

임의 전장을 준비하는 [ApplicationScenario.h](../tests/native/ApplicationScenario.h)와 기존 [Scenario.h](../tests/native/Scenario.h)는 테스트·네이티브 벤치마크에서만 사용합니다. 프로덕션 코드는 이 헤더들을 포함하지 않습니다. 두 벡터/캐시 전체 출력 벤치마크 경로도 변경된 API로 실행 확인했습니다. 이번 작업에서 전체 성능 수치를 재측정하지 않았으므로 이전 벤치마크 결과는 해당 문서의 기록 시점 자료입니다.

## 검증

- [catalog_validation_test.cpp](../tests/native/catalog_validation_test.cpp): 음수 비용/재료, NaN/무한대, 잘못된 참조, 빈/중복 경로, 확장 접합, 스폰, 광산 연결과 유효한 0 값. 실패 때 판이 생성되지 않음.
- [card_offers_test.cpp](../tests/native/card_offers_test.cpp): 후보 외 선택·재료 부족·잘못된 교체·중복 결제·이전 ID·보상/모집 우선순위·만료·다음 손패.
- [controllers_test.cpp](../tests/native/controllers_test.cpp): 실제 오퍼에 따른 후보 선택, 취소·재개, 결제.
- [application_test.cpp](../tests/native/application_test.cpp): 현재 오퍼에 따른 실제 후보 위젯, 비공개 상태와 const 출력 계약, 기존 ThorVG 픽셀 검사.
- [audio_test.cpp](../tests/native/audio_test.cpp): 출력 핸들의 PCM과 stop/mute 세대 전달.
- [브라우저 실제 입력](../tests/browser/playable.js): 취소 전후 같은 오퍼·가격·후보 유지, 확정 후 소모와 기존 전투/채굴/모바일 흐름.

네이티브 11개 타깃과 브라우저 8개 시나리오가 통과했습니다. 출력 권한의 컴파일 시점 검사를 보강한 앱 통합 타깃도 개별 재실행하여 통과했습니다. Python 문서·의존성 검사 34개와 네이티브/WASM 빌드, 벡터/캐시 전체 출력 경로 smoke를 확인했습니다.
