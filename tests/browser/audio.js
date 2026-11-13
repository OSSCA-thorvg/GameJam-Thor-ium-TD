/**
 * Verify real SDL/Web Audio output, trusted input unlock, mute, volume and mobile touch.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{muted:boolean,volume:number,voices:number,played:number,suppressed:number,beeps:number,mp3:number,assetErrors:number}} Audio */
    /** @typedef {{phase:number,paused:boolean,audio:Audio,audioDevice:boolean,audioActive:boolean,audioAudibleFrames:number}} State */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** Require a concrete output or interaction contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Observe output via an additional silent analysis branch, without altering the game's audio path. @returns {void} */
    function installProbe(){
        /** @type {{contexts:AudioContext[],meters:AnalyserNode[],lastAudible:number,peak:number}} */
        const probe={contexts:[],meters:[],lastAudible:0,peak:0};window.__thoriumAudioProbe=probe;
        /** @type {typeof AudioContext} */ const Original=window.AudioContext;
        window.AudioContext=class extends Original{constructor(...args){super(...args);probe.contexts.push(this);}};
        /** @type {AudioNode['connect']} */ const connect=AudioNode.prototype.connect;
        AudioNode.prototype.connect=function(destination,...args){
            /** @type {AudioNode} */ const result=connect.call(this,destination,...args);
            if(destination instanceof AudioDestinationNode){
                /** @type {AnalyserNode} */ const meter=this.context.createAnalyser();meter.fftSize=256;
                /** @type {GainNode} */ const silent=this.context.createGain();silent.gain.value=0;
                connect.call(this,meter);connect.call(meter,silent);connect.call(silent,destination);probe.meters.push(meter);
            }
            return result;
        };
        setInterval(()=>{
            for(const meter of probe.meters){
                /** @type {Float32Array} */ const samples=new Float32Array(meter.fftSize);meter.getFloatTimeDomainData(samples);
                /** @type {number} */ let peak=0;for(const value of samples)peak=Math.max(peak,Math.abs(value));
                probe.peak=Math.max(probe.peak,peak);if(peak>.00001)probe.lastAudible=performance.now();
            }
        },10);
    }
    /** Read public state without writing any game or audio state. @param {import('playwright').Page} target @returns {Promise<State>} */
    async function state(target){return target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Await a real state condition. @param {import('playwright').Page} target @param {string} condition @returns {Promise<void>} */
    async function wait(target,condition){await target.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);}
    /** Activate an actual logical SDL control. @param {import('playwright').Page} target @param {number} id @param {boolean} touch @returns {Promise<void>} */
    async function click(target,id,touch=false){
        await target.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await target.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);
        if(touch)await target.touchscreen.tap(item.x+item.w/2,item.y+item.h/2);else await target.mouse.click(item.x+item.w/2,item.y+item.h/2);
    }
    /** Record unexpected JavaScript failures. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    page.on('pageerror',onError);
    try{
        await page.addInitScript(installProbe);await page.setViewportSize({width:1200,height:900});
        await page.goto('http://127.0.0.1:8317/');await wait(page,'s.audio&&s.audioDevice');
        /** @type {State} */ const initial=await state(page);
        check(initial.audio.beeps===23&&initial.audio.mp3===0&&initial.audio.assetErrors===0,'All missing optional MP3 files use quiet procedural fallbacks');
        check(!initial.audioActive&&initial.audioAudibleFrames===0,'No audio output before trusted input');
        await click(page,36);await wait(page,'s.screen===1');await click(page,10);await wait(page,'s.phase===1&&s.audioActive&&s.audioAudibleFrames>0');
        await page.waitForFunction(()=>window.__thoriumAudioProbe.contexts.some(context=>context.state==='running')&&window.__thoriumAudioProbe.peak>.00001);
        checks.push('first real click activates SDL audio and the Web Audio graph receives nonzero PCM');
        await click(page,34);await wait(page,'s.audio.muted&&s.audio.voices===0');
        await page.waitForTimeout(200);
        /** @type {number} */ const silence=await page.evaluate(()=>window.__thoriumAudioProbe.lastAudible);
        await page.waitForTimeout(250);
        check(await page.evaluate(time=>window.__thoriumAudioProbe.lastAudible===time,silence),'Mute must silence the actual output graph after the bounded queue drains');
        await click(page,34);await wait(page,'!s.audio.muted');
        // Toggling speed plays the selection cue, so audible output resumes without waiting for combat.
        // The cue's cooldown runs on the frame-advanced mixer clock, and unmuting plays the same cue, so each
        // toggle waits for frames after the previous cue.
        await page.waitForTimeout(120);
        /** @type {number} */ const cues=(await state(page)).audio.played;
        await click(page,12);await page.waitForTimeout(120);await click(page,12);await wait(page,`s.audio.played>=${cues+2}`);
        await page.waitForFunction(time=>window.__thoriumAudioProbe.lastAudible>time,silence);
        /** @type {number} */ const volume=(await state(page)).audio.volume;
        await click(page,35);await wait(page,`s.audio.volume!==${volume}`);check((await state(page)).audio.volume!==volume,'Volume control changes master gain');
        await page.keyboard.press('s');await wait(page,'s.audio.muted');
        await click(page,24);await click(page,30);await wait(page,'s.phase===0');
        check((await state(page)).audio.muted,'Run reset retains sound preferences');
        checks.push('real output mute/unmute, master volume, keyboard control and reset persistence');
        /** @type {import('playwright').Browser} */ const browser=page.context().browser();
        /** @type {import('playwright').BrowserContext} */ const mobileContext=await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true});
        /** @type {import('playwright').Page} */ const mobile=await mobileContext.newPage();mobile.on('pageerror',onError);
        try{
            await mobile.addInitScript(installProbe);await mobile.goto('http://127.0.0.1:8317/');await wait(mobile,'s.audioDevice');
            await click(mobile,36,true);await wait(mobile,'s.screen===1');await click(mobile,56,true);await click(mobile,10,true);await wait(mobile,'s.audioAudibleFrames>0');
            await mobile.waitForFunction(()=>window.__thoriumAudioProbe.peak>.00001);
            await click(mobile,34,true);await wait(mobile,'s.audio.muted');
            await click(mobile,11,true);await wait(mobile,'s.paused');
            await click(mobile,35,true);await wait(mobile,'!s.audio.muted');
            await mobile.screenshot({path:'.playwright-mcp/thorium-audio-mobile.png',scale:'css'});
            checks.push('390px touch unlock, compact mute control and pause-menu volume');
        }finally{await mobileContext.close();}
        check(errors.length===0,errors.join('; '));return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
