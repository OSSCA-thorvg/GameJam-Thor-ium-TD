# 독립 코드 검토 후속 수정

2026-11-08 승인된 세 결함을 수정했습니다. 검토는 이전 대화·README·기획 문서 없이 세 에이전트가 코드부터 읽는 방식으로 진행했고, 메인 에이전트가 별도 재현으로 교차 확인했습니다. [현재 구조](current-architecture.md)와 [배치 사전 판정](placement-preflight.md)에 이어지는 구현 기록입니다.

## 결과 화면과 명령의 재시작 조건 공유

승리·패배 화면은 `ConfirmReset` 버튼을 등록했지만 실행 코드는 `Modal::NewRun`만 허용하여, 정상 결과 화면에서 버튼이 동작하지 않았습니다.

[CommandPolicy.h](../src/game/presentation/CommandPolicy.h)의 `canConfirmNewRun`을 화면과 [AppFlow](../src/app/AppFlow.cpp)가 함께 사용합니다. 게임의 승패 결과 화면에서는 즉시 새 원정을 시작할 수 있습니다. 진행 중인 판은 기존 확인창이 필요하고, 메인 화면에서 진행 중 원정을 새로 시작하는 확인 흐름도 유지합니다. 설정·가이드 등 다른 화면의 오래된 확인 명령과 새 판에 도착한 중복 명령은 판을 초기화하지 않습니다.

정책은 게임 화면 계층의 작은 읽기 전용 함수입니다. 규칙이나 엔진이 화면을 참조하지 않으며, 새 범용 상태머신을 추가하지 않았습니다. `AppFlow`의 클래스 설명도 실제 담당 범위인 탐색·설정·새 원정·시뮬레이션 허용 조회에 맞췄습니다.

## 모든 유효한 양수 채굴 시간에서 안전한 위치 계산

검증을 통과한 `minerStep=1e-12`에서 기존 코드는 이동 진행량을 `int`로 바꾼 뒤 경로 범위를 제한했습니다. 검토 당시 UBSan으로 범위를 벗어난 변환을 확인했습니다.

[minerPosition](../src/game/rules/Mining.cpp)은 나눗셈을 `double`로 수행하고, 진행량을 실제 경로 인덱스 범위에 제한한 **후에** 정수로 변환합니다. 양수 이동 시간을 임의로 거절하는 대신 현재 검증 경계 전체에서 조회가 안전하도록 바꿨습니다. 이동이 끝나고 굴착을 기다리는 동안에도 마지막 작업 위치를 유지합니다. 굴착 완료·자원 지급 시점은 기존 작업 규칙이 담당합니다.

## 배치·이동의 칸 중심을 규칙이 결정

기존 규칙은 패드 중심의 허용 반경 안에 있는지만 확인하고 입력 좌표를 그대로 저장했습니다. 같은 패드의 반대쪽을 차례로 선택하면 서로 다른 타워로 처리할 수 있었습니다. 정수로 맞추는 현재 UI에서는 발생하지 않았지만 공개 규칙 API의 불변식이 부족했습니다.

[resolveCell](../src/game/rules/RuleGeometry.h)이 유한한 입력을 확인하고 실제 정의된 칸 중심을 반환합니다. 카드의 내부 `PlayPlan`은 이 좌표를 보유하여 미리보기 가격·점유 검사·개체 생성·배치 이벤트에 같은 대상을 사용합니다. [정비 중 유닛 이동·교환](../src/game/rules/Combat.cpp)도 같은 변환을 거칩니다. 마법은 자유로운 소수 좌표를 유지합니다.

## 회귀 검증

- [실제 앱 테스트](../tests/native/application_test.cpp): 공개 명령과 검증된 콘텐츠로 자연스러운 승리·패배에 도달한 뒤, 등록된 버튼에 포인터를 눌렀다 떼어 재시작합니다. 설정 유지와 중복 입력 거절도 확인합니다.
- [컨트롤러 테스트](../tests/native/controllers_test.cpp): 결과 화면·명시적 확인창·진행 중 게임·다른 화면의 11가지 조합에서 표시 정책과 명령 처리를 함께 확인합니다.
- [배치 테스트](../tests/native/play_preview_test.cpp): 패드 반대쪽 좌표의 단일 타워 강화·교체·정확한 비용과 이벤트, 유닛 중복 거절, 정비 중 좌표 정규화와 자리 교환을 공개 명령으로 확인합니다.
- [규칙 테스트](../tests/native/rules_test.cpp): `denorm_min`, `1e-12`, 기본값, 지원 상한의 이동 시간에서 위치·보간·굴착 완료의 단일 지급을 확인합니다. 규칙 소스를 UBSan과 `float-cast-overflow`로 계측한 실행에서도 통과했습니다.
- [브라우저 결과 화면](../tests/browser/results.js): 기본 게임에서 실제 공세로 패배한 뒤 320px 화면의 결과 버튼으로 재시작하고 다시 첫 웨이브를 시작합니다. 상태를 쓰는 진단 API는 사용하지 않습니다.

네이티브/WASM 빌드, 네이티브 12개 타깃, 브라우저 10개 시나리오와 Python 문서·의존성 검사 34개가 통과했습니다. UBSan으로 계측한 규칙 테스트의 검사항목 198개도 통과했습니다. Windows 실행·모바일 실기기 성능은 이번 검증에 포함하지 않습니다.

네이티브 빌드 후 프로젝트 루트에서 실행한 macOS Clang의 추가 검증 명령은 다음과 같습니다. 실제 규칙 소스를 계측하며, UI·연출 라이브러리는 기존 빌드 결과를 연결합니다.

~~~sh
c++ -std=c++17 -O1 -g -fsanitize=undefined,float-cast-overflow \
  -fno-sanitize-recover=all -Isrc tests/native/rules_test.cpp \
  src/game/rules/{Run,Combat,Cards,Mining,Progression}.cpp \
  src/game/content/{Catalog,CatalogValidation}.cpp \
  build/native/libthorium-feedback.a build/native/libthorium-animation.a \
  build/native/libthorium-ui.a -o /tmp/thorium-rules-sanitized
/tmp/thorium-rules-sanitized
~~~
