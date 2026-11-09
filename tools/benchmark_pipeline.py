"""Measure synchronized combat and the shared app/SDL output pipeline in isolated processes."""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
from typing import Any
from zoneinfo import ZoneInfo

ROOT: Path = Path(__file__).resolve().parents[1]


def main() -> None:
    """Record device conditions, binary identities and CPU stages without parallel benchmark load."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=60, choices=range(3, 121), metavar='3..120')
    parser.add_argument('--video-driver', default='dummy')
    parser.add_argument('--audio-driver', default='dummy')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/benchmarks/frame-pipeline.json')
    arguments: argparse.Namespace = parser.parse_args()
    environment: dict[str, str] = dict(os.environ)
    environment['SDL_VIDEODRIVER'] = arguments.video_driver
    environment['SDL_AUDIODRIVER'] = arguments.audio_driver
    combat_binary: Path = ROOT / 'build/native/thorium-combat-benchmark'
    frame_binary: Path = ROOT / 'build/native/thorium-frame-benchmark'
    combat: subprocess.CompletedProcess[str] = subprocess.run(
        [str(combat_binary)], cwd=ROOT, text=True, capture_output=True, check=True,
    )
    records: list[dict[str, Any]] = []
    for density in (1, 1.5):
        for count in (100, 300, 1000):
            for mode in ('vector', 'cache'):
                result: subprocess.CompletedProcess[str] = subprocess.run(
                    [str(frame_binary), str(count), str(arguments.frames), mode, str(density)],
                    cwd=ROOT, env=environment, text=True, capture_output=True, check=True,
                )
                record: dict[str, Any] = json.loads(result.stdout)
                records.append(record)
                print(f'{density=} {count=} {mode=} work_p50={record["performance"]["p50"]["total"]:.3f} ms', flush=True)
    report: dict[str, Any] = {
        'measured_at': datetime.now(ZoneInfo('Asia/Seoul')).isoformat(),
        'host': platform.platform(),
        'binaries': {binary.name: hashlib.sha256(binary.read_bytes()).hexdigest()
                     for binary in (combat_binary, frame_binary)},
        'asset_sha256': hashlib.sha256((ROOT / 'content/animations/oreling.json').read_bytes()).hexdigest(),
        'conditions': {
            'build': 'release; ThorVG software single thread', 'viewport': [1200, 800],
            'warmup_frames': 3, 'steady_frames': arguments.frames,
            'scope': 'one fixed 60Hz tick + presentation/feedback + full UI + CPU raster + PCM pump + SDL upload/present',
            'population': 'fixture-only enemies spread along the route, three roles, six alternating gun/cannon towers; no leaks or defeats',
            'pacing': 'VSync requested off; pace fast frames to 60Hz after measurement so the PCM device can consume queued data',
            'total': 'CPU iteration work including output calls, excluding initialization, fixture construction and post-frame pacing',
            'interval': 'wall-clock cadence including prior pacing; neither statistic measures physical audio latency or GPU completion',
            'drivers': {'video': arguments.video_driver, 'audio': arguments.audio_driver},
            'dummy_limit': 'dummy video uses software SDL output; results do not establish browser, hardware GPU or mobile FPS',
            'percentiles': 'each stage is summarized independently; summing stage p95 values is not total p95',
            'combat': '40 forced synchronized volleys; enemies overlap and eight towers share a fixture position; CPU Run::advance only',
        },
        'combat': json.loads(combat.stdout), 'pipeline': records,
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
