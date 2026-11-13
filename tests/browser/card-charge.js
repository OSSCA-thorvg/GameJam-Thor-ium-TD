/**
 * Exercise energy coverage on real hand cards, including pause, compact layout and the ready transition.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{card:number,cost:number,mode:number,fraction:number}} Charge */
    /** @typedef {{time:number,energy:number,paused:boolean,handCharge:Charge[]}} Snapshot */
    /** @type {string[]} */ const errors=[];
    /** Collect browser runtime failures. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    /** Require an actual UI contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read current diagnostics without changing energy or clocks. @returns {Promise<Snapshot>} */
    async function state(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for normal game progression or SDL delivery. @param {string} condition @returns {Promise<void>} */
    async function wait(condition){
        try{await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout:20000});}
        catch(error){throw new Error(`Card charge condition failed: ${condition}; state=${JSON.stringify(await state())}`,{cause:error});}
    }
    /** Resolve a current enabled widget. @param {number} id @returns {Promise<Widget>} */
    async function widget(id){
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        return page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);
    }
    /** Click a real SDL control. @param {number} id @returns {Promise<void>} */
    async function click(id){/** @type {Widget} */ const item=await widget(id);await page.mouse.click(item.x+item.w/2,item.y+item.h/2);}
    /** Place the selected card using the public world rectangle. @param {number} x @param {number} y @returns {Promise<void>} */
    async function cell(x,y){
        /** @type {{originX:number,originY:number,tile:number}} */
        const field=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        await page.mouse.click(field.originX+(x+.5)*field.tile,field.originY+(y+.5)*field.tile);
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});await page.goto('http://127.0.0.1:8317/');
        await wait('s.width===1200');await click(36);await click(10);
        await click(1001);await cell(6,4);await wait('s.towers===1');
        await click(1002);await cell(8,4);await wait('s.units===1');
        await wait('s.handCharge.some(card=>card.card===3&&card.mode===1&&card.fraction>=.5)');
        /** @type {Charge} */ const half=(await state()).handCharge.find(card=>card.card===3);
        check(half.cost===4&&half.fraction<1,'Four-cost card has partial coverage until enough real energy exists');
        await page.screenshot({path:'.playwright-mcp/thorium-card-charge-half.png',scale:'css'});
        await page.keyboard.press('Space');await wait('s.paused&&s.performance.latest.steps===0');
        /** @type {Snapshot} */ const paused=await state();
        /** @type {number} */ const frozen=paused.handCharge.find(card=>card.card===3).fraction;
        await page.waitForTimeout(300);
        check((await state()).time===paused.time&&(await state()).handCharge.find(card=>card.card===3).fraction===frozen,'Pause freezes the card clock with energy regeneration');
        await page.setViewportSize({width:320,height:568});await wait('s.width===320&&s.height===568');
        check((await state()).handCharge.find(card=>card.card===3).fraction===frozen,'Resize preserves progress rather than restarting a cooldown');
        await page.keyboard.press('Space');await wait('!s.paused&&s.handCharge.some(card=>card.card===3&&card.mode===1)');
        await page.screenshot({path:'.playwright-mcp/thorium-card-charge-320.png',scale:'css'});
        await wait('s.handCharge.some(card=>card.card===3&&card.mode===0&&card.fraction===1)&&s.energy>=4');
        check(errors.length===0,errors.join('; '));
        return {checks:['actual energy and fractional regeneration drive per-card charge','pause and 320px resize preserve the same progress','enough energy turns the card ready without an independent timer'],errors};
    }finally{page.off('pageerror',onError);}
}
