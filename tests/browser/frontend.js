/**
 * Exercise the title, nested utility pages, real settings and preservation of a running expedition.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean,focusable:boolean}} Widget */
    /** @typedef {{screen:number,phase:number,wave:number,time:number,towers:number,paused:boolean,hasRun:boolean,reducedMotion:boolean,damageNumbers:boolean,settingsTab:number,guidePage:number,modal:number,width:number,height:number,audio:{volume:number,muted:boolean,played:number},cardMotion:{busy:boolean}[]}} State */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** Require a player-observable navigation or gameplay invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read current state without writing game data or clocks. @returns {Promise<State>} */
    async function state(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Await one condition after the packaged runtime has initialized. @param {string} condition @returns {Promise<void>} */
    async function wait(condition){
        try{await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);}
        catch(error){throw new Error(`Front-end condition failed: ${condition}; state=${JSON.stringify(await state())}`,{cause:error});}
    }
    /** Read current registered hit regions. @returns {Promise<Widget[]>} */
    async function widgets(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())));}
    /** Click one real enabled native UI control. @param {number} id @returns {Promise<void>} */
    async function click(id){await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);/** @type {Widget} */ const item=(await widgets()).find(item=>item.id===id);await page.mouse.click(item.x+item.w/2,item.y+item.h/2);}
    /** Verify all enabled controls fit and are unambiguous, including compact layouts. @returns {Promise<void>} */
    async function fit(){
        /** @type {State} */ const current=await state();/** @type {Widget[]} */ const controls=(await widgets()).filter(item=>item.enabled&&item.focusable);
        check(controls.every(item=>item.x>=0&&item.y>=0&&item.x+item.w<=current.width+1&&item.y+item.h<=current.height+1),'Front-end controls fit viewport');
        for(let i=0;i<controls.length;++i)for(let j=i+1;j<controls.length;++j){/** @type {Widget} */ const a=controls[i],b=controls[j];check(a.x+a.w<=b.x+.5||b.x+b.w<=a.x+.5||a.y+a.h<=b.y+.5||b.y+b.h<=a.y+.5,`Front-end controls ${a.id}/${b.id} overlap`);}
    }
    /** Click a logical battlefield cell through the measured world transform. @param {number} x @param {number} y @returns {Promise<void>} */
    async function cell(x,y){
        /** @type {{originX:number,originY:number,tile:number}} */
        const field=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        await page.mouse.click(field.originX+(x+.5)*field.tile,field.originY+(y+.5)*field.tile);
    }
    /** Capture unexpected script failures. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});await page.goto('http://127.0.0.1:8317/');await wait('s.screen===0&&s.width===1200');
        check(!(await widgets()).some(item=>item.id===1||item.id===10),'Title cannot hit the hidden battlefield or start a wave');
        await page.keyboard.press('1');await page.keyboard.press('m');await page.waitForTimeout(180);
        check((await state()).wave===0&&(await state()).time===0,'Gameplay shortcuts do not run under title');
        await fit();await page.screenshot({path:'.playwright-mcp/thorium-home.png',scale:'css'});
        await click(38);await wait('s.screen===2');await fit();
        await click(45);await wait('s.audio.volume===.4');await click(46);await wait('s.audio.volume===.45');
        await click(34);await wait('s.audio.muted');check(!(await widgets()).find(item=>item.id===47).enabled,'Muted audio preview is disabled');
        await click(34);await wait('!s.audio.muted');await click(47);
        await page.screenshot({path:'.playwright-mcp/thorium-settings-audio.png',scale:'css'});
        await click(50);await wait('s.settingsTab===1');await click(43);await wait('s.reducedMotion');await click(44);await wait('!s.damageNumbers');
        await page.screenshot({path:'.playwright-mcp/thorium-settings-visual.png',scale:'css'});
        await click(39);await wait('s.screen===3');await click(52);await wait('s.guidePage===1');await click(52);await wait('s.guidePage===2');
        check(!(await widgets()).find(item=>item.id===52).enabled,'Final guide page cannot advance beyond content');
        await page.keyboard.press('Escape');await wait('s.screen===2&&s.settingsTab===1');await click(41);await wait('s.screen===0');
        await click(40);await wait('s.screen===4');await fit();await click(41);await wait('s.screen===0');
        checks.push('cold title, functional audio/display settings, nested Back, guide and credits');
        await click(88);await wait("s.screen===0&&s.language==='en'");await fit();
        check(JSON.parse(await page.evaluate(()=>localStorage.getItem('thorium_td_save'))).language==='en','Language change is saved at once');
        await page.reload();await wait("s.screen===0&&s.language==='en'");
        await click(38);await wait('s.screen===2');await click(89);await wait('s.settingsTab===3');await fit();await click(4200);await wait("s.language==='ko'");await click(42);await wait('s.screen===0');
        checks.push('home globe and settings language tab switch, save and survive a reload');
        await click(36);await wait('s.screen===1&&s.phase===0');check((await state()).reducedMotion&&!(await state()).damageNumbers,'New expedition retains presentation preferences');
        await fit();await page.screenshot({path:'.playwright-mcp/thorium-briefing-desktop.png',scale:'css'});
        await click(55);await wait('!s.guideOpen');await click(54);await wait('s.guideOpen');
        await click(10);await click(1001);await cell(6,4);await wait('s.towers===1');
        check(!(await state()).cardMotion[0].busy,'Reduced motion makes the replacement face immediately available');
        await click(38);await wait('s.screen===2');
        /** @type {number} */ const frozen=(await state()).time;await page.waitForTimeout(250);
        check((await state()).time===frozen&&(await state()).towers===1,'Settings freezes real simulation and preserves placed towers');
        await click(41);await wait('s.screen===1');await wait(`s.time>${frozen+.03}`);
        await click(11);await wait('s.paused');await click(38);await wait('s.screen===2');await click(41);await wait('s.screen===1&&s.paused');
        /** @type {number} */ const paused=(await state()).time;await page.waitForTimeout(180);check((await state()).time===paused,'Returning from settings preserves an existing pause');
        await click(42);await wait('s.screen===0&&s.hasRun');await click(36);await wait('s.modal===4');await click(25);await wait('s.modal===0');
        check((await state()).towers===1,'Cancel new-expedition confirmation preserves the current run');
        await click(37);await wait('s.screen===1&&!s.paused');check((await state()).towers===1,'Continue restores the same expedition');
        await click(11);await click(42);await wait('s.screen===0');await click(36);await click(30);await wait('s.screen===1&&s.phase===0&&s.towers===0');
        check((await state()).reducedMotion&&!(await state()).damageNumbers,'Confirmed restart preserves user preferences');
        await click(38);await click(48);await wait('!s.reducedMotion&&s.damageNumbers&&s.audio.volume===.45&&!s.audio.muted');await click(42);await wait('s.screen===0&&s.hasRun');
        checks.push('settings freezes/resumes simulation, prior pause survives, home continue and safe new-run confirmation');
        for(const size of [{width:980,height:739},{width:390,height:844},{width:320,height:568},{width:1200,height:568}]){
            await page.setViewportSize(size);await wait(`s.width===${size.width}&&s.height===${size.height}`);await fit();
            await click(38);await wait('s.screen===2');await click(49);await fit();await click(50);await fit();await click(39);await wait('s.screen===3');await fit();await click(42);await wait('s.screen===0');
        }
        await page.setViewportSize({width:390,height:844});await wait('s.width===390&&s.height===844');
        await page.screenshot({path:'.playwright-mcp/thorium-home-mobile.png',scale:'css'});
        await click(38);await wait('s.screen===2');await click(49);await page.screenshot({path:'.playwright-mcp/thorium-settings-mobile.png',scale:'css'});
        checks.push('title/settings/guide controls fit at 1200, 980, 390 and 320px, including short landscape');
        await click(42);await wait('s.screen===0');await click(37);await wait('s.screen===1&&s.phase===0&&s.guideOpen');
        await fit();await page.screenshot({path:'.playwright-mcp/thorium-briefing-mobile.png',scale:'css'});
        check(!(await widgets()).find(item=>item.id===10).enabled,'Briefing disables covered compact controls');
        await click(56);await wait('!s.guideOpen&&s.phase===0');await click(54);await wait('s.guideOpen');await click(55);await wait('!s.guideOpen');
        await page.setViewportSize({width:320,height:568});await wait('s.width===320&&s.height===568');
        await click(54);await wait('s.guideOpen');await fit();await page.screenshot({path:'.playwright-mcp/thorium-briefing-320.png',scale:'css'});
        await click(56);await click(1001);await wait('s.selectedCard===1');await fit();
        await page.mouse.move(0,0);await page.screenshot({path:'.playwright-mcp/thorium-card-controls-320.png',scale:'css'});
        checks.push('redesigned briefing X, tab and acknowledgement work without starting a wave; compact card controls do not overlap');
        check(errors.length===0,errors.join('; '));return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
