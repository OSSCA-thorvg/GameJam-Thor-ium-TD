/**
 * Verify independent actor phases and shared-frame rendering through real UI and read-only diagnostics.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,state:number,phase:number,time:number,frame:number}} Actor */
    /** @typedef {{error:string,cached:boolean,bytes:number,entries:number,bakes:number,hits:number,cachedDraws:number,sprites:number,bakerSources:number,pinned:number}} Rendering */
    /** @typedef {{phase:number,screen:number,width:number,height:number,paused:boolean,time:number,actors:Actor[],rendering:Rendering}} State */
    /** @type {string[]} */ const checks = [];
    /** @type {string[]} */ const errors = [];
    /** Collect runtime failures. @param {Error} error @returns {void} */
    const onError = error => errors.push(error.message);
    /** Require an externally observable contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value, message) { if (!value) throw new Error(message); }
    /** Read diagnostics without writing clocks or invoking game commands. @returns {Promise<State>} */
    async function state() { return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()))); }
    /** Wait for initialization and a diagnostic condition. @param {string} condition @returns {Promise<void>} */
    async function wait(condition) {
        try {
            await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);
        } catch (error) {
            throw new Error(`Shared-frame condition failed: ${condition}; state=${JSON.stringify(await state())}`, {cause: error});
        }
    }
    /** Click one current enabled control via SDL input. @param {number} id @returns {Promise<void>} */
    async function click(id) {
        await page.waitForFunction(id => JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(widget => widget.id === id && widget.enabled), id);
        /** @type {{x:number,y:number,w:number,h:number}} */
        const widget = await page.evaluate(id => JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(widget => widget.id === id), id);
        await page.mouse.click(widget.x + widget.w / 2, widget.y + widget.h / 2);
    }
    page.on('pageerror', onError);
    try {
        await page.setViewportSize({width: 1200, height: 900});
        await page.goto('http://127.0.0.1:8317/');
        await wait('s.screen===0&&s.rendering');
        check(!(await state()).rendering.cached && (await state()).rendering.bytes === 0,
              'Measured vector default does not allocate an unused frame cache on startup');
        await click(38); await click(50); await click(53);
        await wait('s.rendering.cached&&s.rendering.entries>0');
        /** @type {Rendering} */ const warmed = (await state()).rendering;
        check(warmed.bakerSources === 1 && warmed.bytes <= 32 * 1024 * 1024,
              'Current source is parsed once for bounded shared-frame preparation');
        await click(53); await wait('!s.rendering.cached'); await click(41);
        await click(36); await click(10); await click(12);
        await wait('s.actors.filter(actor=>actor.state===1).length>=3');
        /** @type {Actor[]} */ const moving = (await state()).actors.filter(actor => actor.state === 1);
        check(new Set(moving.map(actor => actor.phase)).size === moving.length &&
                  new Set(moving.map(actor => actor.frame.toFixed(1))).size > 1,
              'Repeated monsters have fixed distinct phases instead of moving in lockstep');
        await page.keyboard.press('Space'); await wait('s.paused');
        /** @type {string} */ const frozen = JSON.stringify((await state()).actors);
        await click(38); await click(50); await click(53); await wait('s.rendering.cached'); await click(41);
        await wait('s.screen===1&&s.paused&&s.rendering.sprites>=3');
        check(JSON.stringify((await state()).actors) === frozen,
              'Switching rendering backends does not reset actor clocks, phases or game positions');
        check((await state()).rendering.bakes === warmed.bakes,
              'Prepared poses are reused across actors and navigation without combat-time rebaking');
        await page.screenshot({path: '.playwright-mcp/thorium-shared-frames.png', scale: 'css'});
        /** @type {number} */ const resumedAt = (await state()).time;
        await page.keyboard.press('Space'); await wait('!s.paused');
        await wait(`s.time>${resumedAt + .12}&&s.rendering.cachedDraws>10`);
        check((await state()).rendering.hits > 10, 'Active battle shares already baked source frames');
        checks.push('deterministic per-monster phase variation and cache reuse across live rendering switches');
        await page.keyboard.press('Space'); await wait('s.paused'); await click(42);
        await click(36); await click(30); await wait('s.screen===1&&s.phase===0');
        check((await state()).actors.length === 0 && (await state()).rendering.pinned === 0 &&
                  (await state()).rendering.entries === warmed.entries,
              'New run releases actor image references while preserving reusable frame pixels');
        await page.setViewportSize({width: 320, height: 568});
        await wait('s.width===320&&s.height===568');
        await click(38); await click(50); await click(48); await wait('!s.rendering.cached');
        await click(53); await wait('s.rendering.cached'); await click(41); await click(56); await click(10);
        await wait('s.actors.length>0&&s.rendering.sprites>0');
        check((await state()).rendering.bakes === warmed.bakes, 'Compact battle reuses the prepared 32px source frames');
        await page.screenshot({path: '.playwright-mcp/thorium-shared-frames-mobile.png', scale: 'css'});
        checks.push('session cache persistence, safe reset, 320px rendering preference and compact cached battle');
        check((await state()).rendering.error === '', 'Final renderer remains healthy after sustained independent poses');
        check(errors.length === 0, errors.join('; '));
        return {checks, errors};
    } finally { page.off('pageerror', onError); }
}
