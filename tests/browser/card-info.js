/**
 * Inspect real hand, recruitment and replacement cards using mouse hover and held touch input.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{kind:number,name:string,cost:number,stats:{label:string,value:string}[]}} CardDetails */
    /** @typedef {{phase:number,modal:number,energy:number,gold:number,selectedCard:number,width:number,height:number,cardInfo:null|{widget:number,details:CardDetails,x:number,y:number,w:number,h:number},offeredCard:CardDetails|null}} State */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** Collect browser failures. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    /** Require a visible interaction contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read diagnostics without modifying game state. @param {import('playwright').Page} target @returns {Promise<State>} */
    async function state(target){return target.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for actual SDL/animation delivery. @param {import('playwright').Page} target @param {string} condition @returns {Promise<void>} */
    async function wait(target,condition){await target.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,null,{timeout:60000});}
    /** Resolve the current enabled control. @param {import('playwright').Page} target @param {number} id @returns {Promise<Widget>} */
    async function widget(target,id){
        await target.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        return target.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);
    }
    /** Click a normal mouse control. @param {number} id @returns {Promise<void>} */
    async function click(id){/** @type {Widget} */ const item=await widget(page,id);await page.mouse.click(item.x+item.w/2,item.y+item.h/2);}
    /** Hover one real card and await its shared detail panel. @param {number} id @returns {Promise<void>} */
    async function hover(id){/** @type {Widget} */ const item=await widget(page,id);await page.mouse.move(item.x+item.w/2,item.y+item.h/2);await wait(page,`s.cardInfo?.widget===${id}`);}
    /** Dig one board cell through the visible world transform. @param {number} index @param {number} digs @returns {Promise<void>} */
    async function dig(index,digs){
        /** @type {Widget} */ const area=await widget(page,1);
        /** @type {number} */ const tile=Math.min((area.w-40)/5,(area.h-42)/6);
        await page.mouse.click(area.x+(area.w-5*tile)/2+(index%5+.5)*tile,area.y+(area.h-6*tile)/2+8+(Math.floor(index/5)+.5)*tile);
        await wait(page,`!s.busy&&s.digs===${digs}`);
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});await page.goto('http://127.0.0.1:8317/');
        await wait(page,'s.width===1200');await click(36);
        await hover(1001);
        check((await state(page)).cardInfo.details.stats.some(stat=>stat.label==='공격력'&&stat.value==='12'),'Hover displays the real attack value');
        check((await state(page)).cardInfo.details.stats.some(stat=>stat.label==='사거리'&&stat.value==='3칸'),'Hover displays the real attack range');
        await page.screenshot({path:'.playwright-mcp/thorium-card-tooltip.png',scale:'css'});
        await page.mouse.move(2,2);await wait(page,'s.cardInfo===null');
        /** @type {Widget} */ const guard=await widget(page,1002);
        await page.mouse.move(guard.x+guard.w/2,guard.y+guard.h/2);await page.mouse.down();
        await wait(page,'s.cardInfo?.widget===1002');await page.mouse.up();
        await wait(page,'s.selectedCard===0&&s.energy===6');
        await page.mouse.move(2,2);
        checks.push('mouse hover and held click show real base stats without selecting or spending');

        await click(10);await click(12);await wait(page,'s.phase===2');
        await dig(6,4);await dig(7,3);await dig(11,2);await dig(12,1);await click(17);await dig(13,0);
        await click(20);await wait(page,'s.modal===1');await hover(3006);
        await page.screenshot({path:'.playwright-mcp/thorium-card-candidates.png',scale:'css'});
        await click(3006);await wait(page,'s.modal===2&&s.offeredCard?.kind===6');
        check((await state(page)).offeredCard.stats.some(stat=>stat.label==='체력'&&stat.value==='70'),'Incoming card includes its unit performance before replacement');
        await page.mouse.move(2,2);await wait(page,'s.cardInfo===null');
        await page.screenshot({path:'.playwright-mcp/thorium-card-replacement.png',scale:'css'});
        await hover(2001);check((await state(page)).gold===4,'Inspecting a replacement target never pays');
        await page.screenshot({path:'.playwright-mcp/thorium-card-compare.png',scale:'css'});
        await page.setViewportSize({width:320,height:568});await wait(page,'s.width===320&&s.height===568');
        await hover(2001);
        /** @type {State} */ const comparison=await state(page);
        check(comparison.cardInfo.y>=0&&comparison.cardInfo.y+comparison.cardInfo.h<=568&&comparison.offeredCard.kind===6,'Compact comparison keeps incoming card and inspected target in the same screen');
        await page.screenshot({path:'.playwright-mcp/thorium-card-compare-320.png',scale:'css'});
        await click(25);await wait(page,'s.modal===0&&s.offeredCard===null&&s.cardInfo===null&&s.gold===4');
        checks.push('recruitment and incoming-card comparison show portraits, stats and free inspection before payment');

        /** @type {import('playwright').Browser} */ const browser=page.context().browser();
        /** @type {import('playwright').BrowserContext} */ const context=await browser.newContext({viewport:{width:390,height:844},hasTouch:true,isMobile:true,deviceScaleFactor:2});
        /** @type {import('playwright').Page} */ const mobile=await context.newPage();mobile.on('pageerror',onError);
        try{
            await mobile.goto('http://127.0.0.1:8317/');await wait(mobile,'s.width===390');
            /** @type {Widget} */ const start=await widget(mobile,36);await mobile.touchscreen.tap(start.x+start.w/2,start.y+start.h/2);
            /** @type {import('playwright').CDPSession} */ const touch=await context.newCDPSession(mobile);
            for (const size of [{width:390,height:844},{width:320,height:568}]) {
                await mobile.setViewportSize(size);await wait(mobile,`s.width===${size.width}&&s.height===${size.height}`);
                /** @type {Widget} */ const card=await widget(mobile,1002);
                await touch.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:card.x+card.w/2,y:card.y+card.h/2,id:42}]});
                await wait(mobile,'s.cardInfo?.widget===1002');
                await touch.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
                await wait(mobile,'s.cardInfo?.widget===1002&&s.selectedCard===0&&s.energy===6');
                /** @type {State} */ const held=await state(mobile);
                check(held.cardInfo.x>=0&&held.cardInfo.y>=0&&held.cardInfo.x+held.cardInfo.w<=size.width&&held.cardInfo.y+held.cardInfo.h<=size.height,'Touch detail panel stays inside the viewport');
                await mobile.screenshot({path:`.playwright-mcp/thorium-card-info-${size.width}.png`,scale:'css'});
                await mobile.touchscreen.tap(2,2);await wait(mobile,'s.cardInfo===null&&s.selectedCard===0');
            }
            checks.push('actual 390px and 320px held touch opens bounded details; release and dismiss never play the card');
        }finally{await context.close();}
        check(errors.length===0,errors.join('; '));
        return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
