# Lottie 애니메이션과 개체 상태

2026-11-04 승인한 `AnimationPlayer + 작은 개체 상태머신 + 임시 공격/사망 클립`을 구현했습니다. 현재 적용 대상은 Lottie로 그리는 적입니다. 유닛·건물은 기존 벡터 아이콘과 피격·반동 효과를 사용하며, 상태별 Lottie를 준비하면 같은 플레이어와 상태머신을 재사용할 수 있습니다.

후속으로 [공유 프레임·위상차·벡터 풀](shared-animation-cache.md)을 구현했습니다. 그리기 자원의 공유·풀 수명과 기본 렌더링 모드는 해당 문서가 최신입니다.

프로필과 에셋의 명시적 연결 및 카탈로그 참조 검증은 [아키텍처 검토 후속](architecture-follow-up.md)의 현재 구현을 따릅니다.

## 책임과 데이터 흐름

~~~mermaid
flowchart TD
    Run["Run · 이동/저지/공격/피해/처치 확정"] --> Channel["FeedbackChannel · 사실을 한 번 drain"]
    Channel --> Actors["ActorAnimations · 살아 있는 적/사망 연출 수명"]
    Channel --> FX["Feedback · 피격/사격/피해 숫자"]
    Channel --> Sound["Soundscape · 효과음"]
    Actors --> FSM["ActorAnimator · Idle / Move / Attack / Death"]
    Sets["AnimationSets · 게임 동작과 Lottie 마커 연결"] --> FSM
    FSM --> Player["engine AnimationPlayer · 시간/반복/완료"]
    Player --> Sample["AnimationSample · 에셋 키 + 절대 프레임"]
    Sample --> Renderer["Renderer · ThorVG 프레임 출력"]
    Renderer --> Metadata["AnimationInfo · 프레임 수/길이/마커"]
    Metadata --> Sets
~~~

마지막 `Renderer → Metadata → Sets`는 시작 시 메타데이터를 읽어 바인딩하는 흐름입니다. 재생 중 게임 상태를 렌더러에서 읽거나 렌더링 완료로 게임 규칙을 진행하지 않습니다.

| 파일 | 소유권과 책임 |
| --- | --- |
| [Animation.h](../src/engine/animation/Animation.h), [Animation.cpp](../src/engine/animation/Animation.cpp) | 엔진 공통 클립·프레임 구간·메타데이터·플레이어. 게임·SDL·ThorVG 미의존 |
| [ActorAnimator.h](../src/game/presentation/ActorAnimator.h), [ActorAnimator.cpp](../src/game/presentation/ActorAnimator.cpp) | 게임 전용 상태 전환·개체별 플레이어·살아 있는 적과 시각용 사망 정보 |
| [Renderer.cpp](../src/engine/render/Renderer.cpp) | 로딩 시 ThorVG Lottie 확장으로 마커와 길이를 읽고, 그리기 시 지정된 절대 프레임만 출력 |
| [AssetKeys.h](../src/game/presentation/AssetKeys.h), [Assets.h](../src/game/presentation/Assets.h) | 논리 에셋 키와 실제 파일 연결 |
| [State.h](../src/game/presentation/State.h) | AppState의 소스 바인딩은 세션 유지, PlayState의 개체·재생 시간은 새 판에 초기화 |
| [FeedbackChannel.cpp](../src/app/FeedbackChannel.cpp) | 같은 확정 사실을 애니메이션·시각 효과·사운드에 각각 한 번 전달 |
| [Application.cpp](../src/app/Application.cpp) | 고정 갱신에서 개체 재생 → 규칙 갱신 → 새 사실 관측, 이후 프레임 출력 |

## 클립과 재생

`FrameRange`는 시작을 포함하고 끝을 제외하는 원본 프레임 구간입니다. `AnimationClip`은 에셋·구간·초 단위 길이·`Loop/Once/Pose` 정책을 가집니다. 구간 길이는 원본의 프레임 수와 재생 길이에서 계산하므로 FPS를 게임 코드에 중복 입력하지 않습니다.

