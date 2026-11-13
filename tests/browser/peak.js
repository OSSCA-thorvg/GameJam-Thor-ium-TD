/**
 * Open the measurement-only stage 5 bench battle, check that it reaches a late-wave crowd with real frame
 * reports, and that it leaves no saved settings behind. Thresholds are generous on purpose: headless timing
 * varies with the host, so this checks the measurement path rather than a frame budget.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[],peak:{enemies:number,frames:number,p50:number,p95:number}}>}
 */
async (page) => {
    /** @typedef {{level:number}} Building */
    /** @typedef {{phase:number,wave:number,enemies:number,towers:number,units:number,paused:boolean,screen:number,currentStage:number,benchmark:{stage:number,wave:number,crowd:number}|null,buildings:Building[],rendering:{error:string,cached:boolean,cachedDraws:number,layers:number,layerReuses:number}}} State */
    /** @typedef {{enemies:number,total:number,stages:number[],steps:number}} Frame */
    /** @type {string[]} */ const checks = [];
    /** @type {string[]} */ const errors = [];
    /** Collect runtime failures. @param {Error} error @returns {void} */
    const onError = error => errors.push(error.message);
    /** Require an externally observable contract. @param {unknown} value @param {string} message @returns {void} */
    function check(value, message) { if (!value) throw new Error(message); }
    /** Read diagnostics without writing game state. @returns {Promise<State>} */
    async function state() { return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()))); }
    /** Wait for initialization and a diagnostic condition. @param {string} condition @param {number} [timeout] @returns {Promise<void>} */
    async function wait(condition, timeout = 30000) {
        try {
            await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`,
                                       null, {timeout});
        } catch (error) {
            throw new Error(`Peak condition failed: ${condition}; state=${JSON.stringify(await state())}`, {cause: error});
        }
    }
    /** Drain bench frames recorded since the previous read. @returns {Promise<Frame[]>} */
    async function frames() {
        /** @type {string} */ const text = await page.evaluate(() => Module.UTF8ToString(Module._thorium_bench_frames()));
        return text.split('\n').filter(line => line).map(line => {
            /** @type {number[]} */ const values = line.split(',').map(Number);
            return {enemies:values[0], total:values[1], stages:values.slice(2, 8), steps:values[8]};
        });
    }
    /** Read one nearest-rank percentile. @param {number[]} values @param {number} fraction @returns {number} */
    function percentile(values, fraction) {
        /** @type {number[]} */ const sorted = [...values].sort((a, b) => a - b);
        return sorted[Math.min(sorted.length - 1, Math.ceil(sorted.length * fraction) - 1)];
    }
    page.on('pageerror', onError);
    try {
        await page.setViewportSize({width: 1280, height: 800});
        await page.goto('http://127.0.0.1:8317/?bench=s5w16&anim=cache');
        await wait('s.benchmark&&s.screen===1&&s.phase===1&&s.wave===16&&s.currentStage===5');
        /** @type {State} */ const opened = await state();
        check(opened.towers >= 30 && opened.buildings.some(building => building.level > 3) && opened.units > 0,
              `Autoplayed defence fills the pads with awakened buildings and units: ${JSON.stringify(opened.buildings)}`);
        checks.push('bench query opens stage 5 wave 16 with an autoplayed, awakened defence');
        // A focus loss suspends the battle and would normally save settings; the bench session must not.
        await page.evaluate(() => window.dispatchEvent(new Event('blur')));
        for (let attempt = 0; attempt < 10 && (await state()).paused; ++attempt) {
            await page.keyboard.press('Space');
            await page.waitForTimeout(150);
        }
        await wait('!s.paused');
        check(await page.evaluate(() => localStorage.getItem('thorium_td_save')) === null,
              'A bench session never writes the save, even after a suspend');
        await frames();
        await wait('s.enemies>=150', 90000);
        await page.waitForTimeout(2500);
        /** @type {Frame[]} */ const recorded = await frames();
        check(recorded.length >= 60, `Bench frames are recorded during the battle: ${recorded.length}`);
        check(recorded.every(frame => Number.isFinite(frame.total) && frame.total >= 0 &&
                                      frame.stages.every(stage => Number.isFinite(stage) && stage >= 0) &&
                                      frame.stages.reduce((sum, stage) => sum + stage, 0) <= frame.total + .05),
              'Every bench frame reports finite, exclusive stages bounded by its total');
        /** @type {number} */ const peak = Math.max(...recorded.map(frame => frame.enemies));
        /** @type {number[]} */ const totals = recorded.map(frame => frame.total);
        check(peak >= 150, `Late wave reaches a large live crowd: ${peak}`);
        check(percentile(totals, .95) < 250, `Peak frames stay far from a stall: p95 ${percentile(totals, .95)} ms`);
        /** @type {State} */ const running = await state();
        check(running.rendering.error === '' && running.rendering.cached && running.rendering.cachedDraws > 0,
              'Shared-frame backend chosen by the query draws the crowd without renderer errors');
        check(running.rendering.layerReuses > 10 * running.rendering.layers,
              `Static backdrop and ground are drawn from retained pixels: ${JSON.stringify(running.rendering)}`);
        await page.screenshot({path: '.playwright-mcp/thorium-peak.png', scale: 'css'});
        checks.push(`late-wave crowd of ${peak} with ${recorded.length} reported frames`);
        await page.goto('http://127.0.0.1:8317/');
        await wait('s.screen===0&&s.rendering');
        /** @type {State} */ const normal = await state();
        check(normal.benchmark === null && !normal.rendering.cached,
              'A normal load ignores the bench session and its animation backend');
        checks.push('normal load afterwards keeps default settings and no bench state');
        check(errors.length === 0, errors.join('; '));
        return {checks, errors, peak:{enemies:peak, frames:recorded.length, p50:percentile(totals, .5), p95:percentile(totals, .95)}};
    } finally { page.off('pageerror', onError); }
}
