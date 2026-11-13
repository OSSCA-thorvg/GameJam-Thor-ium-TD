/**
 * Exercise authored stages, suspended-run ownership and map gestures through the shipped SDL client.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean,focusable:boolean}} Widget */
    /** @typedef {{x:number,y:number}} Point */
    /** @typedef {{screen:number,phase:number,towers:number,totalWaves:number,currentStage:number,selectedStage:number,selectedCard:number,modal:number,field:{width:number,height:number,tile:number,originX:number,originY:number,zoom:number,panX:number,panY:number},lanes:{route:number,count:number,length:number}[]}} Snapshot */
    /** @type {string[]} */ const errors=[];
    /** Collect unexpected page errors. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    /** Read the observable WASM snapshot. @returns {Promise<Snapshot>} */
    async function state(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for a rendered invariant with actionable failure evidence. @param {string} predicate @returns {Promise<void>} */
    async function wait(predicate){await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${predicate};})()`);}
    /** Resolve the currently painted logical geometry. @param {number} id @returns {Promise<Widget>} */
    async function widget(id){return page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);}
    /** Click or tap an enabled control through SDL. @param {number} id @param {boolean} touch @returns {Promise<void>} */
    async function click(id,touch=false){
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await widget(id);
        if(touch) await page.touchscreen.tap(item.x+item.w/2,item.y+item.h/2);
        else await page.mouse.click(item.x+item.w/2,item.y+item.h/2);
        // Read the next control only after SDL has presented the resulting screen.
        await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>resolve()))));
    }
    /** Require a user-visible contract. @param {unknown} condition @param {string} message @returns {void} */
    function check(condition,message){if(!condition)throw new Error(message);}
    /** Project a placement using only the same public transform used by rendering. @param {number} x @param {number} y @returns {Promise<Point>} */
    async function cell(x,y){/** @type {Snapshot} */ const s=await state();return{x:s.field.originX+(x+.5)*s.field.tile,y:s.field.originY+(y+.5)*s.field.tile};}
    page.on('pageerror',onError);
    try {
        await page.setViewportSize({width:1200,height:900});
        // A previous legitimate completion is external input, rather than a live-state debug mutation.
        await page.addInitScript(()=>{if(location.origin==='http://127.0.0.1:8317')localStorage.setItem('thorium_td_save','{"highestUnlockedStage":3,"tutorialCompleted":true,"excavation":false}');});
        await page.goto('http://127.0.0.1:8317/');await wait('s.highestUnlockedStage===3');
        await click(63);await wait('s.selectedStage===2');await click(36);await wait('s.currentStage===2&&s.totalWaves===10&&s.field.width===34&&s.lanes.length===3');
        await page.screenshot({path:'.playwright-mcp/thorium-campaign-stage2.png',scale:'css'});
        await click(38);await click(42);await click(63);await wait('s.selectedStage===3');await click(36);await click(30);
        await wait('s.currentStage===3&&s.totalWaves===12&&s.field.width===38&&s.lanes.length===4');
        /** @type {Widget} */ const world=await widget(1);
        await page.mouse.move(world.x+world.w/2,world.y+world.h/2);await page.mouse.wheel(0,-450);
        await wait('s.field.zoom>1.5');
        /** @type {number} */ const zoom=(await state()).field.zoom;
        await page.mouse.move(world.x+world.w*.55,world.y+world.h*.55);await page.mouse.down();
        await page.mouse.move(world.x+world.w*.7,world.y+world.h*.7,{steps:6});await page.mouse.up();
        await wait('Math.abs(s.field.panX)>.1||Math.abs(s.field.panY)>.1');
        check((await state()).field.zoom===zoom&&!(await state()).selectedCard,'Map drag preserves zoom and does not play a card');
        await page.screenshot({path:'.playwright-mcp/thorium-campaign-zoom.png',scale:'css'});
        await click(66);await wait('s.field.zoom===1&&s.field.panX===0&&s.field.panY===0');
        await click(38);await click(42);await click(62);await click(62);await click(36);await click(30);
        await wait('s.currentStage===1&&s.totalWaves===8&&s.field.width===30&&s.lanes.length===3');
        await click(10);
        /** @type {Point} */ const pad=await cell(6,4);
        await page.mouse.move(pad.x,pad.y);await page.mouse.wheel(0,-250);await wait('s.field.zoom>1');
        /** @type {Point} */ const enlarged=await cell(6,4);
        /** @type {Widget} */ const card=await widget(1001);
        await page.mouse.move(card.x+card.w/2,card.y+card.h/2);await page.mouse.down();await page.mouse.move(enlarged.x,enlarged.y,{steps:5});
        await wait('s.placement?.allowed&&s.placement.x===6&&s.placement.y===4&&s.placement.range===3');
        /** @type {number} */ const heldZoom=(await state()).field.zoom;
        await page.mouse.wheel(0,-200);check((await state()).field.zoom===heldZoom,'Wheel cannot move a card target during capture');
        await page.mouse.up();await wait('s.towers===1&&s.selectedCard===0');
        await click(11);await click(38);await click(42);await click(63);await wait('s.selectedStage===2&&s.currentStage===1&&s.towers===1');
        await click(37);await wait('s.currentStage===1&&s.totalWaves===8&&s.towers===1');
        await click(11);await click(38);await click(42);await click(36);await click(30);await wait('s.currentStage===2&&s.totalWaves===10&&s.towers===0');
        /** @type {import('playwright').BrowserContext} */ const context=await page.context().browser().newContext({viewport:{width:320,height:568},hasTouch:true,isMobile:true});
        try {
            /** @type {import('playwright').Page} */ const mobile=await context.newPage();
            // This scenario checks the tile-by-tile cutaway; a fresh profile would open an excavation wall.
            await mobile.addInitScript(()=>{if(location.origin==='http://127.0.0.1:8317'&&!localStorage.getItem('thorium_td_save'))localStorage.setItem('thorium_td_save','{"excavation":false}');});
            mobile.on('pageerror',onError);
            await mobile.goto('http://127.0.0.1:8317/');await mobile.waitForFunction(()=>typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function');
            /** Tap one enabled mobile control. @param {number} id @returns {Promise<void>} */
            async function tap(id){
                /** @type {Widget} */ const item=await mobile.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id&&item.enabled),id);
                check(item,`Touch control ${id} is enabled`);await mobile.touchscreen.tap(item.x+item.w/2,item.y+item.h/2);
                await mobile.waitForTimeout(80);
            }
            await tap(36);await tap(55);await tap(67);await tap(67);
            /** @type {Widget} */ const area=await mobile.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===1));
            /** @type {import('playwright').CDPSession} */ const touch=await context.newCDPSession(mobile);
            await touch.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:area.x+area.w*.5,y:area.y+area.h*.5,id:1}]});
            await touch.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:area.x+area.w*.7,y:area.y+area.h*.7,id:1}]});
            await touch.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
            await mobile.waitForFunction(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return s.field.zoom>1&&Math.abs(s.field.panY)>.1;});
            await mobile.screenshot({path:'.playwright-mcp/thorium-campaign-mobile.png',scale:'css'});
            await tap(66);
            await mobile.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field.zoom===1);
            /** @type {Widget[]} */ const controls=await mobile.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).filter(item=>item.enabled&&item.focusable));
            check(controls.every(item=>item.x>=0&&item.y>=0&&item.x+item.w<=320.5&&item.y+item.h<=568.5),'320px map controls remain in the viewport');
            await tap(10);await tap(12);
            /** Place an ally through the current projected touch coordinates. @param {number} x @param {number} y @returns {Promise<void>} */
            async function tapCell(x,y){
                /** @type {{originX:number,originY:number,tile:number}} */ const field=await mobile.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
                await mobile.touchscreen.tap(field.originX+(x+.5)*field.tile,field.originY+(y+.5)*field.tile);
            }
            await tap(1001);await tapCell(6,4);await tap(1002);await tapCell(8,4);
            await mobile.waitForFunction(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return s.energy>=3&&s.towers===1&&s.units===1;});
            await tap(1005);await tapCell(9,4);
            await mobile.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).phase===2,null,{timeout:60000});
            /** @type {Widget} */ const mine=await mobile.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===1));
            check(mine.h>=190,'Small touch cutaways allocate enough height for the full board');
            /** @type {{x:number,y:number,tile:number,columns:number}} */ const board=await mobile.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).mine);
            check(board.tile>=24,'Mine tiles retain at least 24 logical pixels');
            await tap(15);
            /** Resolve one cutaway cell center in the current scroll position. @param {number} index @returns {Point} */
            const at=index=>({x:board.x+(index%board.columns+.5)*board.tile,y:board.y+(Math.floor(index/board.columns)+.5)*board.tile});
            /** @type {Point} */ const iron=at(9);
            /** @type {Point} */ const unknown=at(1);
            await mobile.touchscreen.tap(unknown.x,unknown.y);
            await mobile.waitForFunction(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return s.scans===0&&s.digs===5;});
            await mobile.touchscreen.tap(iron.x,iron.y);
            await mobile.waitForFunction(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return !s.busy&&s.digs===4&&s.iron===2;});
            await mobile.screenshot({path:'.playwright-mcp/thorium-campaign-mining-mobile.png',scale:'css'});
        } finally {await context.close();}
        check(!errors.length,errors.join('; '));
        return {checks:['three authored campaigns expose 8/10/12 waves and 3/3/4 independent entrances','stage browsing preserves the live expedition; continue and confirmed new-stage reset use the correct content','wheel zoom, map pan, fit reset and enlarged placement/range preview share one transform','320px touch zoom, map pan and real combat-to-mining transitions retain usable controls and 24px cutaway tiles'],errors};
    } finally {page.off('pageerror',onError);}
}
