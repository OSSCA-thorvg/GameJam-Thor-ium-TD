# 반복 몹의 공유 프레임과 위상차

2026-11-04 승인한 공유 프레임 캐시·개체별 위상차·벡터/캐시 비교 모드를 구현했습니다. [기존 플레이어·상태머신](animation-system.md)은 계속 개체별 상태와 시간을 소유합니다.

실측에 따라 **기본값은 벡터 원본 + 제한된 인스턴스 풀**로 정했습니다. 현재의 단순한 oreling은 높은 픽셀 밀도에서 벡터가 빠릅니다. 공유 프레임은 메모리와 로딩 비용을 비교할 수 있는 선택지로 제공하며 설정 → 화면·피드백 → 애니메이션 방식에서 전환합니다.

## 소유권과 경계

~~~mermaid
flowchart TD
    Sources["원본·메타데이터 · 세션 공유"] --> Baker["AnimationFrameBaker · 원본당 scratch 1개"]
    Baker --> Cache["AnimationFrameCache · 불변 픽셀 / LRU / 32 MiB"]
    Actor["ActorAnimator · 상태 / 로컬 시간 / 고정 위상"] --> Sample["AnimationSample · 에셋 / 원본 프레임 / 클립 구간"]
    Sample --> Renderer["Renderer · 비교 모드 선택"]
    Renderer --> Vector["벡터 · 개체별 독립 Animation"]
    Vector --> Pool["퇴장 후 풀 · 애셋당 32 / 전체 128개"]
    Pool --> Vector
    Renderer --> Sprite["캐시 · 개체별 위치/크기 + 공유 프레임 참조"]
    Cache --> Sprite
    Vector --> Canvas["ThorVG CPU Canvas → SDL 화면 출력"]
    Sprite --> Canvas
~~~

| 파일 | 책임 |
| --- | --- |
| [AnimationCache.h](../src/engine/render/AnimationCache.h), [AnimationCache.cpp](../src/engine/render/AnimationCache.cpp) | SDK 미의존 프레임 키·해상도·샘플링·불변 픽셀 핸들·예산·LRU·계측 |
| [AnimationFrameBaker.h](../src/engine/render/AnimationFrameBaker.h), [AnimationFrameBaker.cpp](../src/engine/render/AnimationFrameBaker.cpp) | 원본당 scratch Lottie 하나로 프레임을 미리 렌더링, 알파 여백 정리 |
| [AnimationRendering.cpp](../src/engine/render/AnimationRendering.cpp) | 벡터 풀과 공유 이미지 경로·물리 해상도 선택·원본 fallback·그리기 수명 |
| [Renderer.h](../src/engine/render/Renderer.h), [Renderer.cpp](../src/engine/render/Renderer.cpp) | 원본/메타데이터 수명·캔버스·프레임 종료 후 자원 정리 |
| [Animation.cpp](../src/engine/animation/Animation.cpp) | 반복 클립의 위상 샘플링과 원샷의 원래 시간 유지 |
| [ActorAnimator.cpp](../src/game/presentation/ActorAnimator.cpp) | 연출용 시드·생성 ID로 고정 위상을 결정하고 상태 전환 중 유지 |
| [Application.cpp](../src/app/Application.cpp) | 설정 적용·처음 캐시 모드를 선택할 때 예열·읽기 전용 통계 |

게임 규칙과 엔진 캐시에 서로의 타입을 추가하지 않았습니다. 캐시 키에는 **에셋·내용 리비전·샘플 프레임·물리 크기·여백**을 넣으며 개체 ID·월드 좌표·남은 체력은 넣지 않습니다. 같은 역할의 적은 몸체 이미지를 공유하고, 방패·피격·슬로우 표시는 개체별 표현으로 겹쳐 그립니다. 역할별 원본은 [적 로스터](enemy-roster.md)를 따릅니다.

## 재생 시점과 구간

생성 시 `actorLoopPhase(ID, 연출용 시드)`로 `[0, 1)` 범위의 위상을 정합니다. 고정 정수 해시를 사용하며 게임 난수나 자원·공격 간격을 변경하지 않습니다. 같은 시드·ID는 같은 위상이며, 상태 전환마다 재추첨하지 않습니다.

