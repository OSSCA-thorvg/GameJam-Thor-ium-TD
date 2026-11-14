# 사운드 시스템과 MP3 교체 안내

2026-11-04 요청에 따라 SDL3 오디오 출력과 23종 효과음을 추가했습니다. 현재 배포 콘텐츠의 MP3는 배경음악 `content/audio/bgm.mp3` 하나이며, 효과음은 시작 시 합성한 짧은 비프음을 사용합니다. 각 효과음의 ID·파일명·재생 정책을 유지한 채 MP3로 하나씩 바꿀 수 있습니다.

## 배경음악

`content/audio/bgm.mp3`가 있으면 시작할 때 읽어 끝없이 반복한다. 압축된 파일을 메모리에 두고 512프레임씩 풀어 효과음 아래에 0.4 배율로 섞으며, 끝에 닿으면 첫 프레임으로 돌아간다. 효과음과 같은 볼륨·음소거를 따르고, 음소거 중에는 멈춰 있다가 이어서 재생한다. 일시정지·창 전환으로 효과음을 비울 때도 곡의 위치는 유지한다. 파일은 16MiB까지 받는다. 설정의 오디오 탭에서 음악 크기를 10% 단위로 따로 조절하며 저장된다. `content/audio/bgm-battle.mp3`가 있으면 전투가 시작될 때 약 1초에 걸쳐 그 곡으로 바뀌고, 정비로 돌아오면 다시 기본 곡으로 바뀐다. 두 곡은 각자 위치를 유지한다. 진단 JSON의 `audio.music`·`audio.musicLoops`로 적재와 반복 횟수를 확인한다.

## 조작과 재생

- 데스크톱 HUD의 소리 버튼, 모바일 HUD의 스피커 버튼으로 음소거합니다.
- 일시정지 창에서 음소거와 볼륨을 조절할 수 있습니다. 데스크톱에는 HUD 볼륨 버튼도 있습니다.
- 메인/게임의 설정 페이지에서는 5% 단위 음량 조절과 효과음 미리 듣기도 지원합니다. `S`는 음소거, `V`는 볼륨 순환입니다. 기존 `M`은 정비 중 지상/지하 전환으로 유지합니다.
- 기본 볼륨은 45%이며 버튼은 25·50·75·100% 단계로 순환합니다. 새 판을 시작해도 현재 설정은 유지합니다. 볼륨·음악 크기·음소거는 창이 가려지거나 포커스를 잃을 때 다른 설정과 함께 [SaveData](../src/app/Persistence.h)에 저장하고, 다음 실행에서 복원합니다.
- 웹에서는 첫 클릭·터치·키 입력 뒤 재생을 시작합니다. 창 비활성화 시 장치 큐·재생 중인 음·지연된 카드 등장음을 버리고, 다음 입력으로 다시 활성화합니다. 일시정지·철수/초기화 확인 창에서도 이전 전투음을 비웁니다.

브라우저의 AudioContext 활성화는 사용자 입력에 연결하며 SDL의 브리지가 실제로 실행 중일 때 PCM을 공급합니다. 관련 동작은 [MDN의 Web Audio 안내](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API/Best_practices)와 [SDL 오디오 스트림 문서](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream)를 참고했습니다.

## MP3로 바꾸기

1. `content/audio/`에 아래 이름으로 MP3를 넣습니다. 예: `content/audio/card-draw.mp3`, `content/audio/shot.mp3`.
2. 네이티브는 게임을 다시 시작합니다. 웹은 `python3 tools/build.py web`으로 다시 빌드한 뒤 새로고침합니다.
3. 존재하는 정상 MP3만 해당 효과음의 비프음을 대체합니다. 파일이 없거나 디코딩에 실패하면 비프음을 유지하며, 손상된 파일은 로그와 읽기 전용 진단의 `audio.assetErrors`에 기록합니다.

[효과음 폴더](../content/audio/README.md)에도 이 안내를 연결했습니다.

MP3는 모노/스테레오, 8~192kHz를 받아 48kHz 스테레오 출력으로 변환합니다. 효과음 하나는 압축 파일 4MiB 이하·디코딩 길이 10초 이하입니다. 시작 시 한 번 디코딩하며 재생 중 파일 읽기나 합성을 하지 않습니다. 파일 추가·수정·삭제 모두 콘텐츠 서명과 링크 의존성에 반영해 웹 데이터 패키지를 갱신합니다.

| 파일명 | 재생 시점 |
| --- | --- |
| `select.mp3` | 카드·탭 선택 등 짧은 UI 조작 |
| `confirm.mp3` | 일반 명령 성공 |
| `error.mp3` | 비용 부족·잘못된 배치 등 명령 실패 |
| `card-use.mp3` | 전투 중 카드 사용·에너지 교환으로 손패가 순환할 때 |
| `card-draw.mp3` | 퇴장 구간 뒤 새 카드가 들어올 때 |
| `placement.mp3` | 건물·유닛 배치 및 건물 강화 확정 |
| `shot.mp3` | 일반 사격·근접 공격 |
| `cannon.mp3` | 포격탑·폭파병 공격 |
| `lightning.mp3` | 전격 공격 |
| `frost.mp3` | 서리 마법 |
| `blast.mp3` | 폭발 마법 |
| `hit.mp3` | 실제 피해 적용 |
| `enemy-defeat.mp3` | 전투 개체의 사망 확정 |
| `base-hit.mp3` | 적 통과로 거점 피해 발생 |
| `mine-start.mp3` | 광부 작업 시작 |
| `mine-complete.mp3` | 광부 작업 완료 |
| `scan.mp3` | 실제 스캔 성공 |
| `upgrade.mp3` | 도구·장갑·슬롯·코일 제작 성공 |
| `reward.mp3` | 카드 모집·보상 교체 확정 |
| `wave-start.mp3` | 웨이브 시작 |
| `wave-clear.mp3` | 전투에서 정비 단계로 전환 |
| `victory.mp3` | 마지막 웨이브 생존 |
| `defeat.mp3` | 거점 파괴 |

