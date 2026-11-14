/**
 * Open the two deepest stages from an unlocked profile and work their hand-operated machines through real
 * input: the stage 6 cart lever after its first countdown, and a stage 7 gas tank once per battle.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{x:number,y:number,countdown:number,progress:number}} Rail */
    /** @typedef {{x:number,y:number,ready:boolean}} Tank */
    /** @typedef {{phase:number,currentStage:number,selectedStage:number,highestUnlockedStage:number,totalWaves:number,field:{width:number,originX:number,originY:number,tile:number,zoom:number},lanes:unknown[],rails:Rail[],tanks:Tank[],forks:{x:number,y:number,detour:boolean,length:number}[]}} GameState */
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** @type {(error:Error)=>void} */ const onError=error=>errors.push(error.message);
    /** Require a gameplay or input invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read public run state without modifying WASM memory. @returns {Promise<GameState>} */
    async function state(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for a public-state condition, evaluated in the browser. @param {string} condition @param {number} timeout @returns {Promise<void>} */
    async function wait(condition,timeout=15000){
        try{await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout});}
        catch(error){throw new Error(`Machine condition failed: ${condition}; state=${JSON.stringify(await state())}`,{cause:error});}
    }
    /** Click a real UI control once it is enabled. @param {number} id @returns {Promise<void>} */
    async function click(id){
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);
        await page.mouse.click(item.x+item.w/2,item.y+item.h/2);
        await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve()))));
    }
    /** Tap a logical battlefield cell through the SDL pointer path. @param {number} x @param {number} y @returns {Promise<void>} */
    async function tap(x,y){
        /** @type {{originX:number,originY:number,tile:number}} */ const field=(await state()).field;
        await page.mouse.click(field.originX+(x+.5)*field.tile,field.originY+(y+.5)*field.tile);
        await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve()))));
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1280,height:800});
        await page.addInitScript(()=>{if(location.origin==='http://127.0.0.1:8317')localStorage.setItem('thorium_td_save','{"highestUnlockedStage":7,"tutorialCompleted":true,"excavation":false}');});
        await page.goto('http://127.0.0.1:8317/');await wait('s.highestUnlockedStage===7');
        for(let step=0;step<5;++step)await click(63);
        await wait('s.selectedStage===6');await click(36);
        await wait('s.currentStage===6&&s.totalWaves===16&&s.field.width===50&&s.lanes.length===4&&s.rails.length===2&&s.tanks.length===0');
        checks.push('stage 6 opens a 50-wide crystal cavern with four entrances and two cart rails');
        /** @type {Rail} */ const lever=(await state()).rails[0];
        await tap(lever.x,lever.y);
        check((await state()).rails[0].progress===-1,'A lever outside battle only explains itself');
        /** @type {{x:number,y:number,detour:boolean,length:number}} */ const fork=(await state()).forks[0];
        await tap(fork.x,fork.y);
        await wait('s.forks[0].detour');
        await tap((await state()).forks[0].x,(await state()).forks[0].y);
        await wait(`!s.forks[0].detour&&s.forks[0].length===${fork.length}`);
        checks.push('a tap on the stage 6 barricade before the first wave reroutes the entrance and back');
        await click(10);await wait('s.phase===1');await click(66);await wait('s.field.zoom===1');
        await tap(lever.x,lever.y);
        check((await state()).rails[0].progress===-1&&(await state()).rails[0].countdown>0,'The first departure waits for its countdown');
        await wait('s.rails[0].countdown===0',20000);
        await tap(lever.x,lever.y);
        await wait('s.rails[0].progress>=0||s.rails[0].countdown>10');
        checks.push('a tap on the stage 6 lever sends the cart once its first countdown ends');
        await wait('s.rails[0].progress===-1&&s.rails[0].countdown>10');
        await tap(lever.x,lever.y);
        check((await state()).rails[0].progress===-1,'A returning cart ignores the lever');
        checks.push('the cart returns and its lever waits through the cooldown');
        await page.screenshot({path:'.playwright-mcp/thorium-stage6-cart.png',scale:'css'});

        await click(11);await click(38);await click(42);await click(63);await wait('s.selectedStage===7');
        await click(36);await click(30);
        await wait('s.currentStage===7&&s.totalWaves===16&&s.field.width===52&&s.lanes.length===5&&s.tanks.length===6&&s.rails.length===0');
        checks.push('stage 7 opens a 52-wide swamp with five entrances and six gas tanks');
        await click(10);await wait('s.phase===1');await click(66);await wait('s.field.zoom===1');
        await wait('s.tanks.every(tank=>tank.ready)');
        /** @type {Tank} */ const tank=(await state()).tanks[0];
        await tap(tank.x,tank.y);
        await wait('!s.tanks[0].ready&&s.tanks.slice(1).every(tank=>tank.ready)');
        checks.push('a tap on a gas tank detonates that tank alone');
        await tap(tank.x,tank.y);
        check(!(await state()).tanks[0].ready&&(await state()).tanks.slice(1).every(item=>item.ready),'A spent tank stays spent for the battle');
        checks.push('a spent tank ignores further taps in the same battle');
        await page.screenshot({path:'.playwright-mcp/thorium-stage7-tank.png',scale:'css'});
    }finally{page.off('pageerror',onError);}
    return {checks,errors};
}
