"""Generate the prototype's original vector Lottie creatures without external artwork."""

from __future__ import annotations
import json
from pathlib import Path
from typing import Any

Color = list[float]


def transform() -> dict[str, Any]:
    """Return the identity transform that closes every shape group."""
    return {'ty':'tr','p':{'a':0,'k':[0,0]},'a':{'a':0,'k':[0,0]},'s':{'a':0,'k':[100,100]},'r':{'a':0,'k':0},'o':{'a':0,'k':100},'sk':{'a':0,'k':0},'sa':{'a':0,'k':0}}


def fill(color: Color) -> dict[str, Any]:
    """Return an opaque solid fill item."""
    return {'ty':'fl','c':{'a':0,'k':color},'o':{'a':0,'k':100},'r':1}


def ellipse(name: str, position: list[int], size: list[int], color: Color) -> dict[str, Any]:
    """Create one named vector group with a solid ellipse and an identity transform."""
    return {'ty':'gr','nm':name,'it':[
        {'ty':'el','p':{'a':0,'k':position},'s':{'a':0,'k':size},'d':1,'nm':name},
        fill(color),
        transform()
    ]}


def outline(points: list[list[float]], closed: bool) -> dict[str, Any]:
    """Create a straight-segment Lottie path through the given points."""
    corners: list[list[float]] = [[0, 0] for _ in points]
    return {'ty':'sh','ks':{'a':0,'k':{'i':corners,'o':corners,'v':points,'c':closed}}}


def polygon(name: str, points: list[list[float]], color: Color) -> dict[str, Any]:
    """Create one named filled polygon group."""
    return {'ty':'gr','nm':name,'it':[outline(points, True), fill(color), transform()]}


def stroke(name: str, points: list[list[float]], width: float, color: Color) -> dict[str, Any]:
    """Create one named open polyline group with round caps."""
    return {'ty':'gr','nm':name,'it':[
        outline(points, False),
        {'ty':'st','c':{'a':0,'k':color},'o':{'a':0,'k':100},'w':{'a':0,'k':width},'lc':2,'lj':2},
        transform()
    ]}


INK: Color = [0.16, 0.23, 0.24, 1]
WHITE: Color = [0.98, 0.97, 0.92, 1]


def eyes(left: list[int], right: list[int], size: list[int], color: Color = INK) -> list[dict[str, Any]]:
    """Return a symmetric pair of eyes."""
    return [ellipse('eye-left', left, size, color), ellipse('eye-right', right, size, color)]


def oreling() -> list[dict[str, Any]]:
    """Original ore creature used by the common enemy."""
    return [
        ellipse('antenna',[50,22],[15,20],[0.45,0.76,0.73,1]),
        ellipse('body',[50,57],[68,58],[0.87,0.43,0.31,1]),
        ellipse('shine',[35,46],[16,12],[0.98,0.66,0.43,1]),
        ellipse('eye-left',[40,55],[7,12],[0.16,0.23,0.24,1]),
        ellipse('eye-right',[61,55],[7,12],[0.16,0.23,0.24,1]),
        ellipse('cheek-left',[28,66],[9,5],[0.96,0.63,0.51,1]),
        ellipse('cheek-right',[72,66],[9,5],[0.96,0.63,0.51,1]),
    ]


def dashling() -> list[dict[str, Any]]:
    """Lean yellow runner with swept-back ears."""
    yellow: Color = [0.97, 0.79, 0.24, 1]
    return [
        polygon('ear-back', [[42,44],[18,18],[34,50]], [0.85,0.6,0.15,1]),
        polygon('ear-front', [[58,42],[40,14],[50,48]], [0.9,0.66,0.18,1]),
        ellipse('body',[50,62],[58,46],yellow),
        stroke('stripe-1', [[30,56],[38,70]], 4, [0.85,0.55,0.12,1]),
        stroke('stripe-2', [[40,52],[47,70]], 4, [0.85,0.55,0.12,1]),
        ellipse('eye-white',[62,58],[18,18],WHITE),
        ellipse('pupil',[66,58],[8,11],INK),
        ellipse('cheek',[70,70],[8,5],[0.99,0.6,0.45,1]),
    ]


