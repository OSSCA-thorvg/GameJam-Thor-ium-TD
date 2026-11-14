# Rubiks와 범용 엔진 패턴 조사

조사일: 2026-11-03. 서브에이전트 3개가 규칙 감사·로컬 Rubiks·공식 엔진 문서를 각각 조사했고 주 에이전트가 통합했습니다. 읽기 전용 조사이며 참고 프로젝트의 파일·의존성·빌드 산출물을 변경하지 않았습니다. 다음 내용은 관찰과 프로젝트 적용 판단을 구분합니다.

## 판단 기준

**범용적으로 쓰기 위한 구조와 이 게임의 변경·검증을 쉽게 만드는 구조는 다릅니다.** 엔진과 클래스 이름을 맞추는 대신 현재 필요한 수명·상태·의존·갱신 순서를 확인합니다. 한 구현이어도 렌더러 없는 규칙 테스트에 필요한 경계는 가치가 있습니다. 미래 기능만을 근거로 관리자·포트·추상 베이스를 늘리지 않습니다.

채택 조건은 해결할 현재 문제, 상태 소유자, 허용 의존성, 검증 사례를 설명할 수 있는 것입니다. 성능 구조는 프로파일 결과로 추가 판단합니다.

## Rubiks의 실제 상태

참고 저장소는 로컬의 thorvg-rubiks이며 조사한 HEAD는 `6238cff2cc8937eb123f93513aef329a49fe2345`입니다. 추적 파일 수정은 없고 `git status --short`에는 미추적 `.claude/`만 있었습니다. 아래 줄 번호는 해당 조사 시점 기준입니다. 새 게임에 해당 구현이 있다는 뜻이 아닙니다.

| 실제 근거 | 관찰 | 적용 판단 |
| --- | --- | --- |
| `cube/meson.build`, `graphics/meson.build` | 규칙과 표현 계산이 ThorVG 없이 빌드되는 경계 | 규칙·좌표·미리보기의 독립 테스트 원리 채택 |
| `build_wasm.sh` 12–45행, 루트 meson.build | C++17·Meson·Emscripten, 산출물 확인 뒤 웹에 동기화 | 경로·도구 버전·필요 export를 새 게임에 맞춰 선정 |
| `engine/meson.build` 1–16행 | CPU 엔진, 로더·세이버 비활성화, WASM 스레드 비활성화 | Lottie 로더를 별도 활성화. file=false를 메모리 로딩 불가로 해석하지 않음 |
| `capi.cpp` 5–30행, `AbiContract.test.ts` | 원시 타입의 ABI와 성공·실패 계약, C++·export·TS·호출 이름 검사 | 필요한 게임 명령만 설계하고 타입·필드 배치·단위 검증 보완 |
| `Application.cpp` 383–408·836–843행 | 초기화 실패 정리, 렌더러 해제 후 ThorVG 종료 | 수명 정리 원리 채택, Lottie 원본·인스턴스 관계 추가 설계 |
| `PointerController.ts` 67–84·134–200행 | CSS→drawing-buffer 변환, pointer capture·cancel | 변환·취소 원리 채택, 3D picking·orbit은 적용 대상에서 제외 |
| `AppLifecycle.ts` 486–493·574–630행 | 숨김 중 정지·복귀 시간 초기화, listener·입력을 WASM보다 먼저 정리 | 시간 초기화·실패·종료 경계 채택 |
| `CubeEngine.ts` 1325–1403행 | 버퍼 주소·영역 검사와 heap 성장 후 ImageData view 재생성 | CPU 표시 경로에서 채택, GPU 공통 계약으로 강제하지 않음 |
| `RenderBench.cpp` 75–102행 | warm-up·median/p95·실제 그려진 프레임 측정 | 측정 방법 참고. native 큐브 결과를 WASM·Lottie 지원 수량으로 전용하지 않음 |

### 렌더링 요약의 정정

기존 문서의 ‘매 프레임 장면 재구성’은 브라우저 프레임마다 무조건 재구성한다는 뜻으로 읽힐 수 있었습니다. 실제로는 Application.cpp 684–707행에서 surface별 FrameInputs가 같으면 compose와 render를 생략합니다. 달라진 surface가 성공적으로 그려졌을 때만 frame 번호를 올립니다. CubeEngine.ts 527–539행도 그 번호가 바뀌었을 때만 putImageData를 수행합니다.

실제로 그리는 surface에서는 ThorVGSoftwareRenderer.cpp 291–300·332–338행의 rebuild_canvas가 canvas를 비우고 도형을 재구성합니다. 따라서 **변경되지 않은 surface는 합성·그리기·화면 복사를 건너뛰고, 변경된 surface를 그릴 때 canvas를 재구성**합니다. 전투 장면은 계속 바뀌므로 동일한 절감 비율을 기대할 근거는 없습니다.

