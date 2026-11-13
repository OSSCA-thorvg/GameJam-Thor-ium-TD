/**
 * Verify placement preflight through real SDL mouse input and read-only diagnostics.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{x:number,y:number}} Point */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** @type {(error:Error)=>void} */ const onError=error=>errors.push(error.message);
    /** Require a real-input invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Wait for a read-only snapshot predicate. @param {string} condition @returns {Promise<void>} */
    async function wait(condition){await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout:15000});}
    /** Resolve a registered SDL control. @param {number} id @returns {Promise<Widget>} */
    async function widget(id){return page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);}
    /** Click the current enabled control. @param {number} id @returns {Promise<void>} */
    async function click(id){
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await widget(id);
        await page.mouse.click(item.x+item.w/2,item.y+item.h/2);
    }
    /** Transform a surface cell using the same viewport geometry. @param {number} x @param {number} y @returns {Promise<Point>} */
    async function cell(x,y){
        /** @type {{originX:number,originY:number,tile:number}} */
        const field=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        return {x:field.originX+(x+.5)*field.tile,y:field.originY+(y+.5)*field.tile};
    }
    /** Begin an actual card drag and hold it over a target. @param {number} id @param {Point} target @returns {Promise<void>} */
    async function drag(id,target){
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await widget(id);
        await page.mouse.move(item.x+item.w/2,item.y+item.h/2);
        await page.mouse.down();await page.mouse.move(target.x,target.y,{steps:3});
    }
    /** Confirm current energy is physically adjacent to every hand card. @returns {Promise<void>} */
    async function adjacentEnergy(){
        /** @type {{x:number,y:number,w:number,h:number}} */ const bar=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).energyBar);
        /** @type {Widget[]} */ const cards=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).filter(item=>item.id>1000&&item.id<2000));
        check(bar&&cards.length===4&&cards.every(item=>item.y+item.h<=bar.y&&bar.y-item.y-item.h<=20),'Energy sits directly below all four cards');
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});
        await page.goto('http://127.0.0.1:8317/');await wait('s.width===1200&&s.height===900');
        await click(36);await click(10);await wait('s.phase===1');
        await adjacentEnergy();
        /** @type {Point} */ const pad=await cell(6,4);
        /** @type {Point} */ const invalid=await cell(5,5);
        await drag(1001,pad);await wait('s.placement?.allowed&&s.placement.action===1&&s.placement.cost===3&&s.placement.range===3');
        await page.screenshot({path:'.playwright-mcp/thorium-drag-range.png',scale:'css'});
        await page.mouse.move(invalid.x,invalid.y);
        await wait('s.placement&&!s.placement.allowed&&s.placement.reason===4&&s.towers===0&&s.handAvailability.some(card=>card.card===1)');
        await page.screenshot({path:'.playwright-mcp/thorium-placement-invalid.png',scale:'css'});
        await page.mouse.move(pad.x,pad.y);await wait('s.placement?.allowed&&s.placement.action===1');
        await page.mouse.up();await wait('s.towers===1&&!s.handAvailability.some(card=>card.card===1)&&s.placement===null');
        checks.push('drag preview reports install cost and invalid pad without committing; release commits once');

        // Spend through actual exchanges to expose the lower-priced existing-tower upgrade.
        for (const id of [1005,1002,1004]) {await click(id);await click(16);}
        await wait('s.energy===2&&s.handAvailability.some(card=>card.card===8&&card.allowed&&card.minimumCost===2&&card.maximumCost===3)');
        await drag(1008,pad);await wait('s.placement?.allowed&&s.placement.action===2&&s.placement.cost===2');
        /** @type {Point} */ const empty=await cell(9,4);
        await page.mouse.move(empty.x,empty.y);
        await wait('s.placement&&!s.placement.allowed&&s.placement.action===1&&s.placement.cost===3&&s.placement.reason===10');
        await page.mouse.move(pad.x,pad.y);await wait('s.placement?.allowed&&s.placement.action===2&&s.placement.cost===2');
        await page.screenshot({path:'.playwright-mcp/thorium-placement-upgrade.png',scale:'css'});
        await page.mouse.up();await wait('s.towers===1&&!s.handAvailability.some(card=>card.card===8)&&s.placement===null');
        checks.push('energy two enables the cheaper upgrade while empty-pad install reports energy shortage');

        await click(1001);await page.mouse.move(pad.x,pad.y);await wait('s.placement?.card===1');
        await page.keyboard.press('Space');await wait('s.paused&&s.placement===null');
        await click(31);await wait('!s.paused');
        await click(38);await wait('s.screen!==1&&s.placement===null');
        await page.keyboard.press('Escape');await wait('s.screen===1');
        checks.push('pause and settings suppress placement hints');

        await page.setViewportSize({width:320,height:568});await wait('s.width===320&&s.height===568');
        await adjacentEnergy();
        if (!await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).selectedCard===1)) await click(1001);
        /** @type {Point} */ const mobilePad=await cell(6,4);
        await page.mouse.move(mobilePad.x,mobilePad.y);await wait('s.placement?.card===1&&s.placement.action===2&&s.placement.cost===3');
        await page.screenshot({path:'.playwright-mcp/thorium-placement-mobile.png',scale:'css'});
        check(errors.length===0,errors.join('; '));
        checks.push('320px layout preserves the actual target and effective cost after resize');
        checks.push('current energy remains directly beneath cards and drag previews expose the real attack radius');
        return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
