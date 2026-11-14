/**
 * Complete the seven practice lessons through real desktop and touch input, without state injection.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean,focusable:boolean}} Widget */
    /** @typedef {{x:number,y:number}} Point */
    /** @typedef {{screen:number,phase:number,energy:number,time:number,towers:number,units:number,gold:number,iron:number,copper:number,tool:number,digs:number,busy:boolean,width:number,height:number,reducedMotion:boolean,tutorial:{active:boolean,lesson:number,mineCell:number,completed:boolean},forks:{x:number,y:number,detour:boolean}[]}} Snapshot */
    /** @type {string[]} */ const errors=[];
    /** Require a real input/learning invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Collect browser errors. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    /** Read current diagnostics without writing memory. @param {import('playwright').Page} target @returns {Promise<Snapshot>} */
    async function state(target){return target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for initialization, SDL delivery or ordinary simulation. @param {import('playwright').Page} target @param {string} condition @returns {Promise<void>} */
    async function wait(target,condition){
        try{await target.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout:35000});}
        catch(error){throw new Error(`Tutorial condition failed: ${condition}; state=${JSON.stringify(await state(target))}`,{cause:error});}
    }
    /** Read the current control registrations. @param {import('playwright').Page} target @returns {Promise<Widget[]>} */
    async function widgets(target){return target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())));}
    /** Resolve a current control. @param {import('playwright').Page} target @param {number} id @returns {Promise<Widget>} */
    async function widget(target,id){return (await widgets(target)).find(item=>item.id===id);}
    /** Click using actual mouse or touch events. @param {import('playwright').Page} target @param {number} id @param {boolean} touch @returns {Promise<void>} */
    async function click(target,id,touch=false){
        await target.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await widget(target,id);
        if(touch)await target.touchscreen.tap(item.x+item.w/2,item.y+item.h/2);else await target.mouse.click(item.x+item.w/2,item.y+item.h/2);
    }
    /** Fit all actionable controls and reject duplicate IDs, including after viewport changes. @param {import('playwright').Page} target @returns {Promise<void>} */
    async function fit(target){
        /** @type {Widget[]} */ const all=await widgets(target), controls=all.filter(item=>item.enabled&&item.focusable);
        /** @type {Snapshot} */ const current=await state(target);
        check(new Set(all.map(item=>item.id)).size===all.length,'Tutorial controls have unique identities');
        for(const a of controls){
            check(a.x>=0&&a.y>=0&&a.x+a.w<=current.width+1&&a.y+a.h<=current.height+1,'Tutorial controls fit viewport');
            for(const b of controls)if(a.id<b.id)check(a.x+a.w<=b.x+.5||b.x+b.w<=a.x+.5||a.y+a.h<=b.y+.5||b.y+b.h<=a.y+.5,`Tutorial controls overlap: ${a.id}/${b.id}`);
        }
    }
    /** Resolve surface or cutaway coordinates from the actual canvas registration. @param {import('playwright').Page} target @param {number} x @param {number} y @param {boolean} mine @returns {Promise<Point>} */
    async function cell(target,x,y,mine=false){
        if(!mine){
            /** @type {{originX:number,originY:number,tile:number}} */ const field=await target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
            return {x:field.originX+(x+.5)*field.tile,y:field.originY+(y+.5)*field.tile};
        }
        /** @type {{x:number,y:number,tile:number}} */ const board=(await state(target)).mine;
        return {x:board.x+(x+.5)*board.tile,y:board.y+(y+.5)*board.tile};
    }
    /** Tap a logical target using a real pointer. @param {import('playwright').Page} target @param {Point} point @param {boolean} touch @returns {Promise<void>} */
    async function pointClick(target,point,touch){if(touch)await target.touchscreen.tap(point.x,point.y);else await target.mouse.click(point.x,point.y);}
    /** Strike one highlighted wall cell and wait for the dig to be spent. @param {import('playwright').Page} target @param {number} index @param {number} digs @param {boolean} touch @returns {Promise<void>} */
    async function dig(target,index,digs,touch){await pointClick(target,await cell(target,index%5,Math.floor(index/5),true),touch);await wait(target,`!s.busy&&s.digs===${digs}`);}
    /** Exercise the complete lesson sequence at the caller's viewport and input device. @param {import('playwright').Page} target @param {boolean} touch @returns {Promise<void>} */
    async function complete(target,touch){
        await target.goto('http://127.0.0.1:8317/');await wait(target,'s.screen===0');await fit(target);
        await click(target,57,touch);await wait(target,'s.tutorial.active&&s.tutorial.lesson===1');await fit(target);
        await target.waitForTimeout(250);check((await state(target)).time===0&&(await state(target)).energy===6,'Briefing freezes only simulation');
        await click(target,58,touch);await wait(target,'s.tutorial.lesson===2');
        await click(target,1003,touch);check((await state(target)).energy===6,'Other cards cannot spend setup energy');
        /** @type {Widget} */ const card=await widget(target,1001);
        /** @type {Point} */ const pad=await cell(target,6,4);
        /** @type {import('playwright').CDPSession|null} */ const cdp=touch?await target.context().newCDPSession(target):null;
        if(touch){
            await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:card.x+card.w/2,y:card.y+card.h/2,id:41}]});
            await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:pad.x,y:pad.y,id:41}]});
        }else{
            await target.mouse.move(card.x+card.w/2,card.y+card.h/2);await target.mouse.down();await target.mouse.move(pad.x,pad.y,{steps:8});
        }
        await wait(target,'s.placement?.card===1&&s.placement.range===3&&s.time===0');
        await target.screenshot({path:`.playwright-mcp/thorium-tutorial-drag-${touch?'320':'desktop'}.png`,scale:'css'});
        if(touch)await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});else await target.mouse.up();
        if(cdp)await cdp.detach();
        await wait(target,'s.tutorial.lesson===3&&s.towers===1&&s.energy===3&&s.time===0');
        await click(target,38,touch);await wait(target,'s.screen===2');await click(target,50,touch);await click(target,43,touch);await click(target,41,touch);
        await wait(target,'s.screen===1&&s.tutorial.lesson===3&&s.time===0');
        await click(target,59,touch);await wait(target,'s.towers===1&&s.energy===3&&s.tutorial.lesson===3');
        await click(target,1002,touch);await pointClick(target,await cell(target,8,4),touch);
        await wait(target,'s.tutorial.lesson===4&&s.units===1&&s.energy===0');
        await wait(target,'s.phase===2&&s.tutorial.lesson===5&&s.digs===5&&!s.forks[0].detour');await fit(target);
        /** @type {{x:number,y:number}} */ const barricade=(await state(target)).forks[0];
        await pointClick(target,await cell(target,barricade.x,barricade.y),touch);
        await wait(target,'s.tutorial.lesson===6&&s.forks[0].detour');
        await target.screenshot({path:`.playwright-mcp/thorium-tutorial-scan-${touch?'320':'desktop'}.png`,scale:'css'});
        await click(target,15,touch);await pointClick(target,await cell(target,1,1,true),touch);
        await wait(target,'s.tutorial.lesson===7&&s.scans===0&&s.digs===5&&s.iron===0');
        await dig(target,0,4,touch);await wait(target,'s.tutorial.lesson===7&&s.iron===0');
        await dig(target,5,3,touch);await wait(target,'s.tutorial.lesson===8&&s.iron>=4');
        await dig(target,3,2,touch);await wait(target,'s.copper>=4');
        await click(target,17,touch);await wait(target,'s.tutorial.lesson===9&&s.tool===2&&s.copper===0');
        await dig(target,8,1,touch);await dig(target,7,0,touch);
        await wait(target,'s.tutorial.lesson===10&&s.tutorial.completed&&s.gold===4');await fit(target);
        await target.screenshot({path:`.playwright-mcp/thorium-tutorial-complete-${touch?'320':'desktop'}.png`,scale:'css'});
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});await complete(page,false);
        await click(page,61);await wait(page,'!s.tutorial.active&&s.phase===0&&s.gold===0&&s.tool===1&&s.towers===0&&s.tutorial.completed');
        await click(page,10);await click(page,1001);await pointClick(page,await cell(page,6,4),false);await wait(page,'s.towers===1');
        await click(page,38);await wait(page,'s.screen===2');await click(page,42);await wait(page,'s.screen===0');
        /** @type {Snapshot} */ const saved=await state(page);
        await click(page,57);await wait(page,'s.tutorial.active');await click(page,60);await wait(page,'s.screen===0&&!s.tutorial.active');
        check((await state(page)).towers===1&&(await state(page)).time===saved.time&&(await state(page)).reducedMotion,'Skipping practice preserves existing regular progress and settings');
        /** @type {import('playwright').BrowserContext} */ const context=await page.context().browser().newContext({viewport:{width:320,height:568},isMobile:true,hasTouch:true});
        try{
            /** @type {import('playwright').Page} */ const mobile=await context.newPage();mobile.on('pageerror',onError);
            await complete(mobile,true);await click(mobile,60,true);await wait(mobile,'s.screen===0&&!s.tutorial.active&&s.tutorial.completed');
            await click(mobile,39,true);await wait(mobile,'s.screen===3');await click(mobile,57,true);await wait(mobile,'s.tutorial.lesson===1&&s.gold===0&&s.tool===1');await fit(mobile);
        }finally{await context.close();}
        check(errors.length===0,errors.join('; '));
        return {checks:['eight lessons complete with real costs, combat, a barricade move, scans, wall strikes, T2 crafting and gold','setup drag/range previews work while simulation is frozen; settings and retry preserve lesson checkpoints','completion and skip isolate regular progress, resources, preferences and session completion','full 320px touch practice and restarting from the guide work with bounded nonoverlapping controls'],errors};
    }finally{page.off('pageerror',onError);}
}