다중 surface는 큐브·전개도·축 등 Rubiks의 뷰 모델입니다. 이 API나 scene bit를 지상·지하의 범용 scene manager로 복제하지 않습니다. CPU pixel_buffer를 요구하는 Renderer 계약도 새 GPU 출력 경계를 제한할 수 있으므로 그대로 채택하지 않습니다.

Rubiks는 애니메이션 종료에 맞춰 논리 회전을 commit하는 부분이 있습니다. 타워 디펜스의 피해·채굴 완료는 별도 게임 시간이 결정해야 하므로 이 부분은 새 규칙으로 설계합니다.

## 범용 엔진 공식 문서 비교

확인한 문서는 Unity 6.0(6000.0), Unreal 문서 표시 5.8, Godot 4.4 고정판입니다. Godot stable도 교차 확인했습니다. 문서 버전은 비교 근거이며 이 엔진이나 해당 버전을 새 프로젝트에 도입한다는 뜻이 아닙니다.

| 공식 문서의 관찰 | 이 게임에 가져올 원리 | 범용성 비용으로 보류할 부분 |
| --- | --- | --- |
| [Unity Components](https://docs.unity3d.com/6000.0/Documentation/Manual/Components.html): GameObject 기능을 Component로 조합 | 이동·공격·상태의 책임을 필요한 데이터와 함수로 조합 | 덱·레시피까지 GameObject·Transform·공통 Update 베이스로 만드는 구조 |
| [Unity ScriptableObject](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScriptableObject.html): 인스턴스가 공유하는 데이터 자산 | EnemyDefinition과 현재 HP·쿨다운 분리 | 에디터 자산 객체 체계 복제. 읽기 전용성은 우리의 선택이며 원본 타입의 보장으로 취급하지 않음 |
| [Unreal Components](https://dev.epicgames.com/documentation/en-us/unreal-engine/components-in-unreal-engine): 비공간 기능·Transform·시각 및 충돌 기능을 구분 | 공간·게임 기능·표현의 독립성 | 동적 등록·tick·render proxy·물리 상태 생성 체계 전체 |
| [Unreal Data Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine): 데이터 자산과 ID·번들 관리 | 타입이 있는 정의와 참조 ID | 글로벌 Asset Manager·상속형 자산·번들 프레임워크 |
| [Godot Resources](https://docs.godotengine.org/en/4.4/tutorials/scripting/resources.html): 공유 데이터와 실행 Node 구분 | 공유 정의와 개별 상태, 에셋 수명 구분 | Resource·PackedScene 체계 전체, ThorVG에도 같은 공유 동작이 있다는 가정 |
| [Godot Scene Organization](https://docs.godotengine.org/en/4.4/tutorials/best_practices/scene_organization.html): 의존 관계 전달·중재, scene tree를 수명 관계로 판단 | 명시적 소유자와 필요한 의존성 전달 | 공간과 수명·게임 규칙을 하나의 Node 트리로 강제 |
| [Unity Execution Order](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html), [Unreal Actor Ticking](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine) | 갱신 순서와 빈도를 명시 | 개체 콜백의 우연한 순서 의존, 일반 tick 의존 그래프 |
| [Godot Processing](https://docs.godotengine.org/en/4.4/tutorials/scripting/idle_and_physics_processing.html) | 게임 tick과 화면 프레임 분리 | 고정 갱신을 위해 물리 엔진을 함께 도입 |
| [Godot 2D Transforms](https://docs.godotengine.org/en/4.4/tutorials/2d/2d_transforms.html) | 화면·viewport·논리 좌표와 입력 역변환 | 2.5D 표현만을 이유로 3D world·raycast 도입 |

## 이번 설계의 채택·조건부·보류

| 판단 | 항목 | 필요한 근거 |
| --- | --- | --- |
| 채택 제안 | 정의/상태 분리, 한 판의 명시적 소유자, 타입 있는 명령과 결과, 좌표 타입 구분, 게임 시간 분리 | 현재 전투·채굴·재시작·headless 테스트 요구 |
| 조건부 | 풀·공간 분할·SoA·Lottie 갱신 빈도 조절 | 실제 장면의 프로파일과 변경 전후 측정 |
| 조건부 | 개별 인터페이스·추가 로딩 경계 | 현재 테스트 격리나 서로 다른 책임을 설명할 수 있음 |
| 보류 | 범용 ECS·Component 베이스·SceneTree 복제·전역 이벤트 버스·DI 컨테이너·Service Locator | 현재 요구에 비해 저장·수명·순서 추적 비용이 증가 |
| 보류 | 반사·범용 직렬화·동적 플러그인·스크립트·일반 병렬 스케줄러 | 데이터형 사용자 몬스터와 단일 세션에 먼저 필요한 범위를 넘음 |

소유권과 갱신 순서를 표현하는 구체 설계는 [아키텍처](architecture.md), 규칙 선택지는 [규칙 질문](rule-options.md), 개발 순서는 [시작 계획](preproduction-plan.md)에 연결합니다.
