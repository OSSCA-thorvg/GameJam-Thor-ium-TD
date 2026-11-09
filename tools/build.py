"""Build the same SDL/ThorVG application for native desktop and WebAssembly."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT: Path = Path(__file__).resolve().parents[1]
DEPS: Path = ROOT / '.deps'
TOOLS: Path = ROOT / '.tools'
SDL_VERSION: str = '3.4.18'
THORVG_COMMIT: str = '12132e8ebe329bf549a3f3248b8e8cef295e5c94'
CMAKE_VERSION: str = '4.1.2'


def run(arguments: list[str], *, environment: dict[str, str] | None = None) -> None:
    """Execute an argument vector without shell interpolation or global installs."""
    print('>', ' '.join(arguments), flush=True)
    subprocess.run(arguments, cwd=ROOT, env=environment, check=True)


def download(url: str, destination: Path) -> None:
    """Download one dependency atomically and retain a SHA-256 audit manifest."""
    lock_path: Path = ROOT / 'dependencies.lock.json'
    locks: dict[str, dict[str, str]] = json.loads(lock_path.read_text(encoding='utf-8')).get('downloads', {}) if lock_path.exists() else {}
    expected: str | None = locks.get(destination.name, {}).get('sha256')
    if destination.is_file():
        if expected and hashlib.sha256(destination.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f'Cached dependency hash mismatch: {destination}')
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial: Path = destination.with_suffix(destination.suffix + '.partial')
    run(['curl', '--fail', '--location', '--retry', '3', '--silent', '--show-error', url, '-o', str(partial)])
    if expected and hashlib.sha256(partial.read_bytes()).hexdigest() != expected:
        partial.unlink()
        raise RuntimeError(f'Download hash mismatch: {url}; review the dependency lock before updating.')
    partial.replace(destination)
    manifest_path: Path = DEPS / 'downloads.json'
    manifest: dict[str, dict[str, str]] = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    manifest[destination.name] = {'url': url, 'sha256': hashlib.sha256(destination.read_bytes()).hexdigest()}
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


def unpack(archive: Path, destination: Path) -> None:
    """Extract a pinned archive without permitting filesystem traversal."""
    if destination.is_dir():
        return
    temporary: Path = DEPS / (destination.name + '-extract')
    if temporary.exists():
        shutil.rmtree(temporary)
    temporary.mkdir(parents=True)
    with tarfile.open(archive) as source:
        source.extractall(temporary, filter='data')
    entries: list[Path] = list(temporary.iterdir())
    if len(entries) != 1 or not entries[0].is_dir():
        raise RuntimeError(f'Unexpected archive layout: {archive}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    entries[0].rename(destination)
    temporary.rmdir()


def prepare() -> Path:
    """Prepare versioned sources and a project-local CMake installation."""
    DEPS.mkdir(exist_ok=True)
    cmake: Path = TOOLS / 'venv' / ('Scripts/cmake.exe' if os.name == 'nt' else 'bin/cmake')
    if not cmake.exists():
        run([sys.executable, '-m', 'venv', str(TOOLS / 'venv')])
        python: Path = cmake.parent / ('python.exe' if os.name == 'nt' else 'python')
        run([str(python), '-m', 'pip', 'install', '--disable-pip-version-check', '--cache-dir', str(TOOLS / 'pip-cache'), f'cmake=={CMAKE_VERSION}'])
    python: Path = cmake.parent / ('python.exe' if os.name == 'nt' else 'python')
    if subprocess.run([str(python), '-c', 'import fontTools'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        run([str(python), '-m', 'pip', 'install', '--disable-pip-version-check', '--cache-dir', str(TOOLS / 'pip-cache'), 'fonttools==4.60.1'])
    sdl_archive: Path = DEPS / f'SDL-{SDL_VERSION}.tar.gz'
    download(f'https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-{SDL_VERSION}', sdl_archive)
    unpack(sdl_archive, DEPS / 'SDL')
    thorvg: Path = ROOT / 'subprojects/thorvg'
    if not thorvg.exists():
        thorvg_archive: Path = DEPS / 'thorvg.tar.gz'
        download(f'https://codeload.github.com/thorvg/thorvg/tar.gz/{THORVG_COMMIT}', thorvg_archive)
        unpack(thorvg_archive, thorvg)
    fonts: Path = ROOT / 'content/fonts'
    download('https://raw.githubusercontent.com/google/fonts/main/ofl/notosanskr/NotoSansKR%5Bwght%5D.ttf', DEPS / 'NotoSansKR-variable.ttf')
    download('https://raw.githubusercontent.com/google/fonts/main/ofl/notosanskr/OFL.txt', fonts / 'OFL.txt')
    if not (DEPS / 'font-generated-v1').exists() or not (fonts / 'NotoSansKR.ttf').exists():
        run([str(python), str(ROOT / 'tools/make_font.py')])
        (DEPS / 'font-generated-v1').write_text('500\n', encoding='utf-8')
    lock: dict = json.loads((ROOT / 'dependencies.lock.json').read_text())
    for name, relative in [('dr_mp3.h', 'third_party/dr_mp3/dr_mp3.h'), ('dr_libs.txt', 'content/licenses/dr_libs.txt')]:
        download(lock['downloads'][name]['url'], ROOT / relative)
    licenses: Path = ROOT / 'content/licenses'
    licenses.mkdir(exist_ok=True)
    shutil.copy2(DEPS / 'SDL/LICENSE.txt', licenses / 'SDL.txt')
    shutil.copy2(thorvg / 'LICENSE', licenses / 'ThorVG.txt')
    if not (ROOT / 'content/animations/oreling.json').exists():
        run([sys.executable, str(ROOT / 'tools/make_assets.py')])
    return cmake


def sdk_environment() -> dict[str, str]:
    """Locate an existing SDK and keep Emscripten caches inside this project."""
    environment: dict[str, str] = dict(os.environ)
    sdk: Path = Path(environment.get('EMSDK', ROOT.parents[2] / 'playground/emsdk'))
    emscripten: Path = sdk / 'upstream/emscripten'
    if not (emscripten / 'emcc').exists():
        raise RuntimeError('Set EMSDK to an installed Emscripten SDK before building the web target.')
    environment['EMSDK'] = str(sdk)
    environment['EM_CONFIG'] = str(sdk / '.emscripten')
    environment['EM_CACHE'] = str(DEPS / 'em-cache')
    environment['EMCC_SKIP_SANITY_CHECK'] = '1'
    environment['PATH'] = str(emscripten) + os.pathsep + environment['PATH']
    return environment


def build(target: str) -> None:
    """Compile SDL, then isolated rules/engine targets and the final application."""
    cmake: Path = prepare()
    environment: dict[str, str] = sdk_environment() if target == 'web' else dict(os.environ)
    environment['PATH'] = str(cmake.parent) + os.pathsep + environment['PATH']
    prefix: Path = DEPS / f'sdl-{target}'
    sdl_build: Path = DEPS / f'sdl-build-{target}'
    if not (prefix / 'include/SDL3/SDL.h').exists() or not (prefix / 'audio-enabled-v1').exists():
        configure: list[str] = [str(cmake), '-S', str(DEPS / 'SDL'), '-B', str(sdl_build), '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_INSTALL_PREFIX={prefix}', '-DSDL_SHARED=OFF', '-DSDL_STATIC=ON',
            '-DSDL_TESTS=OFF', '-DSDL_TEST_LIBRARY=OFF', '-DSDL_EXAMPLES=OFF', '-DSDL_AUDIO=ON',
            '-DSDL_JOYSTICK=OFF', '-DSDL_HAPTIC=OFF', '-DSDL_SENSOR=OFF', '-DSDL_CAMERA=OFF',
            '-DSDL_DIALOG=OFF', '-DSDL_TRAY=OFF', '-DSDL_GPU=OFF']
        if target == 'web':
            configure.insert(0, str(Path(environment['EMSDK']) / 'upstream/emscripten/emcmake'))
        run(configure, environment=environment)
        run([str(cmake), '--build', str(sdl_build), '--parallel', '8'], environment=environment)
        run([str(cmake), '--install', str(sdl_build)], environment=environment)
        (prefix / 'audio-enabled-v1').write_text('SDL_AUDIO=ON\n')
    environment['PKG_CONFIG_PATH'] = str(prefix / 'lib/pkgconfig')
    environment['PKG_CONFIG_LIBDIR'] = str(prefix / 'lib/pkgconfig')
    environment['CMAKE_PREFIX_PATH'] = str(prefix)
    build_dir: Path = ROOT / 'build' / target
    configure_meson: list[str] = ['meson', 'setup', str(build_dir), '--buildtype=release']
    if (build_dir / 'meson-private/coredata.dat').exists():
        configure_meson.append('--reconfigure')
    if target == 'web':
        configure_meson.extend(['--cross-file', str(ROOT / 'cross/wasm32.txt')])
    run(configure_meson, environment=environment)
    run(['meson', 'compile', '-C', str(build_dir), '-j', '8'], environment=environment)
    if target == 'native':
        run(['meson', 'test', '-C', str(build_dir), '--print-errorlogs'], environment=environment)
    else:
        checker: str | None = shutil.which('node', path=environment['PATH'])
        if checker:
            run([checker, '--check', str(build_dir / 'thorium.js')], environment=environment)
        output: Path = ROOT / 'dist'
        output.mkdir(exist_ok=True)
        for suffix in ('js', 'wasm', 'data'):
            shutil.copy2(build_dir / f'thorium.{suffix}', output / f'thorium.{suffix}')
        shutil.copy2(ROOT / 'web/index.html', output / 'index.html')


def main() -> None:
    """Dispatch a reproducible local dependency, native or web build."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=('prepare', 'native', 'web', 'all'))
    target: str = parser.parse_args().target
    if target == 'prepare':
        prepare()
    else:
        for item in ('native', 'web') if target == 'all' else (target,):
            build(item)


if __name__ == '__main__':
    main()
