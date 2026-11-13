/**
 * Verify animation delivery through real mouse/keyboard input and read-only WASM diagnostics.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{old:number,next:number,oldKind:number,nextKind:number,age:number,busy:boolean}} Motion */
    /** @typedef {{type:number,subject:number,age:number}} Cue */
    /** @typedef {{energy:number,towers:number,paused:boolean,selectedCard:number,feedbackChanges:number,feedbackHits:number,rejectedCard:number,rejectionAge:number,cardMotion:Motion[],combatCues:Cue[]}} State */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** Collect unexpected browser errors. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    /** Require an observable contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read public state without controlling game clocks or commands. @returns {Promise<State>} */
    async function state(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Await a read-only condition after packaged runtime initialization. @param {string} condition @returns {Promise<void>} */
    async function wait(condition){await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);}
    /** Read current logical control geometry. @param {number} id @returns {Promise<Widget>} */
    async function widget(id){return page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);}
    /** Click a ready control through SDL. @param {number} id @returns {Promise<void>} */
    async function click(id){await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);/** @type {Widget} */ const item=await widget(id);await page.mouse.click(item.x+item.w/2,item.y+item.h/2);}
    /** Click one logical surface cell. @param {number} x @param {number} y @returns {Promise<void>} */
    async function cell(x,y){
        /** @type {{originX:number,originY:number,tile:number}} */
        const field=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        await page.mouse.click(field.originX+(x+.5)*field.tile,field.originY+(y+.5)*field.tile);
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});await page.goto('http://127.0.0.1:8317/');await wait('s.cardMotion&&s.cardMotion.length===4');
        check((await state()).feedbackChanges===0,'Initial hand must not falsely animate a replacement');
        await click(36);await wait('s.screen===1');await click(10);await click(1001);await cell(6,4);
        await wait('s.cardMotion[0].next===5&&s.cardMotion[0].busy');
        /** @type {State} */ const drawn=await state();
        check(drawn.cardMotion[0].old===1&&drawn.cardMotion[0].oldKind===drawn.cardMotion[0].nextKind,'Same-kind cards keep distinct outgoing/incoming identities');
        check(drawn.energy===3&&drawn.feedbackChanges===1,'Exactly one successful use starts one replacement');
        /** @type {Widget} */ const arrival=await widget(1005);check(!arrival.enabled,'Arriving face is not yet clickable');
        await page.mouse.move(arrival.x+arrival.w/2,arrival.y+arrival.h/2);await page.mouse.down();
        await page.keyboard.press('1');check((await state()).selectedCard===0,'Hotkey cannot select a card before its arrival');
        await page.screenshot({path:'.playwright-mcp/thorium-card-exit.png',scale:'css'});
        // Press while unavailable, release after arrival: this gesture must not use the new card.
        await wait('!s.cardMotion[0].busy');await page.mouse.up();
        check((await state()).selectedCard===0,'A held disabled press cannot become a fresh card click');
        await page.screenshot({path:'.playwright-mcp/thorium-card-draw.png',scale:'css'});
        await wait('s.cardMotion[0].age>=.85');
        checks.push('same-kind replacement has exit/arrival/highlight, one cost, and guarded input');
        await click(1003);await click(16);await wait('s.cardMotion[2].next===6');
        /** @type {State} */ const exchanged=await state();
        check(exchanged.cardMotion[2].oldKind!==exchanged.cardMotion[2].nextKind&&exchanged.feedbackChanges===2,'Exchange also animates the new definition');
        await wait('s.cardMotion[2].age>=.85');
        await click(1005);await cell(0,0);await wait('s.rejectedCard===5&&s.rejectionAge<.32');
        check((await state()).feedbackChanges===2&&(await state()).towers===1,'Invalid placement gives rejection feedback without a draw or placement');
        await page.keyboard.press('Escape');
        checks.push('manual exchange animates, rejected placement preserves the hand');
        await wait('s.feedbackHits>0&&s.combatCues.some(c=>c.type===1&&c.age<.15)');
        await page.keyboard.press('Space');await wait('s.paused');
        /** @type {string} */ const paused=JSON.stringify((await state()).combatCues);
        await page.waitForTimeout(220);check(JSON.stringify((await state()).combatCues)===paused,'Paused combat preserves cue time');
        await page.keyboard.press('Space');await wait('!s.paused');
        await wait('s.combatCues.some(c=>c.type===1&&c.age<.15)');
        await page.screenshot({path:'.playwright-mcp/thorium-impact.png',scale:'css'});
        checks.push('actual attacks produce timed hit feedback and pause/resume preserves it');
        check(errors.length===0,errors.join('; '));return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
