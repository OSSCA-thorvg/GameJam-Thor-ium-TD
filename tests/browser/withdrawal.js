/**
 * Verify field selection, confirmation, withdrawal and responsive hit regions through SDL input.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{phase:number,energy:number,towers:number,units:number,time:number,modal:number,selectedTower:number,selectedUnit:number,selectedCard:number,width:number,height:number,hand:number[]}} State */
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean,focusable:boolean}} Widget */
    /** @type {string[]} */ const errors=[];
    /** @type {string[]} */ const checks=[];
    /** Collect browser runtime failures. @param {Error} error @returns {void} */
    const onError=error=>errors.push(error.message);
    /** Assert an observable player-facing invariant. @param {unknown} value @param {string} message @returns {void} */
    function check(value,message){if(!value)throw new Error(message);}
    /** Read the public snapshot, without writing WASM state. @returns {Promise<State>} */
    async function state(){return page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));}
    /** Wait for simulation or input delivery. @param {string} condition @returns {Promise<void>} */
    async function wait(condition){await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&typeof Module._thorium_debug_state==='function'&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);}
    /** Resolve current hit regions after the next UI frame. @param {number} id @returns {Promise<Widget>} */
    async function widget(id){
        await page.waitForFunction(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(item=>item.id===id&&item.enabled),id);
        return page.evaluate(id=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(item=>item.id===id),id);
    }
    /** Click an actual SDL control. @param {number} id @returns {Promise<void>} */
    async function click(id){/** @type {Widget} */ const item=await widget(id);await page.mouse.click(item.x+item.w/2,item.y+item.h/2);}
    /** Click a world cell using its registered viewport transform. @param {number} x @param {number} y @returns {Promise<void>} */
    async function cell(x,y){
        /** @type {Widget} */ const world=await widget(1);
        /** @type {number} */ const tile=Math.min((world.w-24)/18,(world.h-32)/10);
        await page.mouse.click(world.x+(world.w-tile*18)/2+(x+.5)*tile,world.y+(world.h-tile*10)/2+(y+.5)*tile);
    }
    /** Ensure enabled controls fit and do not intercept each other. @returns {Promise<void>} */
    async function fit(){
        /** @type {Widget[]} */ const controls=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).filter(item=>item.enabled&&item.focusable));
        /** @type {State} */ const current=await state();
        check(controls.every(item=>item.x>=0&&item.y>=0&&item.x+item.w<=current.width+1&&item.y+item.h<=current.height+1),'Controls fit the viewport');
        for(let i=0;i<controls.length;++i)for(let j=i+1;j<controls.length;++j){
            /** @type {Widget} */ const a=controls[i],b=controls[j];
            check(a.x+a.w<=b.x+.5||b.x+b.w<=a.x+.5||a.y+a.h<=b.y+.5||b.y+b.h<=a.y+.5,`Controls ${a.id}/${b.id} overlap`);
        }
    }
    page.on('pageerror',onError);
    try{
        await page.setViewportSize({width:1200,height:900});
        await page.goto('http://127.0.0.1:8317/');await wait('s.width===1200&&s.phase===0');await click(36);await wait('s.screen===1');
        await fit();await click(10);await click(1001);await cell(6,4);await wait('s.towers===1&&s.energy===3');
        await cell(6,4);await wait('s.selectedTower!==0');await widget(23);await fit();
        await page.screenshot({path:'.playwright-mcp/thorium-selected.png',scale:'css'});
        await click(23);await wait('s.modal===3');
        /** @type {State} */ const before=await state();
        await page.waitForTimeout(300);await page.keyboard.press('2');
        check((await state()).time===before.time,'Confirmation freezes combat time');
        check((await state()).selectedCard===0,'Confirmation blocks underlying card hotkeys');
        await page.screenshot({path:'.playwright-mcp/thorium-withdrawal.png',scale:'css'});
        await click(25);await wait('s.modal===0');check((await state()).towers===1,'Cancel preserves the building');
        await click(23);await wait('s.modal===3');
        /** @type {State} */ const committed=await state();
        await click(29);await wait('s.towers===0&&s.selectedTower===0&&s.modal===0');
        check((await state()).energy===committed.energy,'Building withdrawal gives no refund');
        check(JSON.stringify((await state()).hand)===JSON.stringify(committed.hand),'Withdrawal preserves hand order');
        await click(1002);await cell(8,4);await wait('s.units===1');
        await cell(8,4);await wait('s.selectedUnit!==0');await click(23);await wait('s.modal===3');
        /** @type {number} */ const energy=(await state()).energy;
        await click(29);await wait('s.units===0&&s.selectedUnit===0');check((await state()).energy===energy,'Unit withdrawal frees capacity without a refund');
        checks.push('tower/unit selection and withdrawal, cancel, frozen confirmation, no resource/card refund');
        for(const size of [{width:980,height:739},{width:390,height:844},{width:320,height:568}]){
            await page.setViewportSize(size);await wait(`s.width===${size.width}&&s.height===${size.height}`);await fit();
        }
        checks.push('enabled hit regions fit without overlap at 1200, 980, 390 and 320px');
        check(errors.length===0,errors.join('; '));
        return {checks,errors};
    }finally{page.off('pageerror',onError);}
}
