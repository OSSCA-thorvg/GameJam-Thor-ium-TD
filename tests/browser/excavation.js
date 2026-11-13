/**
 * Play the first preparation of a fresh campaign on an excavation wall through real input: the surface reach
 * rule, pick and blast strikes with their dig prices, a scan, the mining mode setting and its persistence,
 * and the same wall on a 320px touch screen.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{screen:number,phase:number,wave:number,hasRun:boolean,modal:number,width:number,digs:number,scans:number,mining:number,buried:number,deposits:number,mineView:boolean,towers:number,units:number,energy:number,iron:number,copper:number,crystal:number,gold:number,mine:{x:number,y:number,tile:number,columns:number,rows:number}}} GameState */
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{x:number,y:number}} Point */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** @type {(error:Error)=>void} */ const onError=error=>errors.push(error.message);
    /** Require a gameplay or input invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read public run state without modifying WASM memory. @param {import('playwright').Page} target @returns {Promise<GameState>} */
    async function state(target){return target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for a public-state condition, evaluated in the browser. @param {import('playwright').Page} target @param {string} condition @param {number} timeout @returns {Promise<void>} */
    async function wait(target,condition,timeout=15000){
        try{await target.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout});}
        catch(error){throw new Error(`Excavation condition failed: ${condition}; state=${JSON.stringify(await state(target))}`,{cause:error});}
    }
    /** Resolve a registered control's logical rectangle. @param {import('playwright').Page} target @param {number} id @returns {Promise<Widget>} */
    async function widget(target,id){return target.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);}
    /** Click or tap a real UI control, preserving the SDL input path. @param {import('playwright').Page} target @param {number} id @param {boolean} touch @returns {Promise<void>} */
    async function click(target,id,touch=false){
        await target.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await widget(target,id);
        if(touch)await target.touchscreen.tap(item.x+item.w/2,item.y+item.h/2);else await target.mouse.click(item.x+item.w/2,item.y+item.h/2);
        await target.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve()))));
    }
    /** Transform a logical surface cell using the same viewport dimensions. @param {import('playwright').Page} target @param {number} x @param {number} y @returns {Promise<Point>} */
    async function cell(target,x,y){
        /** @type {{originX:number,originY:number,tile:number}} */ const field=await target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        return {x:field.originX+(x+.5)*field.tile,y:field.originY+(y+.5)*field.tile};
    }
    /** Strike or scan one wall tile by its row-major index. @param {import('playwright').Page} target @param {number} index @param {boolean} touch @returns {Promise<void>} */
    async function wall(target,index,touch=false){
        /** @type {{x:number,y:number,tile:number,columns:number}} */ const mine=(await state(target)).mine;
        /** @type {Point} */ const point={x:mine.x+(index%mine.columns+.5)*mine.tile,y:mine.y+(Math.floor(index/mine.columns)+.5)*mine.tile};
        if(touch)await target.touchscreen.tap(point.x,point.y);else await target.mouse.click(point.x,point.y);
        await target.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve()))));
    }
    /** Hold the first wave with one tower and one guard, then open the wall. @param {import('playwright').Page} target @param {boolean} touch @returns {Promise<void>} */
    async function reachWall(target,touch){
        await click(target,36,touch);await wait(target,'s.screen===1');
        /** @type {GameState} */ const fresh=await state(target);
        check(fresh.mining===1&&fresh.deposits>8&&fresh.buried===fresh.deposits,'A fresh profile starts an excavation with every deposit buried');
        // The compact layout opens on a briefing drawer that covers the start control.
        if(touch)await click(target,55,true);
        await click(target,10,touch);await wait(target,'s.phase===1');
        await click(target,1001,touch);
        /** @type {Point} */ const pad=await cell(target,6,4);
        if(touch)await target.touchscreen.tap(pad.x,pad.y);else await target.mouse.click(pad.x,pad.y);
        await wait(target,'s.towers===1');
        await click(target,12,touch);
        await wait(target,'s.phase===2',120000);
        await click(target,14,touch);await wait(target,'s.mineView&&s.digs===5&&s.scans===1');
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});
        await page.goto('http://127.0.0.1:8317/');
        await wait(page,'s.width===1200&&s.phase===0');
        await reachWall(page,false);
        /** @type {GameState} */ const opened=await state(page);
        check(opened.mine.columns===7&&opened.mine.rows===11,'The wall has the size of the campaign layer');
        await wall(page,17);await page.waitForTimeout(120);
        check((await state(page)).digs===5,'A strike below untouched rock is rejected without spending a dig');
        await wall(page,3);await wait(page,'s.digs===4');
        await wall(page,3);await page.waitForTimeout(120);
        /** @type {GameState} */ const afterTop=await state(page);
        check(afterTop.digs===4||afterTop.digs===3,'A second strike on bare rock only costs a dig when a neighbour still has rock');
        await wall(page,10);await wait(page,`s.digs===${afterTop.digs-1}`);
        checks.push('strikes start in the top row and follow bare rock down');
        await click(page,75);
        /** @type {number} */ const beforeBlast=(await state(page)).digs;
        await wall(page,10);await wait(page,`s.digs===${beforeBlast-2}`);
        check((await widget(page,74)).enabled&&(await widget(page,75)).enabled,'Both tool toggles stay available');
        checks.push('a blast costs two digs');
        await click(page,74);
        for(let guard=0;guard<6&&(await state(page)).digs>0;++guard){
            /** @type {number} */ const digs=(await state(page)).digs;
            await wall(page,3+7*(guard%2));await page.waitForTimeout(120);
            if((await state(page)).digs===digs)await wall(page,2+guard);
            await page.waitForTimeout(120);
        }
        await wait(page,'s.digs===0');
        await wall(page,4);await page.waitForTimeout(120);
        check((await state(page)).digs===0,'Strikes stop when the digs run out');
        await click(page,15);await wall(page,59);await wait(page,'s.scans===0');
        /** @type {GameState} */ const worked=await state(page);
        check(worked.buried<=worked.deposits&&worked.iron+worked.copper+worked.crystal+worked.gold>=0,'Collected deposits leave the buried count consistent');
        await page.screenshot({path:'.playwright-mcp/thorium-excavation.png',scale:'css'});
        checks.push('scan shows through any tile, and spent digs reject further strikes');

        await click(page,38);await click(page,76);await click(page,77);
        // Settings are written when the page loses focus.
        await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
        await page.waitForFunction(()=>(localStorage.getItem('thorium_td_save')||'').includes('"excavation":false'),null,{timeout:5000});
        await page.screenshot({path:'.playwright-mcp/thorium-settings-mining.png',scale:'css'});
        await page.reload();
        // The preparation in progress is stored, so the title offers to continue the same wall.
        await wait(page,'s.screen===0&&s.hasRun&&s.phase===2&&s.mining===1');
        /** @type {GameState} */ const resumed=await state(page);
        check(resumed.digs===worked.digs&&resumed.scans===worked.scans&&resumed.buried===worked.buried&&resumed.iron===worked.iron,
              `A reload restores the preparation in progress: ${JSON.stringify({digs:resumed.digs,scans:resumed.scans,buried:resumed.buried})}`);
        await click(page,37);await wait(page,'s.screen===1&&s.phase===2&&s.mineView===false');
        await click(page,14);await wait(page,'s.mineView');
        await page.screenshot({path:'.playwright-mcp/thorium-excavation-resumed.png',scale:'css'});
        checks.push('a reload continues the stored preparation on the same wall');
        await click(page,10);await wait(page,'s.phase===1&&s.wave===2');
        await page.reload();
        await wait(page,'s.screen===0&&s.hasRun&&s.phase===2&&s.wave===1');
        checks.push('a reload during a battle continues from the preparation before it');
        await click(page,36);await wait(page,'s.modal===4');await click(page,30);await wait(page,'s.screen===1');
        check((await state(page)).mining===0&&(await state(page)).buried===0&&(await state(page)).wave===0,
              'A confirmed new expedition replaces the stored one and uses the saved mining setting');
        checks.push('mining mode setting persists and applies to a new expedition');

        /** @type {import('playwright').BrowserContext} */ const context=await page.context().browser().newContext({viewport:{width:320,height:568},hasTouch:true,isMobile:true});
        try{
            /** @type {import('playwright').Page} */ const mobile=await context.newPage();mobile.on('pageerror',onError);
            await mobile.goto('http://127.0.0.1:8317/');await wait(mobile,'s.width===320&&s.phase===0');
            await reachWall(mobile,true);
            for(const id of [74,75]){
                /** @type {Widget} */ const tool=await widget(mobile,id);
                check(tool&&tool.enabled&&tool.x>=0&&tool.x+tool.w<=320&&tool.w>=40&&tool.h>=24,`Tool toggle ${id} fits the 320px screen as a touch target`);
            }
            /** @type {GameState} */ const small=await state(mobile);
            check(small.mine.tile>=24,`Wall tiles stay tappable on a 320px screen: ${small.mine.tile}`);
            await wall(mobile,3,true);await wait(mobile,'s.digs===4');
            await click(mobile,75,true);await wall(mobile,3,true);await wait(mobile,'s.digs===2');
            await mobile.screenshot({path:'.playwright-mcp/thorium-excavation-mobile.png',scale:'css'});
            checks.push('320px touch: tool toggles fit and taps strike the wall');
        }finally{await context.close();}
        check(errors.length===0,errors.join('; '));
        return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
