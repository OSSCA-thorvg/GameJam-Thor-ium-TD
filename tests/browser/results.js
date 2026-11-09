/**
 * Reach a real defeat without state injection, then restart using the result-screen SDL button.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @type {string[]} */ const errors=[];
    /** @type {(error:Error)=>void} */ const onError=error=>errors.push(error.message);
    /** Require a player-visible contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Wait for the normal simulation or SDL event path to complete. @param {string} condition @returns {Promise<void>} */
    async function wait(condition){await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout:60000});}
    /** Click one current enabled UI control without writing game memory. @param {number} id @returns {Promise<void>} */
    async function click(id){
        if(id===10){
            await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===10));
            /** @type {Widget|undefined} */ const guide=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===56&&item.enabled));
            if(guide){await page.mouse.click(guide.x+guide.w/2,guide.y+guide.h/2);await wait('!s.guideOpen');}
        }
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        /** @type {Widget} */ const item=await page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);
        await page.mouse.click(item.x+item.w/2,item.y+item.h/2);
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:640,height:640});
        await page.goto('http://127.0.0.1:8317/');await wait('s.width===640&&s.screen===0');
        await click(36);await click(10);await click(12);
        await wait('s.phase===2&&s.wave===1');
        await click(10);await wait('s.phase===4&&s.hp===0');
        await page.setViewportSize({width:320,height:568});await wait('s.width===320&&s.height===568');
        await page.screenshot({path:'.playwright-mcp/thorium-result-defeat.png',scale:'css'});
        await click(30);
        await wait('s.phase===0&&s.wave===0&&s.hp===20&&s.screen===1&&s.modal===0&&s.towers===0&&s.units===0&&s.actors.length===0');
        await click(10);await wait('s.phase===1&&s.wave===1');
        check(errors.length===0,errors.join('; '));
        return {checks:['natural defeat exposes an enabled result restart control at 320px','result restart clears the old run and starts a playable first wave through normal input'],errors};
    }finally{page.off('pageerror',onError);}
}
