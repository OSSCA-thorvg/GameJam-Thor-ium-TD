"""Compare animation backends in isolated native processes with recorded rendering conditions."""
from __future__ import annotations

import argparse
from datetime import datetime
import json
import hashlib
from pathlib import Path
import platform
import subprocess
from typing import Any
from zoneinfo import ZoneInfo

ROOT: Path = Path(__file__).resolve().parents[1]


def main() -> None:
    """Run equal-pose and continuous-reference cases and save the raw measurements."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=30)
    parser.add_argument('--fps', default='24,30')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/benchmarks/animation-cache.json')
    arguments: argparse.Namespace = parser.parse_args()
    fps_values: list[int] = [int(value) for value in arguments.fps.split(',')]
    records: list[dict[str, Any]] = []
    executable: Path = ROOT / 'build/native/thorium-animation-benchmark'
    for density in (1, 1.5):
        for count in (100, 300, 1000):
            cases: list[tuple[str, int]] = [('continuous', 30)]
            cases.extend((mode, fps) for fps in fps_values for mode in ('vector', 'cache'))
            for mode, fps in cases:
                result: subprocess.CompletedProcess[str] = subprocess.run(
                    [str(executable), str(count), mode, str(arguments.frames), str(density), str(fps)],
                    cwd=ROOT, capture_output=True, text=True, check=True,
                )
                record: dict[str, Any] = json.loads(result.stdout)
                records.append(record)
                print(f'{density=} {count=} {mode=} {fps=} median={record["median_ms"]:.3f} ms', flush=True)
    cpu: str = platform.processor() or platform.machine()
    if platform.system() == 'Darwin':
        cpu_result: subprocess.CompletedProcess[str] = subprocess.run(
            ['sysctl', '-n', 'machdep.cpu.brand_string'], capture_output=True, text=True, check=False,
        )
        cpu = cpu_result.stdout.strip() or cpu
    report: dict[str, Any] = {
        'measured_at': datetime.now(ZoneInfo('Asia/Seoul')).isoformat(),
        'host': platform.platform(), 'cpu': cpu,
        'binary_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
        'asset_sha256': hashlib.sha256((ROOT / 'content/animations/oreling.json').read_bytes()).hexdigest(),
        'conditions': {
            'backend': 'ThorVG software, single thread, release build',
            'viewport': [1280, 720], 'sprite_logical_size': [48, 48],
            'warmup_frames': 3, 'steady_frames': arguments.frames,
            'cache_budget_bytes': 32 * 1024 * 1024, 'padding_fraction': .125,
            'scope': 'sampling + scene submission + CPU drawing; excludes game simulation, UI, SDL upload and audio',
            'vector': 'same source frames as the cache; continuous is the original fractional-frame reference',
            'memory': 'pixel_bytes is cache-owned pixels; peak_rss_bytes is each isolated process peak',
        },
        'records': records,
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