def ironback() -> list[dict[str, Any]]:
    """Wide steel-plated heavy with a glowing visor."""
    return [
        ellipse('body',[50,62],[80,50],[0.47,0.54,0.62,1]),
        ellipse('belly',[50,72],[54,24],[0.66,0.72,0.77,1]),
        ellipse('plate',[50,46],[72,36],[0.27,0.33,0.42,1]),
        polygon('visor', [[30,48],[70,48],[66,56],[34,56]], [0.12,0.15,0.2,1]),
        ellipse('eye-left',[41,52],[9,5],[1,0.6,0.25,1]),
        ellipse('eye-right',[59,52],[9,5],[1,0.6,0.25,1]),
        ellipse('rivet-left',[24,52],[6,6],[0.8,0.84,0.88,1]),
        ellipse('rivet-right',[76,52],[6,6],[0.8,0.84,0.88,1]),
        ellipse('rivet-top',[50,34],[6,6],[0.8,0.84,0.88,1]),
    ]


def pebbling() -> list[dict[str, Any]]:
    """Small spiky violet pebble with large eyes."""
    spike: Color = [0.34,0.25,0.5,1]
    return [
        polygon('spike-left', [[28,58],[30,34],[42,50]], spike),
        polygon('spike-mid', [[42,48],[50,26],[58,48]], spike),
        polygon('spike-right', [[58,50],[70,34],[72,58]], spike),
        ellipse('body',[50,64],[54,44],[0.56,0.43,0.74,1]),
        ellipse('shine',[38,56],[12,8],[0.75,0.64,0.9,1]),
        ellipse('eye-left',[42,64],[13,14],WHITE),
        ellipse('eye-right',[59,64],[13,14],WHITE),
        ellipse('pupil-left',[43,65],[6,8],INK),
        ellipse('pupil-right',[60,65],[6,8],INK),
    ]


def shieldling() -> list[dict[str, Any]]:
    """Teal soldier with a dark helmet; its breakable shield is drawn by the game."""
    return [
        polygon('crest', [[44,26],[50,14],[56,26]], [0.86,0.32,0.28,1]),
        ellipse('body',[50,62],[62,50],[0.29,0.6,0.62,1]),
        ellipse('helmet',[50,42],[60,34],[0.2,0.28,0.36,1]),
        polygon('brim', [[20,50],[80,50],[74,56],[26,56]], [0.14,0.2,0.27,1]),
        *eyes([41,62],[59,62],[8,9]),
        ellipse('belt',[50,78],[46,8],[0.2,0.38,0.42,1]),
    ]


def mender() -> list[dict[str, Any]]:
    """Pale green healer with a sprout and a white cross."""
    leaf: Color = [0.3,0.68,0.36,1]
    return [
        stroke('stem', [[50,36],[50,22]], 4, [0.28,0.55,0.3,1]),
        polygon('leaf-left', [[50,24],[30,14],[38,28]], leaf),
        polygon('leaf-right', [[50,24],[70,12],[62,28]], leaf),
        ellipse('body',[50,60],[64,54],[0.66,0.9,0.72,1]),
        ellipse('belly',[50,70],[36,26],WHITE),
        polygon('cross-v', [[46,60],[54,60],[54,80],[46,80]], [0.9,0.32,0.4,1]),
        polygon('cross-h', [[40,66],[60,66],[60,74],[40,74]], [0.9,0.32,0.4,1]),
        *eyes([40,50],[60,50],[7,8]),
    ]


def splitter() -> list[dict[str, Any]]:
    """Three-lobed magenta crystal with visible fracture lines."""
    lobe: Color = [0.79,0.34,0.62,1]
    crack: Color = [0.33,0.1,0.28,1]
    return [
        ellipse('lobe-left',[32,64],[40,38],lobe),
        ellipse('lobe-right',[68,64],[40,38],lobe),
        ellipse('lobe-top',[50,44],[44,40],[0.86,0.42,0.7,1]),
        ellipse('core',[50,62],[26,22],[1,0.72,0.9,1]),
        stroke('crack-left', [[50,30],[44,44],[50,52],[40,66],[34,80]], 3, crack),
        stroke('crack-right', [[50,52],[62,60],[60,72],[68,82]], 3, crack),
        *eyes([42,46],[58,46],[7,9]),
    ]


