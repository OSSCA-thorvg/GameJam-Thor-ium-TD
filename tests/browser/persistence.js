/**
 * Verify fresh-session defaults and actual persisted-setting restoration before any gameplay starts.
 * Fixtures provide external localStorage input; they never mutate live WASM state.
 * @param {import('playwright').Page} page
 * @returns {Promise<{checks:string[],errors:string[]}>}
 */
async (page) => {
    /** @typedef {{screen:number,reducedMotion:boolean,damageNumbers:boolean,tutorial:{completed:boolean},audio:{volume:number,muted:boolean}}} Snapshot */
    /** @typedef {{label:string,stored:string|null,blocked?:boolean,volume:number,muted:boolean,reducedMotion?:boolean,damageNumbers?:boolean,completed?:boolean}} Fixture */
    /** @type {string[]} */ const errors = [];
    /** Collect unexpected runtime errors from each isolated browser context. @param {Error} error @returns {void} */
    const onError = error => errors.push(error.message);
    /** Read the public initialized app snapshot without exposing persistence write commands. @param {import('playwright').Page} target @returns {Promise<Snapshot>} */
    async function snapshot(target) {
        await target.waitForFunction(() => typeof Module !== 'undefined' && Module.calledRun &&
            typeof Module._thorium_debug_state === 'function');
        return target.evaluate(() => JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())));
    }
    /** Require a startup contract and include the actual state when it fails. @param {boolean} condition @param {string} message @param {Snapshot} current @returns {void} */
    function check(condition, message, current) {
        if (!condition) throw new Error(`${message}; state=${JSON.stringify(current)}`);
    }
    page.on('pageerror', onError);
    try {
        await page.goto('http://127.0.0.1:8317/');
        /** @type {Snapshot} */ const fresh = await snapshot(page);
        check(fresh.audio.volume === .45 && !fresh.audio.muted,
              'A missing save uses the same 45% audio defaults as Settings Defaults', fresh);
        /** @type {Fixture[]} */ const fixtures = [
            {label:'empty save', stored:'', volume:.45, muted:false},
            {label:'empty object', stored:'{}', volume:.45, muted:false},
            {label:'malformed save', stored:'not-json', volume:.45, muted:false},
            {label:'missing volume', stored:'{"muted":true}', volume:.45, muted:true},
            {label:'saved custom settings', stored:'{"volume":0.7,"muted":true,"reducedMotion":true,"damageNumbers":false,"tutorialCompleted":true}',
             volume:.7, muted:true, reducedMotion:true, damageNumbers:false, completed:true},
            {label:'saved full volume', stored:'{"volume":1,"muted":false}', volume:1, muted:false},
            {label:'unavailable storage', stored:null, blocked:true, volume:.45, muted:false},
        ];
        /** @type {import('playwright').Browser|null} */ const browser = page.context().browser();
        if (!browser) throw new Error('Persistence checks require an isolated browser context.');
        for (const fixture of fixtures) {
            /** @type {import('playwright').BrowserContext} */ const context = await browser.newContext();
            try {
                await context.addInitScript(({stored, blocked}) => {
                    if (location.origin !== 'http://127.0.0.1:8317') return;
                    if (blocked) {
                        Object.defineProperty(Storage.prototype, 'getItem', {
                            value() { throw new DOMException('Storage unavailable', 'SecurityError'); },
                        });
                    } else if (stored !== null) {
                        localStorage.setItem('thorium_td_save', stored);
                    }
                }, {stored:fixture.stored, blocked:fixture.blocked ?? false});
                /** @type {import('playwright').Page} */ const target = await context.newPage();
                target.on('pageerror', onError);
                await target.goto('http://127.0.0.1:8317/');
                /** @type {Snapshot} */ const current = await snapshot(target);
                check(current.audio.volume === fixture.volume && current.audio.muted === fixture.muted,
                      `${fixture.label}: restore explicit audio values and default absent ones`, current);
                check(current.reducedMotion === (fixture.reducedMotion ?? false) &&
                      current.damageNumbers === (fixture.damageNumbers ?? true) &&
                      current.tutorial.completed === (fixture.completed ?? false),
                      `${fixture.label}: restore presentation and completion settings independently`, current);
            } finally { await context.close(); }
        }
        if (errors.length) throw new Error(errors.join('; '));
        return {checks:[
            'missing, empty, malformed and partial saves share the normal 45% audio defaults',
            'explicit saved audio, presentation and completion settings are restored, including full volume',
            'unavailable browser storage falls back to the same defaults without crashing',
        ], errors};
    } finally { page.off('pageerror', onError); }
}
