# 빌드·실행·테스트

프로젝트 루트에서 실행합니다. Python 3.12 이상, C++17 컴파일러, Meson, Ninja, pkg-config가 필요합니다. CMake·fontTools는 프로젝트의 `.tools/venv/`에 설치하고, SDL·ThorVG 소스와 빌드는 `.deps/`·`subprojects/`·`build/`에 둡니다. 전역 패키지는 설치하지 않습니다. 처음 실행할 때 의존성을 내려받습니다.

## 네이티브

~~~sh
python3 tools/build.py native
./build/native/thorium
~~~

빌드 명령은 빌드 뒤 Meson 테스트도 실행합니다. 네이티브 실행 파일은 개발 폴더의 `content/`를 읽습니다. Windows용 소스 경계는 있지만 Windows 빌드·실행은 검증하지 않았습니다.

## 웹

Emscripten SDK가 더 필요합니다. `EMSDK` 환경변수를 지정하거나 워크스페이스의 `ThorVG/playground/emsdk`를 자동으로 찾습니다. 검증한 SDK는 6.0.0입니다.

~~~sh
python3 tools/build.py web
python3 -m http.server 8317 --bind 127.0.0.1 --directory dist
~~~

[로컬 실행본](http://127.0.0.1:8317/)을 엽니다. 게임·HUD·손패·창은 네이티브와 같은 C++ 코드로 그리고, HTML은 캔버스·로딩·오류 표시만 맡습니다. `content/` 전체가 데이터 패키지로 들어가므로 콘텐츠를 고친 뒤에는 다시 빌드합니다.

## 테스트

~~~sh
meson test -C build/native --print-errorlogs
python3 -m unittest discover -s tests -p 'test_*.py'
~~~

- Meson 테스트: 규칙·입력·캠페인·채굴·튜토리얼·저장·오디오·애니메이션·[다국어](i18n.md) 검사, SDL dummy 드라이버 렌더링, 두 채굴 방식의 밸런스 곡선(`thorium-balance-harness --check`).
- Python 테스트: 문서 링크·구조, 소스 의존 방향, 다국어 키.

브라우저 테스트는 웹 빌드를 위 명령으로 띄워 둔 상태에서 실행합니다. 실제 마우스·터치 입력과 읽기 전용 진단 상태(`_thorium_debug_state`, `_thorium_debug_ui`)만 씁니다.

~~~sh
npm ci
npx playwright install chromium
npm run test:browser
~~~

시나리오 파일은 [tests/browser](../tests/browser/run.mjs)에 있고, `node tests/browser/run.mjs frontend.js`처럼 일부만 돌릴 수 있습니다. `main`에 푸시하면 [GitHub Actions](../.github/workflows/deploy-pages.yml)가 같은 검사를 통과시킨 뒤 Pages에 배포합니다.