## 코드 경계와 동시 재생

| 파일 | 책임 |
| --- | --- |
| [Audio.h](../src/engine/audio/Audio.h), [Audio.cpp](../src/engine/audio/Audio.cpp) | PCM 클립·비프음 합성·리샘플링·팬·볼륨·피크 제한·음성 수와 우선순위 |
| [Mp3.cpp](../src/engine/audio/Mp3.cpp) | `dr_mp3` 디코딩·파일/길이/PCM 검증 |
| [Soundscape.h](../src/game/presentation/Soundscape.h), [Soundscape.cpp](../src/game/presentation/Soundscape.cpp) | 게임 효과음 이름·파일·대체 음색·재생 빈도, 카드/공세/채굴/전투 결과 연결 |
| [AudioDevice.h](../src/platform/sdl/AudioDevice.h), [AudioDevice.cpp](../src/platform/sdl/AudioDevice.cpp) | 선택적 SDL 재생 장치·짧은 PCM 큐·브라우저 준비 상태·중단·정리 |
| [FeedbackChannel.cpp](../src/app/FeedbackChannel.cpp) | 규칙의 전투 사실을 시각·오디오 소비자 양쪽으로 한 번씩 전달 |

동시 재생은 최대 16개이며 효과음별 최소 간격·최대 동시 수를 따로 둡니다. 예를 들어 타격음은 60ms 간격·최대 2개입니다. 중요도가 높은 오류·웨이브 결과음은 낮은 중요도의 전투음을 대체할 수 있습니다. 좌우 팬은 전장 위치에 따라 가볍게 적용하고, 합산 출력은 유한한 피크 범위로 제한합니다. 음소거 중 요청은 쌓아 두지 않습니다.

믹싱은 메인 스레드에서 짧은 블록으로 수행하고 SDL 스트림에는 PCM 복사본을 보냅니다. 오디오 스레드가 게임 상태나 믹서의 재생 목록을 직접 읽지 않습니다. 정상 큐 목표는 512프레임 세 블록이며, 오디오 장치를 열 수 없어도 게임은 계속 실행합니다.

## 검증

2026-11-10 저장 설정의 초기값을 [AudioSettings](../src/engine/audio/Audio.h)와 통일했습니다. [SaveData](../src/app/Persistence.h)는 볼륨·음소거와 화면 설정의 기본값을 해당 설정 타입에서 가져옵니다. 저장 데이터가 없거나 볼륨 항목이 빠졌을 때는 45%로 시작하며, 기본값 복원과 같은 동작입니다. 기존에 저장된 70%·100% 등의 명시적 볼륨은 계속 복원합니다.

[persistence.js](../tests/browser/persistence.js)는 저장 없음·빈 문자열·빈 객체·손상된 문자열·볼륨 누락·사용자 설정·100% 저장값·저장소 접근 실패를 각각 독립 브라우저 컨텍스트에서 검증합니다. [애플리케이션 네이티브 테스트](../tests/native/application_test.cpp)는 저장 기본값과 실제 설정 기본값이 일치하는지 컴파일 시 확인합니다.

브라우저 시나리오가 실패하면 [실행기](../tests/browser/run.mjs)가 시나리오 이름과 마지막 공개 상태를 로그에 남기고 `.playwright-mcp/failure-<scenario>.json` 및 `.png`를 저장합니다. [CI 워크플로우](../.github/workflows/deploy-pages.yml)는 실패한 브라우저 작업의 파일만 `browser-failure-diagnostics` 아티팩트로 7일간 보관합니다. `frontend.js`의 대기 실패에는 실제로 기다린 조건도 포함합니다.

[audio_test.cpp](../tests/native/audio_test.cpp)는 합성음의 양끝·유한성, 실제 MP3 디코딩과 44.1→48kHz 변환, 누락/손상 파일 대체, 음소거, 동시 재생 상한·우선순위, 한 번만 발생하는 카드/공세 이벤트와 지연음 취소를 검사합니다. 원본 테스트 음원은 FFmpeg로 만든 짧은 880Hz 사인파입니다.

[audio.js](../tests/browser/audio.js)는 실제 클릭·터치로 SDL을 거쳐 Web Audio 분석기에 도달하는 0이 아닌 PCM, 실제 출력의 음소거·복구, 볼륨 변경, 새 판의 설정 보존과 모바일 일시정지 창을 검사합니다. 기존 전투·채굴·철수·카드 애니메이션 시나리오도 함께 실행합니다.
