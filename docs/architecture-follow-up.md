# 아키텍처 검토 후속 구현

2026-11-04 승인된 후속: 콘텐츠 기준 통일 → 애니메이션 바인딩 검증 → 광역 피해 조회 개선 → 전체 프레임 구간 측정. 큰 폴더 경계는 [현재 구현 아키텍처](current-architecture.md)를 유지합니다.

2026-11-06 추가한 [콘텐츠 검증·오퍼·앱 접근 경계](content-offers-boundary.md)가 현재 생성 API와 후속 구조의 최신 기록입니다.

## 콘텐츠 정의의 단일 기준

- 전역 기본 카탈로그를 참조하던 `game::isBuilding`·`game::isUnit`을 제거했습니다. 규칙, 클릭·드래그 좌표 보정, 선택 미리보기, 아이콘, 카드 테두리는 모두 현재 `Run::catalog()`의 분류를 읽습니다.
- `CardDefinition.role`의 중복 분류 문구를 제거했습니다. [CardPresentation.h](../src/game/presentation/CardPresentation.h)가 실제 `CardCategory`에서 이름·조작 설명을 도출하고, Theme은 해당 분류로 색상을 선택합니다. 개별 종류별 그림·공격 동작은 게임 표현·규칙에 남습니다.
- 모집 교체 확인 가격, 슬롯 목표 수, 강화 완료 안내도 실제 판의 정의를 사용합니다.
- 현재는 `Run::create(Catalog)`로 검증한 판을 `Application`에 넘깁니다. 새 원정도 같은 검증된 카탈로그를 유지하며, 기본 정의는 실행 진입점에서 선택합니다.

[컨트롤러 회귀 테스트](../tests/native/controllers_test.cpp)는 연사탑 종류를 작은 범위의 마법으로 재정의하여 소수 좌표를 클릭·드래그해도 적을 맞히는지 확인합니다. [앱 통합 테스트](../tests/native/application_test.cpp)는 실제 ThorVG 출력 픽셀에서 마법 분류의 보라색 카드 테두리까지 검사합니다. [의존성 검사](../tests/test_source_boundaries.py)는 UI·입력이 전역 분류로 되돌아가는 것을 막습니다.

## 명시적 몹 애니메이션 연결

[AnimationBindings](../src/game/presentation/AnimationBindings.h)는 `profile → source asset → idle/move/attack/death marker`를 선언합니다. 프로필 이름과 원본 에셋 키는 달라도 됩니다. 서로 다른 프로필이 같은 원본을 다른 클립으로 사용할 수도 있습니다.

[Assets.h](../src/game/presentation/Assets.h)가 게임의 원본 파일과 바인딩 목록을 조합합니다. 초기화 시 모든 등록 바인딩의 키 중복·원본 유무·유효한 클립과 모든 Catalog 적의 프로필 참조를 검증하고, 하나라도 잘못되면 부분 레지스트리를 반환하지 않습니다. 오류에 프로필·원본·적 인덱스를 포함합니다. 예상하지 못한 실행 중 연결 누락도 ActorAnimations가 오류를 기록하고 Application이 프레임 실패로 전달하므로 보이지 않는 적이 계속 전투하지 않습니다.

[애니메이션 테스트](../tests/native/animation_test.cpp)는 두 번째 원본/몹의 실제 샘플, 잘못된 프로필, 원본 누락, 클립 누락, 중복 키, 실행 중 누락과 리셋을 검증합니다. 외부 콘텐츠 팩 로더나 범용 에셋 데이터베이스를 추가한 것은 아닙니다.

## 광역 피해 배치

[Combat.cpp](../src/game/rules/Combat.cpp)의 피해 기록은 틱 안에서만 유효한 대상 배열 인덱스를 보유합니다. 스폰 삽입이 끝난 다음 공격을 모으고 모든 피해가 적용된 뒤 사망 개체를 제거합니다. 인덱스는 다음 틱이나 공개 이벤트로 넘기지 않습니다. 공격 대상 선택·연쇄 검색은 기존 규칙을 유지하며 피해 적용의 반복 ID 검색만 제거했습니다.

피해 적용 비용은 `O(피해 건수 × 대상 수)`에서 `O(피해 건수)`로 바뀝니다. [규칙 테스트](../tests/native/rules_test.cpp)는 순서가 뒤섞인 큰 ID, 대상별 갑옷, 다음 틱의 압축·대량 재할당, 상호 사망과 유닛별 피해를 확인합니다. 동시 공격의 순서·과잉 피해·사망/누수 우선순위는 그대로입니다.