`AnimationPlayer`는 재생·정지·일시정지·재개·속도 변경을 제공하며 ThorVG 객체를 소유하지 않습니다. 같은 클립을 반복 요청하면 현재 시간을 보존합니다. 새 공격은 `Restart::Always`로 명시적으로 처음부터 재생합니다. `Once`는 마지막 프레임에서 멈추고, 완료 edge는 한 번만 소비할 수 있습니다. `Pose`는 정지 포즈를 유지하며 완료를 반복 발생시키지 않습니다.

잘못된 클립·속도는 기존 재생을 바꾸지 않고 거절합니다. 음수·NaN·무한대 시간은 진행에 사용하지 않습니다. 큰 양의 시간도 반복 구간 안에서 순환합니다. 샘플은 다음 클립의 첫 프레임으로 넘어가지 않습니다.

## 개체 상태 전환

~~~mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Move: 이동 가능
    Move --> Idle: 저지됨
    Idle --> Attack: 확정 공격 사실
    Move --> Attack: 확정 공격 사실
    Attack --> Attack: 다음 공격 · 명시적 재시작
    Attack --> Idle: 완료 · 현재 저지 상태
    Attack --> Move: 완료 · 현재 이동 상태
    Idle --> Death: 확정 처치 사실
    Move --> Death: 확정 처치 사실
    Attack --> Death: 확정 처치 사실
    Death --> [*]: 시각 연출 완료
~~~

이동 여부의 요청은 공격 중에도 최신 값으로 보관합니다. 공격 완료 시 그 값으로 돌아갑니다. 사망에 들어간 뒤에는 이동·공격·중복 사망 사실로 되돌리거나 재시작하지 않습니다. 슬로우는 이동 재생 속도에 적용하고, 공격 간격·피해는 규칙이 결정합니다. 피격은 별도의 섬광·눌림 효과로 겹쳐 적용합니다. 전투가 끝난 뒤 남아 있는 적은 대기 포즈로 돌아가고 사망 연출은 완료할 수 있게 유지합니다.

적의 공격에도 `CombatEvent::Attack`을 발행하도록 보완했습니다. 이벤트가 전달될 때 피해는 이미 규칙에서 확정되어 있습니다. 처치 사실에는 `enemyKind`를 포함하여 같은 갱신에서 생성·제거되어 화면에 관측되지 않았던 적도 올바른 종류의 사망 연출을 만들 수 있습니다.

## 시간과 수명

- 개체 재생은 게임의 고정 1/60초 갱신을 따라갑니다. 배속이 ×2이면 같은 실제 시간에 게임·개체 애니메이션 모두 두 배 진행합니다.
- 일시정지·철수/새 판 확인·메인·설정 같은 화면에서는 개체 시간을 전달하지 않습니다. 재개 시 지나간 실제 시간을 몰아서 적용하지 않습니다.
- 메인 디오라마와 출격 준비 장식은 명시적인 UI 시각을 `loopingSample`에 전달합니다. 전투 개체의 로컬 시각과 독립적입니다.
- 움직임 줄이기에서는 살아 있는 적을 대기 포즈로 그리고 사망 몸체 연출은 생략합니다. 플레이어의 상태·완료 처리는 게임 시각으로 계속 관리하므로 설정 변경이 규칙을 바꾸지 않습니다.
- 규칙에서 적은 처치 즉시 제거됩니다. 위치·종류·재생 정보만 시각용으로 남으며 적 수·저지·피해·자원·웨이브 종료에 참여하지 않습니다. 마지막 적을 처치하면 사망 연출이 남아 있어도 정비나 승리로 진행합니다.
- 사망 연출은 최대 128개입니다. 한꺼번에 초과하면 가장 오래된 시각용 사망 정보부터 제거합니다. 누수로 사라진 적은 사망 연출을 새로 만들지 않습니다.
- 새 판에서는 개체 플레이어·잔여 사망 정보·이전 ID를 정리하고 소스 메타데이터·바인딩을 유지합니다.

## 임시 에셋과 교체 방법

원본 [oreling.json](../content/animations/oreling.json)에 네 개의 마커를 넣었습니다. 현재 원본은 30 FPS, 75프레임이며 게임은 아래 시간을 파일에서 읽습니다.