반복 클립 샘플 위치는 `fraction(로컬 시간 / 클립 길이 + 위상)`입니다. 이동 → 공격 → 이동으로 돌아와도 위상을 유지합니다. `Once`와 `Pose`에는 위상을 적용하지 않아 공격·사망은 확정 이벤트의 첫 프레임에서 시작합니다. 일시정지·배속은 기존 게임 시각을 그대로 따릅니다.

캐시 경로는 표시 프레임만 샘플링하며 플레이어의 연속 시각·완료 시점·게임 판정을 바꾸지 않습니다. `AnimationSample`에 클립 구간을 함께 전달하여 이웃 상태의 프레임으로 반올림하지 않고 첫/마지막 프레임을 보존합니다. 기본 샘플링은 현재 원본과 같은 **30 FPS**입니다. 24 FPS도 측정했으며 클립당 샘플 수는 최대 4096개로 제한합니다.

## 이미지·메모리 수명

- 기본 물리 크기 단계는 **32·64·96px**입니다. 논리 그리기 크기 × 실제 렌더 밀도로 선택하며, 가장 큰 크기를 넘으면 벡터로 그립니다. 큰 UI 디오라마·출격 준비 장식도 벡터로 유지합니다.
- 베이킹은 고정 원본 아트보드에 12.5% 여백을 둡니다. 투명 영역은 픽셀 복사 범위에서 줄이고 원본에 대한 offset을 보존하여 동작별 발 위치가 흔들리지 않게 합니다.
- 픽셀은 premultiplied ABGR8888로 만들고 같은 색공간으로 읽습니다. 비교 테스트로 위치·알파 오차와 사망 마지막 프레임의 투명도를 확인합니다.
- 프레임은 `shared_ptr<const AnimationRasterFrame>`로 공유합니다. 프레임별 그리기 풀과 그림 객체가 픽셀 핸들을 잡고 있는 동안 LRU가 해당 프레임을 퇴출하지 않습니다. raw 이미지 paint를 먼저 해제하고 픽셀 핸들을 나중에 놓습니다.
- raw 그림 객체는 생성 시 하나의 불변 프레임에만 바인딩합니다. 같은 객체에 다른 버퍼를 재로드하지 않고 프레임별 풀에서 위치·크기만 바꿔 사용합니다. 프레임별로 사용하지 않은 풀은 종료 후 해제하며, 사용 중인 풀의 여유 객체도 실제 사용량 +4개까지로 정리합니다. 이는 현재 SDK의 이전 bitmap 표면 참조가 남는 경로를 피하고 반복되는 로더 작업도 줄입니다.
- 예산은 **캐시 소유 픽셀 allocation 32 MiB**입니다. 투명 여백을 줄인 뒤에도 vector에 남아 있는 capacity까지 계산합니다. SDK 객체·벡터 풀·캔버스·폰트까지 포함하는 전체 앱 메모리 상한은 아닙니다.
- 예산을 확보하지 못하면 베이킹 allocation을 시작하지 않고 벡터 경로로 돌아갑니다. 오류와 예산 부족은 구분하며 통계에 남깁니다.
- 캐시는 처음 공유 프레임을 선택할 때 설정 화면에서 일반 적의 원본만 예열합니다. 다른 역할은 처음 그릴 때 예산 안에서 굽습니다. 현재 원본의 네 상태를 세 해상도로 준비하면 225개 프레임, 픽셀 allocation은 **6,720,000 bytes ≈ 6.41 MiB**입니다. 기본 벡터 모드의 시작 시에는 이 메모리를 할당하지 않습니다.
- 새 원정은 개체 재생·그리기 참조를 해제하고 이미 준비한 프레임은 세션 동안 재사용합니다. 원본 내용 리비전도 키에 포함하며 실행 중 원본 hot reload는 아직 제공하지 않습니다.

## 벡터 재등장 비용

화면에서 빠진 벡터 Animation을 즉시 모두 버리는 대신 애셋당 32개, 전체 128개까지 보관합니다. 새 몹은 같은 애셋의 객체를 빌려 자기 샘플·위치·크기를 적용합니다. 개체의 상태머신·위상·재생 시간은 이 풀에 들어가지 않으므로 이전 몹의 사망 포즈나 시간이 다음 몹에 남지 않습니다. 캐시 모드도 개체별 그림자·위치·이미지 합성은 계속 필요합니다.

## 비교 측정