포격탑 8기가 같은 지점에 밀집한 적을 동시에 공격하는 합성 조건입니다. 매번 쿨다운을 초기화한 40회 중앙값이며 단위는 ms입니다. 수정 전 정적 링크 실행 파일을 보존하여 같은 기기에서 순차 실행했습니다. 두 실행 파일은 최적화 변경 외에도 빌드 시점이 달라 미세한 수치 차이를 일반화하지 않습니다.

| 적 수 | 수정 전 | 수정 후 |
| ---: | ---: | ---: |
| 100 | 0.047 | 0.010 |
| 300 | 0.329 | 0.030 |
| 1,000 | 3.005 | 0.094 |
| 3,000 | 24.720 | 0.292 |

일반 웨이브의 평균 프레임 시간이나 게임 전체 FPS를 뜻하지 않습니다. 재현 코드는 [combat_benchmark.cpp](../benchmarks/combat_benchmark.cpp), 원시 기록은 `build/benchmarks/combat-before.json`과 `build/benchmarks/frame-pipeline.json`입니다.

## 전체 출력 경로와 측정

[VideoOutput](../src/platform/sdl/VideoOutput.h)은 게임 SDL 어댑터와 벤치마크가 함께 쓰는 텍스처 업로드·출력 구현입니다. 창 소유권은 호출자에게 있고, 텍스처→SDL 렌더러→창 순서로 정리합니다. 엔진은 SDL이나 게임 정의를 참조하지 않습니다.

실행 데이터 흐름이며 코드 의존 방향과는 구분합니다.

~~~mermaid
flowchart LR
  Tick["Run · 고정 시간 규칙"] --> Facts["상태 / 전투 사실"]
  Facts --> Presentation["ActorAnimations / Feedback / Soundscape"]
  Presentation --> Scene["View · 전장 / HUD / UI 등록"]
  Scene --> Raster["ThorVG CPU 렌더"]
  Raster --> Video["SDL VideoOutput · 업로드 / Present"]
  Presentation --> Audio["AudioMixer / SDL AudioDevice"]
  Tick -. 시간 .-> Profiler["FrameProfiler · 최근 120프레임"]
  Scene -. 시간 .-> Profiler
  Raster -. 시간 .-> Profiler
  Video -. 시간 .-> Profiler
  Audio -. 시간 .-> Profiler
~~~

[FrameProfiler](../src/engine/diagnostics/FrameProfiler.h)는 실제 메인 스레드 구간의 경과 시간을 기록합니다. 프레임 기록에는 정렬·할당이 없고, 진단을 읽을 때만 최근 최대 120개 샘플의 구간별 p50/p95를 계산합니다. 매번 읽을 수 있는 `performance.latest`, `performance.p50`, `performance.p95`는 모두 ms 단위입니다.

| 필드 | 구간 |
| --- | --- |
| `simulation` / `steps` | `Run::advance` 합계 / 수행한 고정 틱 수 |
| `presentation` | 개체 재생 갱신, 사실 소비, 카드·효과·사운드 예약과 선택 동기화 |
| `cachePreparation` | 처음 공유 프레임을 준비한 비용, 일반 렌더와 별도 |
| `scene` | View의 월드·HUD·UI와 ThorVG 도형 구성 |
| `raster` | 렌더 시작의 정리·버퍼 준비와 실제 CPU draw/sync |
| `audio` | PCM 믹싱과 SDL 스트림 공급, 실제 재생 지연은 제외 |
| `upload` | SDL 텍스처 생성/크기 변경 및 픽셀 업로드 호출 |
| `present` | SDL clear/draw/present; 드라이버·VSync 대기를 포함할 수 있음 |
| `total` | SDL 프레임 콜백 안의 전체 경과 시간; 초기화·이벤트 콜백은 제외 |
| `interval` | 이전 프레임 시작 이후 경과 시간, 대기와 스케줄링 포함 |
| `performance.other` | 최신 프레임의 total에서 계측한 구간 합을 뺀 나머지 |

브라우저의 타이머 정밀도 제한 때문에 짧은 구간은 0ms가 될 수 있습니다. `steps`가 1이어도 `simulation`이 0ms로 관측될 수 있으며, 출력 호출도 마찬가지입니다. 측정값에 인위적인 최솟값을 더하지 않습니다.

구간의 백분위는 각각 계산하므로 구간 p95를 합쳐도 total p95가 되지 않습니다. NaN·음수·total을 넘는 구간 합은 기록하지 않습니다. 정지 프레임은 틱 수·시뮬레이션 시간을 0으로 기록하며, 측정 시간이 게임 시간에 영향을 주지 않습니다. 설정이나 새 판에서 프로파일러 이력은 앱 세션 동안 계속 유지됩니다.