def burrower() -> list[dict[str, Any]]:
    """Brown digger with a steel drill on its head and front claws."""
    claw: Color = [0.9,0.86,0.78,1]
    return [
        polygon('drill', [[34,44],[66,44],[50,8]], [0.72,0.75,0.8,1]),
        stroke('thread-1', [[38,38],[62,32]], 3, [0.45,0.48,0.54,1]),
        stroke('thread-2', [[42,28],[58,22]], 3, [0.45,0.48,0.54,1]),
        ellipse('body',[50,64],[66,46],[0.62,0.41,0.24,1]),
        ellipse('band',[50,58],[66,10],[0.48,0.3,0.17,1]),
        ellipse('snout',[50,70],[22,14],[0.92,0.62,0.55,1]),
        *eyes([38,62],[62,62],[7,7]),
        polygon('claw-left', [[18,74],[30,70],[24,84]], claw),
        polygon('claw-right', [[82,74],[70,70],[76,84]], claw),
    ]


def sapper() -> list[dict[str, Any]]:
    """Hunched charcoal sapper with a lamp helmet and a lit dynamite bundle on its back."""
    dynamite: Color = [0.86,0.2,0.18,1]
    return [
        polygon('stick-left', [[18,30],[28,30],[28,62],[18,62]], dynamite),
        polygon('stick-right', [[28,26],[38,26],[38,60],[28,60]], [0.95,0.3,0.24,1]),
        polygon('strap', [[16,42],[40,42],[40,47],[16,47]], [0.36,0.24,0.14,1]),
        stroke('fuse', [[33,26],[36,16],[44,12]], 2.5, [0.3,0.22,0.16,1]),
        ellipse('spark',[45,11],[9,9],[1,0.82,0.3,1]),
        ellipse('body',[56,64],[58,46],[0.33,0.33,0.36,1]),
        ellipse('belly',[60,72],[34,20],[0.5,0.48,0.46,1]),
        ellipse('helmet',[60,44],[46,26],[0.93,0.69,0.18,1]),
        polygon('brim', [[36,48],[84,48],[80,53],[40,53]], [0.72,0.5,0.12,1]),
        ellipse('lamp',[74,40],[12,10],[1,0.97,0.75,1]),
        *eyes([54,60],[70,60],[7,8], [0.98,0.9,0.6,1]),
        polygon('hand', [[80,70],[92,66],[90,78]], [0.5,0.48,0.46,1]),
    ]


def bat() -> list[dict[str, Any]]:
    """Cave bat with wide membrane wings, pointed ears and amber eyes."""
    wing: Color = [0.36,0.3,0.46,1]
    rib: Color = [0.24,0.2,0.32,1]
    return [
        polygon('wing-left', [[44,50],[8,30],[12,46],[4,56],[18,58],[22,68],[42,62]], wing),
        polygon('wing-right', [[56,50],[92,30],[88,46],[96,56],[82,58],[78,68],[58,62]], wing),
        stroke('rib-left', [[42,54],[12,40]], 2, rib),
        stroke('rib-right', [[58,54],[88,40]], 2, rib),
        polygon('ear-left', [[40,40],[38,24],[48,38]], [0.46,0.38,0.56,1]),
        polygon('ear-right', [[60,40],[62,24],[52,38]], [0.46,0.38,0.56,1]),
        ellipse('body',[50,54],[30,34],[0.48,0.4,0.58,1]),
        ellipse('belly',[50,60],[16,18],[0.66,0.58,0.72,1]),
        *eyes([44,48],[56,48],[7,7], [1,0.78,0.3,1]),
        polygon('fang-left', [[46,58],[48,64],[49,58]], WHITE),
        polygon('fang-right', [[51,58],[52,64],[54,58]], WHITE),
    ]


def stalker() -> list[dict[str, Any]]:
    """Hooded shadow with a tattered cloak and two violet eyes."""
    cloak: Color = [0.2,0.17,0.3,1]
    return [
        polygon('cloak', [[50,14],[74,40],[80,86],[68,80],[60,88],[50,80],[40,88],[32,80],[20,86],[26,40]], cloak),
        polygon('hood-shade', [[50,22],[66,42],[50,52],[34,42]], [0.08,0.07,0.14,1]),
        polygon('fold-left', [[34,52],[30,82],[38,78]], [0.27,0.23,0.4,1]),
        polygon('fold-right', [[66,52],[70,82],[62,78]], [0.27,0.23,0.4,1]),
        ellipse('eye-left',[43,40],[8,5],[0.78,0.56,1,1]),
        ellipse('eye-right',[57,40],[8,5],[0.78,0.56,1,1]),
        polygon('blade', [[76,56],[94,44],[80,62]], [0.78,0.8,0.86,1]),
    ]