| 마커 | 구간 `[begin, end)` | 정책 | 길이 |
| --- | --- | --- | --- |
| `idle` | `[0, 1)` | Pose | 정지 포즈 |
| `move` | `[0, 45)` | Loop | 1.5초 |
| `attack` | `[45, 57)` | Once | 0.4초 |
| `death` | `[57, 75)` | Once | 0.6초 |

하나의 소스에 동작을 묶어 `(asset key, instance ID)`의 ThorVG 객체를 상태 전환 중에도 유지합니다. 상태마다 소스를 다시 파싱하지 않습니다. 렌더링에서 더 이상 쓰지 않는 인스턴스는 프레임 종료 후 제한된 벡터 풀에 반환하거나 정리합니다. 공유 픽셀 경로의 수명은 후속 문서를 따릅니다. 실제 소스 파일·파싱 오류와 필수 마커 누락은 시작 시 오류로 보고합니다.

공격은 짧은 몸체 압축·도약·복귀, 사망은 기울어짐·눌림·투명도 감소를 사용한 임시 동작입니다. [build_animation_assets.py](../tools/build_animation_assets.py)는 현재 원본의 벡터 외형을 보존하며 이 임시 구간을 다시 작성하는 제작 보조 도구입니다. 디자이너가 타이밍을 수정한 뒤에는 이 도구를 실행하지 않고 수정한 파일을 사용합니다.

현재 바인딩은 시작 프레임 `ip=0`인 원본과 이름이 같은 네 마커를 전제로 합니다. 에셋을 교체할 때 마커 이름·유효 범위를 유지하고 다시 빌드합니다. 원본 FPS나 마커 길이 변경은 메타데이터를 통해 재생에 반영됩니다. 여러 파일의 클립을 한 `ActorAnimationSet`으로 묶는 것도 값 타입상 가능합니다. 2026-11-12부터 적 역할마다 같은 마커 구성을 가진 별도 원본을 바인딩합니다([적 로스터](enemy-roster.md)).

유저 팩 로더·파일 스키마 검증·애니메이션 편집 UI·블렌딩 그래프는 후속입니다. 단위·건물에 대한 Lottie 아트 바인딩도 후속이며, 현재 벡터 표현은 기존 효과를 유지합니다.

## 검증

네이티브·WASM 빌드와 네이티브 테스트 6종을 확인했습니다. 전용 애니메이션 계약은 47개이며, 기존 브라우저 5개와 새 애니메이션 시나리오가 통과했습니다. 문서·의존성 검사는 33개입니다.

- [animation_test.cpp](../tests/native/animation_test.cpp): 플레이어 반복/재시작/완료·유효성·정지/배속, 상태 우선순위·복귀, 짧은 생존과 사망 연출·상한·새 판 ID, 공격 완료가 피해를 다시 적용하지 않는 계약. SDL·ThorVG 없이 실행합니다.
- [render_assets_test.cpp](../tests/native/render_assets_test.cpp): 실제 ThorVG 메타데이터·마커, 구간별 포즈의 출력, 별개 인스턴스와 정리. `build/native/animation-preview.rgba`로 480×160 RGBA 미리보기도 저장합니다.
- [controllers_test.cpp](../tests/native/controllers_test.cpp): 새 판에서 개체 재생은 비우고 세션 바인딩을 유지하는 계약.
- [animation.js](../tests/browser/animation.js): 읽기 전용 진단과 실제 클릭/키보드로 이동·배속·메뉴 정지·공격/대기 전환·사망 연출 종료·새 판 초기화를 확인합니다.
- 기존 전투/채굴·철수·피드백·오디오·메인/모바일 브라우저 시나리오도 함께 실행합니다. 각 시나리오는 별도 브라우저 페이지/컨텍스트에서 실행하여 오디오 계측이 다른 테스트에 남지 않게 했습니다.

~~~sh
python3 tools/build.py native
python3 tools/build.py web
npm run test:browser
python3 -m unittest discover -s tests -p 'test_*.py' -q
~~~

브라우저 테스트에는 `dist/`를 제공하는 로컬 8317 서버가 필요합니다. 성능 목표의 검증과 동작 검증을 구분하며, 다수 개체의 실제 기기 성능 측정은 별도로 진행합니다.