브라우저 개발자 도구에서는 읽기 전용 진단을 조회할 수 있습니다.

~~~javascript
JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).performance
~~~

## 통합 벤치마크 결과

측정 시각: 2026-11-04T23:18:45.040877+09:00. 호스트: macOS-27.0.1-arm64-arm-64bit-Mach-O.

release, 단일 스레드 ThorVG 소프트웨어, 논리 1200×800, 밀도 1/1.5, 경로에 분포한 적 3종과 타워 6개, 프레임당 고정 1틱, 3프레임 준비 후 60프레임입니다. 실제 앱의 규칙·연출·HUD·도형 생성·렌더와 게임이 사용하는 PCM 공급·SDL 출력 코드를 실행했습니다. 집단 구성만 벤치마크 전용 fixture에서 준비하며 상태 쓰기용 웹 API는 없습니다.

**SDL video/audio driver는 모두 `dummy`입니다.** 텍스처 업로드·소프트웨어 출력 호출과 PCM 장치 공급을 포함하지만 하드웨어 GPU 표시·실제 소리·브라우저·모바일 성능을 입증하지 않습니다. VSync는 끄도록 요청했고 빠른 프레임은 측정 종료 후 60Hz까지 대기하여 오디오 장치가 큐를 소비하게 했습니다. total은 이 대기를 제외하고 interval은 포함합니다. 적은 높은 HP와 낮은 이동 속도로 측정 동안 유지합니다. 실전에서 느린 프레임에 수행할 추가 고정 틱의 비용은 이 고정 1틱 비교에 포함되지 않습니다.

| 밀도 | 적 수 | 벡터 p50 / p95 ms | 공유 프레임 p50 / p95 ms |
| ---: | ---: | ---: | ---: |
| 1 | 100 | 7.97 / 8.85 | 7.57 / 8.42 |
| 1 | 300 | 10.94 / 12.84 | 10.05 / 11.85 |
| 1 | 1,000 | 19.56 / 21.60 | 15.84 / 17.41 |
| 1.5 | 100 | 14.83 / 15.85 | 15.06 / 16.37 |
| 1.5 | 300 | 19.19 / 21.99 | 20.87 / 35.45 |
| 1.5 | 1,000 | 30.99 / 57.92 | 33.71 / 55.73 |

1,000마리·밀도 1.5에서 벡터 total p50 약 30.99ms 중 raster p50이 약 26.06ms입니다. 이 조건에서는 주된 남은 비용이 렌더링입니다. 밀도 1에서는 공유 프레임이 유리하지만 밀도 1.5에서는 반대이므로 기본 벡터 정책은 유지하고 설정 비교를 제공합니다. 모든 조건에서 60 FPS를 달성했다고 해석하지 않습니다. 다음 성능 작업은 실제 목표 기기의 구간 측정 후 정적 배경/HUD 재사용·출력 해상도·에셋별 경로 선택 중 우선순위를 정합니다.

실행은 컴파일·브라우저 테스트와 겹치지 않게 합니다.

~~~sh
python3 tools/build.py native
python3 tools/benchmark_pipeline.py --frames 60
python3 tools/build.py web
npm run test:browser
python3 -m unittest discover -s tests -p 'test_*.py' -q
~~~

[실행 도구](../tools/benchmark_pipeline.py)는 실제 드라이버, 실행 파일·에셋 SHA-256, 준비/정상 프레임, 구간별 중앙값·p95를 `build/benchmarks/frame-pipeline.json`에 저장합니다. `--video-driver`와 `--audio-driver`로 다른 호스트 장치를 선택할 수 있지만 실행 가능한 SDL 드라이버를 명시해야 합니다. 이 도구는 벤치마크 실행 파일을 빌드하지 않습니다.

## 최종 검증

macOS 네이티브·WASM 빌드가 통과했습니다. 네이티브 9개 타깃(규칙/입력, 오디오, 컨트롤러/콘텐츠, 에셋, 애니메이션, 공유 프레임, 프레임 프로파일러, 실제 앱/픽셀 통합, SDL smoke)과 브라우저 8개 시나리오, Python 문서·의존성 검사 34개가 통과했습니다.

브라우저는 실제 SDL 입력·Web Audio·모바일 크기 전환과 읽기 전용 계측을 검증합니다. 네이티브 통합 수치는 위에 기록한 dummy 장치 조건에서 측정했으며, Windows 실행·모바일 실기기 성능은 이번 검증에 포함하지 않습니다. 성능 수치를 CI의 고정 시간 제한으로 단정하지 않습니다.