def leaper() -> list[dict[str, Any]]:
    """Frog-legged orange jumper with coiled hind legs and a bony crest."""
    skin: Color = [0.93,0.55,0.22,1]
    leg: Color = [0.75,0.38,0.14,1]
    return [
        ellipse('leg-left',[24,74],[30,20],leg),
        ellipse('leg-right',[76,74],[30,20],leg),
        polygon('foot-left', [[10,84],[30,82],[22,88]], [0.55,0.26,0.1,1]),
        polygon('foot-right', [[90,84],[70,82],[78,88]], [0.55,0.26,0.1,1]),
        ellipse('body',[50,60],[52,46],skin),
        ellipse('belly',[50,68],[30,22],[1,0.82,0.58,1]),
        polygon('crest', [[38,40],[44,22],[50,36],[56,20],[62,40]], [0.96,0.9,0.76,1]),
        ellipse('eye-white-left',[40,50],[14,14],WHITE),
        ellipse('eye-white-right',[60,50],[14,14],WHITE),
        ellipse('pupil-left',[41,51],[6,8],INK),
        ellipse('pupil-right',[61,51],[6,8],INK),
    ]


def wrecker() -> list[dict[str, Any]]:
    """Stocky rust-red brute carrying a stone hammer over one shoulder."""
    hide: Color = [0.72,0.32,0.24,1]
    return [
        stroke('haft', [[62,70],[84,24]], 6, [0.46,0.32,0.2,1]),
        polygon('hammer', [[72,10],[96,22],[90,36],[66,24]], [0.56,0.56,0.6,1]),
        ellipse('body',[48,62],[56,46],hide),
        ellipse('belly',[48,70],[32,22],[0.9,0.62,0.48,1]),
        polygon('brow', [[30,46],[66,46],[60,52],[36,52]], [0.42,0.18,0.14,1]),
        *eyes([40,55],[56,55],[7,6], [1,0.88,0.4,1]),
        ellipse('fist',[70,68],[14,14],[0.62,0.26,0.2,1]),
    ]


def slinger() -> list[dict[str, Any]]:
    """Lanky ochre thrower whirling a sling with a stone in its pouch."""
    skin: Color = [0.86,0.68,0.3,1]
    return [
        stroke('cord', [[60,44],[78,20],[90,30]], 2.5, [0.36,0.26,0.16,1]),
        ellipse('stone',[90,30],[12,12],[0.5,0.48,0.52,1]),
        ellipse('body',[48,62],[40,48],skin),
        polygon('scarf', [[30,46],[66,46],[62,54],[34,54]], [0.3,0.5,0.62,1]),
        polygon('hood', [[30,44],[50,18],[70,44]], [0.62,0.46,0.2,1]),
        *eyes([42,40],[56,40],[6,6], [0.2,0.14,0.08,1]),
        ellipse('arm',[62,50],[10,18],[0.76,0.58,0.24,1]),
    ]


def carrier() -> list[dict[str, Any]]:
    """Broad-winged teal bat clutching a rope sling with three passengers under it."""
    wing: Color = [0.22,0.46,0.5,1]
    return [
        polygon('wing-left', [[42,40],[4,22],[10,38],[2,48],[16,48],[22,58],[40,50]], wing),
        polygon('wing-right', [[58,40],[96,22],[90,38],[98,48],[84,48],[78,58],[60,50]], wing),
        ellipse('body',[50,42],[26,28],[0.28,0.56,0.6,1]),
        *eyes([44,38],[56,38],[6,6], [1,0.86,0.4,1]),
        stroke('rope-left', [[44,54],[36,72]], 2, [0.5,0.38,0.22,1]),
        stroke('rope-right', [[56,54],[64,72]], 2, [0.5,0.38,0.22,1]),
        polygon('sling', [[32,72],[68,72],[62,84],[38,84]], [0.62,0.46,0.26,1]),
        ellipse('rider-left',[40,70],[10,10],[0.82,0.4,0.32,1]),
        ellipse('rider-mid',[50,68],[10,10],[0.82,0.4,0.32,1]),
        ellipse('rider-right',[60,70],[10,10],[0.82,0.4,0.32,1]),
    ]


