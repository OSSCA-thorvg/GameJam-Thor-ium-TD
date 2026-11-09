"""Generate the prototype's original vector Lottie creature without external artwork."""

from __future__ import annotations
import json
from pathlib import Path
from typing import Any


def ellipse(name: str, position: list[int], size: list[int], color: list[float]) -> dict[str, Any]:
    """Create one named vector group with a solid ellipse and an identity transform."""
    return {'ty':'gr','nm':name,'it':[
        {'ty':'el','p':{'a':0,'k':position},'s':{'a':0,'k':size},'d':1,'nm':name},
        {'ty':'fl','c':{'a':0,'k':color},'o':{'a':0,'k':100},'r':1},
        {'ty':'tr','p':{'a':0,'k':[0,0]},'a':{'a':0,'k':[0,0]},'s':{'a':0,'k':[100,100]},'r':{'a':0,'k':0},'o':{'a':0,'k':100},'sk':{'a':0,'k':0},'sa':{'a':0,'k':0}}
    ]}


def main() -> None:
    """Write a small looping ore creature made entirely from original ellipse shapes."""
    shapes: list[dict[str, Any]] = [
        ellipse('antenna',[50,22],[15,20],[0.45,0.76,0.73,1]),
        ellipse('body',[50,57],[68,58],[0.87,0.43,0.31,1]),
        ellipse('shine',[35,46],[16,12],[0.98,0.66,0.43,1]),
        ellipse('eye-left',[40,55],[7,12],[0.16,0.23,0.24,1]),
        ellipse('eye-right',[61,55],[7,12],[0.16,0.23,0.24,1]),
        ellipse('cheek-left',[28,66],[9,5],[0.96,0.63,0.51,1]),
        ellipse('cheek-right',[72,66],[9,5],[0.96,0.63,0.51,1]),
    ]
    easing: dict[str, Any] = {'i':{'x':[0.6,0.6],'y':[1,1]},'o':{'x':[0.4,0.4],'y':[0,0]}}
    animation: dict[str, Any] = {'v':'5.7.4','fr':30,'ip':0,'op':45,'w':100,'h':100,'nm':'Original oreling by Thor-ium TD','ddd':0,'assets':[], 'layers':[
        {'ddd':0,'ind':1,'ty':4,'nm':'Oreling','sr':1,'ks':{
            'o':{'a':0,'k':100},'r':{'a':0,'k':0},'p':{'a':0,'k':[50,80,0]},'a':{'a':0,'k':[50,80,0]},
            's':{'a':1,'k':[{'t':0,'s':[100,100,100],'e':[106,93,100],**easing},{'t':22,'s':[106,93,100],'e':[100,100,100],**easing},{'t':45,'s':[100,100,100]}]}
        },'ao':0,'shapes':list(reversed(shapes)),'ip':0,'op':45,'st':0,'bm':0}
    ]}
    destination: Path = Path(__file__).resolve().parents[1] / 'content/animations/oreling.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(animation,separators=(',',':'))+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
