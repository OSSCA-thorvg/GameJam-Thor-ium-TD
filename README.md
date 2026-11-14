# Thor-ium TD

> 깊이 캐고, 끝까지 지켜라.

![Thor-ium TD](docs/images/thumbnail.png)

**Thor-ium TD**는 카드로 건물·유닛·마법을 운용해 몰려오는 적에게서 거점을 방어하고, 웨이브 사이 정비 시간에는 지하 광산을 채굴해 다음 공세에 대비하는 **타워 디펜스 × 광산 채굴** 웹 게임입니다. OSSCA × ThorVG 2026 게임잼(ThorJam) 출품작이며, 인게임 전장과 메뉴·HUD를 포함한 모든 화면 그래픽을 [ThorVG](https://github.com/thorvg/thorvg)로 렌더링합니다.

- **웹에서 바로 플레이 (GitHub Pages):** <https://ossca-thorvg.github.io/GameJam-Thor-ium-TD/>
- 별도 설치 없이 브라우저에서 바로 실행되며, 데스크톱(마우스·키보드)과 모바일 터치를 모두 지원합니다.
- 한국어와 영어(인터페이스)를 지원하며, 처음 플레이한다면 메인 화면의 **연습 원정**(약 3~4분)부터 시작해 보세요.

## 스크린샷

| 지상 방어 (5구역 후반 공세) | 지하 채굴 (정비 시간) | 출격 준비 (1구역) |
| --- | --- | --- |
| ![전투](docs/images/screenshot-battle.png) | ![채굴](docs/images/screenshot-mining.png) | ![출격 준비](docs/images/screenshot-prep.png) |

## 게임 방법

1. **지상 방어 (전투):** 시간에 따라 차오르는 에너지로 손패의 카드를 전장에 배치합니다. 건물은 둥근 패드에, 유닛은 길 위의 배치 칸에 놓으며, 마법은 원하는 위치에 즉시 시전합니다. 이미 지어진 건물에 같은 건물 카드를 쓰면 강화되고, 다른 건물 카드를 쓰면 교체됩니다.
2. **지하 채굴 (정비):** 웨이브를 막아내면 지하 광산으로 내려가 정해진 횟수만큼 암반을 굴착합니다. 바닥의 숫자는 주변 8칸에 숨겨진 적 둥지 수를 뜻하며, 무료 스캔으로 의심스러운 칸을 미리 확인할 수 있습니다. 채굴 모드는 **암벽 발굴**(암반 겹을 깎아 매장물 발굴)과 **갱도 탐사**(열린 갱도 주변 탐색) 두 가지를 지원하며, `설정 > 원정` 탭에서 전환할 수 있습니다.
3. **제작과 성장:** 채굴한 철·구리·수정으로 상위 곡괭이와 전투 장비를 제작하고, 금으로 부대 슬롯을 확장하거나 새 카드를 모집합니다. 전투에서 얻은 조각으로는 영웅을 단련하거나 모집 후보를 다시 뽑을 수 있습니다.
4. **구역 제압:** 마지막 웨이브까지 거점을 지켜내면 해당 구역을 클리어하고 다음 구역이 해금됩니다. 총 7개 구역마다 고유한 전장 기믹(낙석 바위, 특성 패드, 갱도 터널, 용암 분출구, 광차 선로, 산성 늪과 가스통)과 보스 골렘이 등장합니다.

보스를 포함한 16종의 적은 공중을 나는 동굴박쥐, 근접해야 모습을 드러내는 암영 추적자, 방어 건물을 부수는 망치병, 저지선을 뛰어넘는 도약꾼 등 저마다 다른 공략을 요구합니다. 상세 규칙과 설계 문서는 [문서 목차](docs/README.md)에서 확인할 수 있습니다.

## 조작법

| 입력 | 동작 |
| --- | --- |
| 카드 클릭/탭 → 대상 위치 클릭/탭 | 카드 배치 및 마법 시전 |
| 카드를 전장으로 드래그 | 카드 배치 (전장 밖에서 놓으면 취소) |
| 배치된 건물·유닛 클릭 | 사거리 및 상태 확인, 수리·치료·철수 |
| `1` ~ `4` | 손패 카드 선택 |
| `Space` | 웨이브 시작 / 일시정지 및 재개 |
| `Esc` | 선택 및 팝업 닫기, 전투 일시정지 |
| `M` | 정비 중 지상 / 지하 화면 전환 |
| `Tab` / `Shift+Tab` / `Enter` | UI 버튼 포커스 이동 및 실행 |
| `S` / `V` | 효과음 음소거 / 볼륨 조절 |
| `+` / `-` / `0`, 마우스 휠 | 전장 화면 확대 / 축소 / 기본 배율 |
| 지하 타일 클릭 | 해당 타일 굴착 또는 스캔 |

## 사용한 ThorVG 기능

SDL3 + C++17 코드를 WebAssembly로 빌드하고, ThorVG(`SwCanvas`, 소프트웨어 렌더러)로 그린 픽셀 버퍼를 화면에 출력합니다.

| 기능 | 활용 영역 | 관련 코드 |
| --- | --- | --- |
| Lottie (`LottieAnimation`, 마커 구간) | 적 16종의 대기·이동·공격·사망 모션 클립, 시작 화면 ThorVG 스플래시 | [Renderer.cpp](src/engine/render/Renderer.cpp), [AnimationRendering.cpp](src/engine/render/AnimationRendering.cpp), [Animation.cpp](src/engine/animation/Animation.cpp) |
| Lottie 프레임 캐싱 | 동일 애니메이션 프레임을 한 번만 래스터화해 다수의 개체가 공유 | [AnimationFrameBaker.cpp](src/engine/render/AnimationFrameBaker.cpp), [AnimationCache.cpp](src/engine/render/AnimationCache.cpp) |
| `Shape` · `Scene` | 지형, 타워, 유닛, 사거리 표시, HUD 등 전 영역 벡터 드로잉 | [Renderer.cpp](src/engine/render/Renderer.cpp), [Battlefield.cpp](src/game/presentation/Battlefield.cpp) |
| `LinearGradient` · `RadialGradient` | UI 패널과 지층 음영, 보스 오라 및 용암 분출구 광원 효과 | [Renderer.cpp](src/engine/render/Renderer.cpp) |
| 점선 스트로크 (`strokeDash`) | 용암 분출구 피해 범위 및 보스 기절 예고 표시 | [Renderer.cpp](src/engine/render/Renderer.cpp), [TerrainArt.h](src/game/presentation/TerrainArt.h) |
| `Picture` (SVG) | 채굴 화면의 광석, 도구, 유물 아이콘 | [content/art/ore-gold.svg](content/art/ore-gold.svg) 외 |
| `Text` + TTF 로더 | 다국어 UI 텍스트 렌더링 (Noto Sans KR 서브셋 폰트) | [Renderer.cpp](src/engine/render/Renderer.cpp) |
| 클립 레이어 · 불투명도 | 패널 내부 클리핑 및 스크롤 영역 처리 | [Renderer.cpp](src/engine/render/Renderer.cpp) |
| 배경 레이어 캐시 | 정적인 지형과 배경 레이어를 재사용해 프레임 비용 절감 | [frame-budget.md](docs/frame-budget.md) |

적 캐릭터 16종의 Lottie 애니메이션은 [tools/make_assets.py](tools/make_assets.py)로 생성한 자체 제작 에셋입니다.

## 빌드와 실행

빌드에는 Python 3.12+, C++17 컴파일러, Meson, Ninja, pkg-config가 필요하며, 웹 빌드에는 Emscripten SDK(6.0.0 기준 검증)가 추가로 필요합니다. 자세한 환경 구성은 [실행 안내](docs/archive/implementation-v0.1.md)를 참고하세요.

~~~sh
python3 tools/build.py native
./build/native/thorium

python3 tools/build.py web
python3 -m http.server 8317 --bind 127.0.0.1 --directory dist
~~~

- 테스트 실행: `python3 -m unittest discover -s tests -p 'test_*.py' -v`
- `main` 브랜치에 푸시하면 [GitHub Actions](.github/workflows/deploy-pages.yml)에서 네이티브 테스트, WASM 빌드, 브라우저 E2E 테스트를 검증한 뒤 GitHub Pages에 자동 배포합니다.

## 팀

| 역할 | 담당 |
| --- | --- |
| 기획 · 개발 | [@hd1534](https://github.com/hd1534) |

## Credits

| 항목 | 출처 및 라이선스 |
| --- | --- |
| ThorVG 1.0.0 | [thorvg/thorvg](https://github.com/thorvg/thorvg), MIT |
| "Powered by ThorVG" 스플래시 Lottie | ThorVG 공식 에셋 원본 사용 ([content/animations/powered-by-thorvg.json](content/animations/powered-by-thorvg.json)) |
| SDL 3.4.18 | [libsdl-org/SDL](https://github.com/libsdl-org/SDL), zlib |
| Noto Sans KR | [Google Fonts](https://github.com/google/fonts/tree/main/ofl/notosanskr), SIL OFL 1.1 |
| dr_mp3 | [mackron/dr_libs](https://github.com/mackron/dr_libs), Public Domain 또는 MIT-0 |
| 배경음악 (`content/audio/bgm.mp3`) | Google Flow Music으로 제작한 전용 음원 (Flow Music 이용 약관 적용, MIT 라이선스 제외) |
| 효과음 | 런타임 파형 합성으로 구현한 자체 효과음 |
| 적 Lottie · 타워·유닛·UI 그래픽 · 광석 SVG | 자체 제작 (생성 스크립트: [make_assets.py](tools/make_assets.py), [make_art.py](tools/make_art.py)) |

외부 의존성의 전체 목록과 고정 버전은 [THIRD_PARTY.md](THIRD_PARTY.md)와 [dependencies.lock.json](dependencies.lock.json)에서 확인할 수 있습니다.

## License

소스 코드는 [MIT License](LICENSE)로 배포됩니다. 외부 라이브러리와 공식 스플래시 에셋, 배경음악은 각각의 원 라이선스를 따릅니다.

## 문서

기획·설계·구현 문서와 변경 내역은 [docs/README.md](docs/README.md)에 정리되어 있습니다.