def golem() -> list[dict[str, Any]]:
    """Massive slate golem with crystal shoulders and a glowing core."""
    rock: Color = [0.37,0.35,0.43,1]
    crystal: Color = [0.42,0.84,0.94,1]
    return [
        ellipse('arm-left',[14,64],[22,40],[0.3,0.28,0.35,1]),
        ellipse('arm-right',[86,64],[22,40],[0.3,0.28,0.35,1]),
        polygon('body', [[24,30],[76,30],[90,52],[84,86],[16,86],[10,52]], rock),
        polygon('head', [[36,14],[64,14],[70,32],[30,32]], [0.45,0.43,0.52,1]),
        polygon('crystal-left', [[14,40],[22,16],[30,38]], crystal),
        polygon('crystal-right', [[70,38],[78,12],[86,40]], crystal),
        polygon('core', [[50,46],[62,60],[50,74],[38,60]], [1,0.58,0.2,1]),
        polygon('core-glow', [[50,52],[56,60],[50,68],[44,60]], [1,0.88,0.5,1]),
        polygon('eye-left', [[38,22],[47,22],[46,27],[39,27]], [1,0.86,0.3,1]),
        polygon('eye-right', [[53,22],[62,22],[61,27],[54,27]], [1,0.86,0.3,1]),
        stroke('crack', [[30,40],[36,50],[32,58]], 2.5, [0.22,0.2,0.26,1]),
    ]


CREATURES: dict[str, tuple[str, Any]] = {
    'oreling': ('Original oreling by Thor-ium TD', oreling),
    'dashling': ('Original dashling by Thor-ium TD', dashling),
    'ironback': ('Original ironback by Thor-ium TD', ironback),
    'pebbling': ('Original pebbling by Thor-ium TD', pebbling),
    'shieldling': ('Original shieldling by Thor-ium TD', shieldling),
    'mender': ('Original mender by Thor-ium TD', mender),
    'splitter': ('Original splitter by Thor-ium TD', splitter),
    'burrower': ('Original burrower by Thor-ium TD', burrower),
    'sapper': ('Original sapper by Thor-ium TD', sapper),
    'bat': ('Original bat by Thor-ium TD', bat),
    'stalker': ('Original stalker by Thor-ium TD', stalker),
    'leaper': ('Original leaper by Thor-ium TD', leaper),
    'wrecker': ('Original wrecker by Thor-ium TD', wrecker),
    'slinger': ('Original slinger by Thor-ium TD', slinger),
    'carrier': ('Original carrier by Thor-ium TD', carrier),
    'golem': ('Original golem by Thor-ium TD', golem),
}


def write(key: str, title: str, shapes: list[dict[str, Any]]) -> None:
    """Write one looping creature with a gentle squash movement cycle."""
    easing: dict[str, Any] = {'i':{'x':[0.6,0.6],'y':[1,1]},'o':{'x':[0.4,0.4],'y':[0,0]}}
    animation: dict[str, Any] = {'v':'5.7.4','fr':30,'ip':0,'op':45,'w':100,'h':100,'nm':title,'ddd':0,'assets':[], 'layers':[
        {'ddd':0,'ind':1,'ty':4,'nm':key.capitalize(),'sr':1,'ks':{
            'o':{'a':0,'k':100},'r':{'a':0,'k':0},'p':{'a':0,'k':[50,80,0]},'a':{'a':0,'k':[50,80,0]},
            's':{'a':1,'k':[{'t':0,'s':[100,100,100],'e':[106,93,100],**easing},{'t':22,'s':[106,93,100],'e':[100,100,100],**easing},{'t':45,'s':[100,100,100]}]}
        },'ao':0,'shapes':list(reversed(shapes)),'ip':0,'op':45,'st':0,'bm':0}
    ]}
    destination: Path = Path(__file__).resolve().parents[1] / f'content/animations/{key}.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(animation,separators=(',',':'))+'\n',encoding='utf-8')


def main() -> None:
    """Write every enemy creature; run build_animation_assets.py afterwards to add the action clips."""
    for key, (title, shapes) in CREATURES.items():
        write(key, title, shapes())


if __name__ == '__main__':
    main()
