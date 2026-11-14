/**
 * Exercise the real SDL/WASM canvas using mouse, keyboard and actual touch events.
 * No test command can write game state; diagnostics are read-only.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,source:number,candidates:number[],cost:{iron:number,copper:number,crystal:number,gold:number}}} CardOffer */
    /** @typedef {{offers:{recruitment:CardOffer|null,reward:CardOffer|null},phase:number,wave:number,hp:number,energy:number,towers:number,units:number,gold:number,iron:number,copper:number,tool:number,digs:number,scans:number,busy:boolean,paused:boolean,width:number,height:number,time:number,modal:number,selectedCard:number,selectedUnit:number,selectedTower:number,cardMotion:{age:number,nextKind:number}[],hand:number[]}} GameState */
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{x:number,y:number}} Point */
    /** @type {string[]} */
    const errors=[];
    /** @type {string[]} */
    const checks=[];
    /** @type {(error:Error)=>void} */
    const onError=error=>errors.push(error.message);
    /** Require a gameplay or input invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read public run state without modifying WASM memory. @param {import('playwright').Page} target @returns {Promise<GameState>} */
    async function state(target){return target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for a public-state condition, evaluated in the browser. @param {import('playwright').Page} target @param {string} condition @param {number} timeout @returns {Promise<void>} */
    async function wait(target,condition,timeout=15000){
        try{await target.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout});}
        catch(error){throw new Error(`Gameplay condition failed: ${condition}; state=${JSON.stringify(await state(target))}`,{cause:error});}
    }
    /** Resolve a registered control's logical rectangle. @param {import('playwright').Page} target @param {number} id @returns {Promise<Widget>} */
    async function widget(target,id){return target.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);}
    /** Click or tap a real UI control, preserving the SDL input path. @param {import('playwright').Page} target @param {number} id @param {boolean} touch @returns {Promise<void>} */
    async function click(target,id,touch=false){
        await target.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await widget(target,id);check(item&&item.enabled,`Widget ${id} must be enabled`);
        if(touch)await target.touchscreen.tap(item.x+item.w/2,item.y+item.h/2);else await target.mouse.click(item.x+item.w/2,item.y+item.h/2);
    }
    /** Transform a logical surface cell using the same viewport dimensions. @param {import('playwright').Page} target @param {number} x @param {number} y @returns {Promise<Point>} */
    async function cell(target,x,y){
        /** @type {{originX:number,originY:number,tile:number}} */
        const field=await target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        return {x:field.originX+(x+.5)*field.tile,y:field.originY+(y+.5)*field.tile};
    }
    /** Resolve one mine tile in the current cutaway view. @param {import('playwright').Page} target @param {number} index @returns {Promise<Point>} */
    async function mineCell(target,index){
        /** @type {{x:number,y:number,tile:number,columns:number}} */ const mine=(await state(target)).mine;
        return {x:mine.x+(index%mine.columns+.5)*mine.tile,y:mine.y+(Math.floor(index/mine.columns)+.5)*mine.tile};
    }
    /** Click a mine tile and wait for its actual automatic job to finish. @param {number} index @param {number} expectedDigs @returns {Promise<void>} */
    async function dig(index,expectedDigs){
        /** @type {Point} */ const point=await mineCell(page,index);await page.mouse.click(point.x,point.y);
        await wait(page,`!s.busy&&s.digs===${expectedDigs}`);
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});
        // This scenario checks the tile-by-tile cutaway; a fresh profile would open an excavation wall.
        await page.addInitScript(()=>{if(location.origin==='http://127.0.0.1:8317'&&!localStorage.getItem('thorium_td_save'))localStorage.setItem('thorium_td_save','{"excavation":false}');});
        await page.goto('http://127.0.0.1:8317/');
        await wait(page,'s.width===1200&&s.height===900&&s.phase===0');
        check((await state(page)).energy===6,'Initial energy budget');await click(page,36);await wait(page,'s.screen===1');
        await click(page,10);await wait(page,'s.phase===1');
        /** @type {Widget} */ const gun=await widget(page,1001);
        /** @type {Point} */ const firstPad=await cell(page,6,4);
        await page.mouse.move(gun.x+gun.w/2,gun.y+gun.h/2);await page.mouse.down();await page.mouse.move(firstPad.x,firstPad.y,{steps:8});await page.mouse.up();
        await wait(page,'s.towers===1&&s.energy===3');
        await click(page,1002);
        /** @type {Point} */ const guard=await cell(page,8,4);await page.mouse.click(guard.x,guard.y);
        await wait(page,'s.units===1&&s.energy===0');
        checks.push('mouse drag and tap each commit one cost');
        await page.keyboard.press('Space');await wait(page,'s.paused');
        /** @type {number} */ const pausedTime=(await state(page)).time;
        await page.waitForTimeout(350);check((await state(page)).time===pausedTime,'Pause stops simulation time');
        await page.keyboard.press('Space');await wait(page,'!s.paused');
        await click(page,12);
        await wait(page,'s.energy>=3');
        await click(page,1005);
        /** @type {Point} */ const secondPad=await cell(page,9,4);await page.mouse.click(secondPad.x,secondPad.y);
        await wait(page,'s.towers===2');
        await page.screenshot({path:'.playwright-mcp/thorium-battle.png',scale:'css'});
        await wait(page,'s.phase===2',45000);
        check((await state(page)).digs===5,'Wave completion enters five-dig preparation');
        checks.push('real combat, pause/resume and preparation transition');
        await click(page,15);
        /** @type {Point} */ const scan=await mineCell(page,1);await page.mouse.click(scan.x,scan.y);
        await wait(page,'s.scans===0');check((await state(page)).digs===5,'Scan does not spend a dig');
        await dig(9,4);check((await state(page)).iron===2,'First real excavation grants iron');
        await dig(10,3);await dig(8,2);await dig(11,1);
        await click(page,17);await wait(page,'s.tool===2&&s.iron===0&&s.copper===0');
        await dig(12,0);check((await state(page)).gold===8,'Fifth dig with the crafted T2 pickaxe grants hard-rock gold');
        await page.screenshot({path:'.playwright-mcp/thorium-mining.png',scale:'css'});
        checks.push('scan, automatic digging and crafting resource ledger');
        await click(page,13);
        /** @type {Point} */ const survivor=await cell(page,8,4);await page.mouse.click(survivor.x,survivor.y);await wait(page,'s.selectedUnit!==0');
        await click(page,32);
        /** @type {Point} */ const move=await cell(page,8,6);await page.mouse.click(move.x,move.y);
        await click(page,33);await page.mouse.click(move.x,move.y);await wait(page,'s.selectedUnit!==0');
        await click(page,23);await wait(page,'s.modal===3');await click(page,25);await wait(page,'s.modal===0');
        check((await state(page)).units===1&&(await state(page)).gold===8,'Preparation movement and withdrawal cancel preserve survivor and wallet');
        await click(page,33);await click(page,14);
        checks.push('explicit preparation move and survivor withdrawal cancellation');
        /** @type {CardOffer} */ const offered=(await state(page)).offers.recruitment;
        check(offered&&offered.candidates.join(',')==='2,7,5,0'&&offered.cost.gold===6&&offered.shards===12,'Run publishes its actual recruitment candidates and price');
        await click(page,20);await wait(page,'s.modal===1');await click(page,3007);await wait(page,'s.modal===2');
        await click(page,25);await wait(page,'s.modal===0');check((await state(page)).gold===8,'Recruit cancel is free');
        check(JSON.stringify((await state(page)).offers.recruitment)===JSON.stringify(offered),'Cancel preserves the same run-owned offer, candidates and price');
        await click(page,20);await click(page,3007);await click(page,2008);await wait(page,'s.modal===0&&s.gold===2');
        check((await state(page)).offers.recruitment===null,'Successful replacement consumes the run-owned offer');
        await click(page,10);await wait(page,'s.phase===1&&s.wave===2');check((await state(page)).hand.includes(7),'New unit card enters next hand');check((await state(page)).cardMotion.some(card=>card.nextKind===7&&card.age<.85),'Recently recruited card is visibly dealt after preparation');
        checks.push('recruit cancel and atomic card replacement');
        await click(page,24);await click(page,30);await wait(page,'s.phase===0&&s.towers===0&&s.wave===0');

        /** @type {import('playwright').Browser} */ const browser=page.context().browser();check(browser,'Browser connection available');
        /** @type {import('playwright').BrowserContext} */ const mobileContext=await browser.newContext({viewport:{width:390,height:844},deviceScaleFactor:2,isMobile:true,hasTouch:true});
        /** @type {import('playwright').Page} */ const mobile=await mobileContext.newPage();mobile.on('pageerror',onError);
        await mobile.addInitScript(()=>{if(location.origin==='http://127.0.0.1:8317'&&!localStorage.getItem('thorium_td_save'))localStorage.setItem('thorium_td_save','{"excavation":false}');});
        try{
            await mobile.goto('http://127.0.0.1:8317/');await wait(mobile,'s.width===390&&s.height===844');await click(mobile,36,true);await wait(mobile,'s.screen===1');
            await click(mobile,56,true);await wait(mobile,'!s.guideOpen');
            /** @type {Widget[]} */ const controls=await mobile.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())));
            check(controls.every(item=>item.x>=0&&item.y>=0&&item.x+item.w<=391&&item.y+item.h<=845),'Mobile controls fit viewport');
            await mobile.screenshot({path:'.playwright-mcp/thorium-mobile.png',scale:'css'});
            await click(mobile,10,true);await click(mobile,1001,true);
            /** @type {Point} */ const mobilePad=await cell(mobile,6,4);await mobile.touchscreen.tap(mobilePad.x,mobilePad.y);
            await wait(mobile,'s.towers===1&&s.energy===3');
            await click(mobile,1002,true);
            /** @type {Point} */ const mobileUnit=await cell(mobile,8,4);await mobile.touchscreen.tap(mobileUnit.x,mobileUnit.y);await wait(mobile,'s.units===1&&s.energy===0');
            await mobile.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===1005&&item.enabled));
            /** @type {Widget} */ const nextCard=await widget(mobile,1005);
            /** @type {import('playwright').CDPSession} */ const touch=await mobileContext.newCDPSession(mobile);
            await touch.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:nextCard.x+20,y:nextCard.y+20,id:42}]});
            await touch.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:mobilePad.x,y:mobilePad.y,id:42}]});
            await wait(mobile,'s.placement?.card===5&&s.placement.range===3');
            await mobile.screenshot({path:'.playwright-mcp/thorium-touch-range.png',scale:'css'});
            await touch.send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
            check((await state(mobile)).towers===1,'Canceled touch drag cannot place a card');
            await click(mobile,1005,true);await wait(mobile,'s.selectedCard!==0');
            await click(mobile,1005,true);await wait(mobile,'s.selectedCard===0');
            await mobile.touchscreen.tap(mobilePad.x,mobilePad.y);await wait(mobile,'s.selectedTower!==0');
            await mobile.screenshot({path:'.playwright-mcp/thorium-mobile-selected.png',scale:'css'});
            await click(mobile,23,true);await wait(mobile,'s.modal===3');
            await click(mobile,25,true);await wait(mobile,'s.modal===0');check((await state(mobile)).towers===1,'Touch cancel preserves tower');
            await click(mobile,23,true);await wait(mobile,'s.modal===3');await click(mobile,29,true);await wait(mobile,'s.towers===0');
            checks.push('touch selection, card deselection, withdrawal confirmation and cancel');
            checks.push('390px touch UI, no synthetic double use, touch cancellation');
        }finally{await mobileContext.close();}
        check(errors.length===0,errors.join('; '));
        await page.screenshot({path:'.playwright-mcp/thorium-ready.png',scale:'css'});
        return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
