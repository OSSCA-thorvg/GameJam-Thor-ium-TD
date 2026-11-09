"""Create a readable, renamed OFL font instance with Korean and game punctuation."""

from __future__ import annotations
from pathlib import Path
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


def main() -> None:
    """Instantiate weight 500, subset supported UI scripts, and rename the derivative."""
    root: Path = Path(__file__).resolve().parents[1]
    font: TTFont = TTFont(root / '.deps/NotoSansKR-variable.ttf')
    font = instantiateVariableFont(font, {'wght':500}, inplace=True)
    options: subset.Options = subset.Options()
    options.layout_features = ['*']
    converter: subset.Subsetter = subset.Subsetter(options=options)
    converter.populate(unicodes=list(range(0x20,0x100))+list(range(0x1100,0x1200))+list(range(0x2000,0x2200))+list(range(0x3000,0x3190))+list(range(0xAC00,0xD7B0)))
    converter.subset(font)
    for record in font['name'].names:
        replacement: str | None = {1:'Thorium UI',2:'Medium',3:'ThoriumUI-Medium-1',4:'Thorium UI Medium',6:'ThoriumUI-Medium'}.get(record.nameID)
        if replacement is not None:
            record.string = replacement.encode(record.getEncoding())
    font.save(root / 'content/fonts/NotoSansKR.ttf')


if __name__ == '__main__':
    main()
