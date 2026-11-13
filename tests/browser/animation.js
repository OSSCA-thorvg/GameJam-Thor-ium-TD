/**
 * Verify real enemy state transitions, game-clock playback, pause and visual-only death tails.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{id:number,kind:number,state:number,time:number,frame:number,x:number,y:number}} Actor */
    /** @typedef {{id:number,x:number,y:number,w:number,h:number,enabled:boolean}} Widget */
    /** @typedef {{phase:number,screen:number,time:number,paused:boolean,enemies:number,defeated:number,deathVisuals:number,actors:Actor[]}} State */
    /** @type {string[]} */ const checks = [];
    /** @type {string[]} */ const errors = [];
    /** Collect unexpected runtime errors. @param {Error} error @returns {void} */
    const onError = error => errors.push(error.message);
    /** Assert one externally observable contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value, message) { if (!value) throw new Error(message); }
    /** Read diagnostics without writing game state or calling commands. @returns {Promise<State>} */
    async function state() { return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()))); }
    /** Wait for a public condition after runtime initialization. @param {string} condition @returns {Promise<void>} */
    async function wait(condition) {
        try {
            await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);
        } catch (error) {
            throw new Error(`Animation condition failed: ${condition}; state=${JSON.stringify(await state())}`, {cause: error});
        }
    }
    /** Click current registered UI geometry through SDL input. @param {number} id @returns {Promise<void>} */
    async function click(id) {
        await page.waitForFunction(id => JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(widget => widget.id === id && widget.enabled), id);
        /** @type {Widget} */ const widget = await page.evaluate(id => JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(widget => widget.id === id), id);
        await page.mouse.click(widget.x + widget.w / 2, widget.y + widget.h / 2);
    }
    /** Target a surface cell using the measured world geometry. @param {number} x @param {number} y @returns {Promise<void>} */
    async function cell(x, y){
        /** @type {{originX:number,originY:number,tile:number}} */
        const field=await page.evaluate(()=>JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).field);
        await page.mouse.click(field.originX+(x+.5)*field.tile,field.originY+(y+.5)*field.tile);
    }
    page.on('pageerror', onError);
    try {
        await page.setViewportSize({width: 1200, height: 900});
        await page.goto('http://127.0.0.1:8317/');
        await wait('s.screen===0&&Array.isArray(s.actors)');
        check((await state()).actors.length === 0, 'Title does not invent living game actors');
        await click(36); await click(10); await click(1002); await cell(8, 4);
        await wait('s.actors.some(actor=>actor.state===1&&actor.time>.05)');
        await click(12);
        /** @type {State} */ const before = await state();
        /** @type {Actor} */ const moving = before.actors.find(actor => actor.state === 1);
        await page.waitForTimeout(160);
        /** @type {State} */ const after = await state();
        /** @type {Actor} */ const progressed = after.actors.find(actor => actor.id === moving.id);
        check(progressed && progressed.state === 1 && progressed.frame >= 0 && progressed.frame < 45,
              'Moving enemy stays inside its movement source segment');
        /** @type {number} */ const clipDelta = (progressed.time - moving.time + 1.5) % 1.5;
        check(Math.abs(clipDelta - (after.time - before.time)) < .025,
              'Animation uses the same simulated delta as the sped-up battle');
        checks.push('per-entity movement loop advances on the fixed game clock and respects battle speed');
        await page.keyboard.press('Space'); await wait('s.paused');
        /** @type {string} */ const frozen = JSON.stringify((await state()).actors);
        await page.waitForTimeout(180);
        check(JSON.stringify((await state()).actors) === frozen, 'Pause freezes local actor playback and position');
        await click(38); await wait('s.screen===2');
        await page.waitForTimeout(120);
        check(JSON.stringify((await state()).actors) === frozen, 'Settings does not advance hidden actor playback');
        await click(41); await wait('s.screen===1&&s.paused');
        await page.keyboard.press('Space'); await wait('!s.paused');
        checks.push('manual pause and utility screens preserve independent playback clocks');
        await wait('s.actors.some(actor=>actor.state===2)');
        /** @type {Actor} */ const attacking = (await state()).actors.find(actor => actor.state === 2);
        check(attacking.frame >= 45 && attacking.frame < 57, 'Enemy attack selects the authored attack segment');
        await page.keyboard.press('Space'); await wait('s.paused');
        /** @type {string} */ const attackPause = JSON.stringify((await state()).actors);
        await page.waitForTimeout(140);
        check(JSON.stringify((await state()).actors) === attackPause, 'Attack one-shot also freezes under pause');
        await page.keyboard.press('Space'); await wait('!s.paused');
        await wait('s.actors.some(actor=>actor.state===0)');
        checks.push('committed enemy attacks play once and return to blocked idle');
        await wait('s.deathVisuals>0&&s.actors.some(actor=>actor.state===3)');
        await page.keyboard.press('Space'); await wait('s.paused&&s.deathVisuals>0');
        /** @type {State} */ const death = await state();
        /** @type {Actor} */ const corpse = death.actors.find(actor => actor.state === 3);
        check(corpse.frame >= 57 && corpse.frame < 75 && death.defeated > 0,
              'A removed enemy leaves a correctly ranged cosmetic death clip');
        check(death.actors.length === death.enemies + death.deathVisuals,
              'Visual-only deaths remain separate from the living enemy count');
        /** @type {string} */ const deadPause = JSON.stringify(death.actors);
        await page.waitForTimeout(160);
        check(JSON.stringify((await state()).actors) === deadPause, 'Corpse tails freeze rather than expiring under pause');
        await page.screenshot({path: '.playwright-mcp/thorium-animation-death.png', scale: 'css'});
        await page.keyboard.press('Space'); await wait('!s.paused');
        await wait(`!s.actors.some(actor=>actor.id===${corpse.id})`);
        checks.push('death tails outlive game entities, pause safely and retire after playback');
        await page.keyboard.press('Space'); await wait('s.paused');
        await click(42); await wait('s.screen===0');
        await click(36); await click(30); await wait('s.screen===1&&s.phase===0');
        check((await state()).actors.length === 0 && (await state()).deathVisuals === 0,
              'New run discards old playback identities and cosmetic corpses');
        await click(10); await wait('s.actors.some(actor=>actor.state===1)');
        checks.push('new-run reset clears actor clocks while retaining usable source bindings');
        check(errors.length === 0, errors.join('; '));
        return {checks, errors};
    } finally {
        page.off('pageerror', onError);
    }
}
