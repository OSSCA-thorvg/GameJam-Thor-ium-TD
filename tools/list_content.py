"""List runtime content as link dependencies, including newly added optional MP3 files."""
from pathlib import Path
import hashlib


def main() -> None:
    """Print a content signature followed by filenames, tracking additions and removals."""
    content: Path = Path(__file__).resolve().parents[1] / 'content'
    assets: list[Path] = [asset for asset in sorted(content.rglob('*')) if asset.is_file()]
    records: list[str] = []
    asset: Path
    for asset in assets:
        records.append(str(asset.relative_to(content)) + ':' + hashlib.sha256(asset.read_bytes()).hexdigest())
    signature: str = hashlib.sha256('\n'.join(records).encode('utf-8')).hexdigest()
    print(signature)
    for asset in assets:
        print(asset)


if __name__ == '__main__':
    main()
