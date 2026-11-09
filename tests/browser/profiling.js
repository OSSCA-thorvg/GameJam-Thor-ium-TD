/**
 * Validate completed native/WASM CPU stages through real gameplay input and pause.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @type {string[]} */ const errors = [];
    /** @type {string[]} */ const checks = [];
    /** Collect browser runtime failures. @param {Error} error @returns {void} */
    function onError(error) { errors.push(error.message); }
    /** Fail one concrete integration contract. @param {boolean} value @param {string} message @returns {void} */
    function check(value, message) { if (!value) throw new Error(message); }
    /** @typedef {{total:number,interval:number,steps:number,simulation:number,presentation:number,cachePreparation:number,scene:number,raster:number,audio:number,upload:number,present:number}} Timing */
    /** @typedef {{time:number,paused:boolean,performance:{frames:number,samples:number,latest:Timing,p50:Timing,p95:Timing}}} Snapshot */
    /** Read the completed platform iteration. @returns {Promise<Snapshot>} */
    async function state() { return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()))); }
    /** Wait for an observed condition without writing state. @param {string} condition @returns {Promise<void>} */
    async function wait(condition) {
        await page.waitForFunction(`typeof Module!=='undefined'&&Module.calledRun&&(()=>{const s=JSON.parse(Module.UTF8ToString(Module._thorium_debug_state()));return ${condition};})()`);
    }
    /** Click a current real widget. @param {number} id @returns {Promise<void>} */
    async function click(id) {
        await page.waitForFunction(id => JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).some(widget => widget.id === id && widget.enabled), id);
        /** @type {{x:number,y:number,w:number,h:number}} */
        const widget = await page.evaluate(id => JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())).find(widget => widget.id === id && widget.enabled), id);
        await page.mouse.click(widget.x + widget.w / 2, widget.y + widget.h / 2);
    }
    page.on('pageerror', onError);
    try {
        await page.setViewportSize({width: 1200, height: 900});
        await page.goto('http://127.0.0.1:8317/');
        await wait('s.performance&&s.performance.samples>=3');
        check((await state()).performance.latest.steps === 0, 'Title has no simulated fixed steps');
        await click(36); await click(10);
        await wait('s.time>.1&&s.performance.latest.steps>0');
        /** @type {Snapshot} */ const running = await state();
        /** @type {Timing} */ const sample = running.performance.latest;
        /** @type {number[]} */ const stages = [sample.simulation, sample.presentation, sample.cachePreparation,
            sample.scene, sample.raster, sample.audio, sample.upload, sample.present];
        check(stages.every(value => Number.isFinite(value) && value >= 0) &&
              stages.reduce((sum, value) => sum + value, 0) <= sample.total + .01,
              'Complete CPU timings are finite, exclusive and bounded by actual iteration work');
        // Browser timer quantization can legitimately report zero for a short output call.
        check(sample.scene > 0 && sample.raster > 0 &&
              running.performance.p95.upload >= running.performance.p50.upload &&
              running.performance.p95.present >= running.performance.p50.present,
              'Measured scene/raster work and output percentiles remain valid with quantized browser timers');
        await page.keyboard.press('Space');
        await wait('s.paused&&s.performance.latest.steps===0');
        /** @type {Snapshot} */ const paused = await state();
        await wait(`s.performance.frames>${paused.performance.frames + 3}`);
        /** @type {Snapshot} */ const later = await state();
        check(later.time === paused.time && later.performance.latest.simulation === 0 &&
              later.performance.samples <= 120 && later.performance.p95.total >= later.performance.p50.total,
              'Profiler continues through paused rendering without advancing gameplay or unbounded history');
        checks.push('completed full-pipeline CPU stages and bounded p50/p95 diagnostics through real WASM gameplay');
        check(errors.length === 0, errors.join('; '));
        return {checks, errors};
    } finally { page.off('pageerror', onError); }
}