[animation_benchmark.cpp](../benchmarks/animation_benchmark.cpp)와 [benchmark_animations.py](../tools/benchmark_animations.py)로 각 조건을 새 프로세스에서 측정했습니다. 원시 결과는 `build/benchmarks/animation-cache.json`에 저장하며 측정 시각·플랫폼·바이너리/원본 SHA-256을 포함합니다.

조건은 macOS 27.0.1 arm64, release 빌드, ThorVG 단일 스레드 CPU 렌더러, 논리 1280×720, 48×48 몹입니다. CPU 모델 문자열은 조회되지 않아 모델명은 기록하지 않았습니다. 3프레임 준비 뒤 45프레임을 측정했고, 동일한 포즈의 벡터/캐시와 원래 연속 프레임 벡터를 구분했습니다. 100·300·1,000마리 × 밀도 1/1.5 × 24/30 FPS 조건을 원시 기록에서 비교할 수 있습니다.

**아래는 게임 전체 FPS가 아닌 샘플링·장면 제출·CPU 그리기 구간의 중앙값(ms)입니다.** 전투 시뮬레이션·HUD·SDL 업로드·오디오 비용은 제외했습니다. OS 부하와 발열에 따른 변동이 있으므로 절대 성능 보장이 아니라 현재 애셋의 경로 선택 자료로 사용합니다.

| 몹 수 | 밀도 1 벡터 / 캐시 | 밀도 1.5 벡터 / 캐시 |
| --- | --- | --- |
| 100 | 1.24 / 1.44 ms | 2.10 / 3.13 ms |
| 300 | 2.75 / 3.43 ms | 4.24 / 7.31 ms |
| 1,000 | 9.53 / 10.40 ms | 12.94 / 22.32 ms |

30 FPS 동일 포즈 비교입니다. 밀도 1·1,000마리 조건에서 첫 프레임은 벡터 43.94 ms, 캐시 11.70 ms이며 캐시 예열 1.13 ms는 별도입니다. 이 조건의 프로세스 피크 RSS는 약 50.92 / 9.16 MiB입니다. 밀도 1.5·1,000마리에서 첫 프레임은 48.95 / 23.19 ms, 피크 RSS는 55.31 / 13.62 MiB입니다. p95를 포함한 전체 결과는 원시 기록에 있습니다.

결론은 **현재 단순 몹의 기본값은 벡터 + 풀**, **공유 프레임은 메모리·첫 등장 부담·복잡한 원본의 대안으로 비교 가능한 경로**입니다. 래스터 합성 비용이 남아 있으므로 이 단계만으로 물량 전체의 CPU 비용이 사라진다고 보지 않습니다. 새 몹이나 GPU 합성 경로를 추가하면 같은 조건으로 다시 측정합니다.

## 검증과 재현

최종 네이티브·WASM 빌드, 네이티브 테스트 7종, 브라우저 시나리오 7개, 문서·의존성 검사 33개가 통과했습니다. 공유 모드는 데스크톱의 연속 재생·모드 전환과 320px 실제 전투에서도 확인했습니다.

- [animation_cache_test.cpp](../tests/native/animation_cache_test.cpp): 실제 공유 버퍼·LRU·핀·예산·리비전, 구간 끝점, DPR, 알파/피벗, 모드 전환, 풀 재사용·상한, 위상과 원샷 독립성, 120프레임의 연속 독립 포즈를 별도 벡터 렌더러와 픽셀 비교.
- [shared-frames.js](../tests/browser/shared-frames.js): 실제 UI로 모드를 전환하고 개체 시각이 유지되는지, 전투 중 예열 이미지 재사용·새 판의 자원 해제·320px 설정을 확인합니다.
- 기존 플레이어·규칙·오디오·메인·모바일 시나리오도 함께 실행합니다.

~~~sh
python3 tools/build.py native
python3 tools/benchmark_animations.py --frames 45
python3 tools/build.py web
npm run test:browser
python3 -m unittest discover -s tests -p 'test_*.py' -q
~~~

측정은 컴파일·브라우저 테스트와 동시에 실행하지 않습니다. 네이티브 결과를 모바일·브라우저 전체 성능으로 확대 해석하지 않으며, 실제 단말의 게임 전체 프로파일링과 GPU 합성·sprite atlas는 후속입니다.
