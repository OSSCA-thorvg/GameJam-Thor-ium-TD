/** Run the same real-input scenario locally against an already served dist/. */
import { chromium } from 'playwright';
import { mkdir, readFile, writeFile } from 'node:fs/promises';

/** @type {string[]} */
const scenarios = ['campaign.js', 'persistence.js', 'tutorial.js', 'playable.js', 'placement.js', 'card-info.js', 'card-charge.js', 'results.js', 'withdrawal.js', 'feedback.js', 'animation.js', 'shared-frames.js', 'audio.js', 'frontend.js', 'profiling.js', 'peak.js', 'excavation.js', 'machines.js'];
/** @type {string[]} */
const requested = process.argv.slice(2);
for (const file of requested) {
    if (!scenarios.includes(file)) throw new Error(`Unknown browser scenario: ${file}`);
}
/** Save read-only game evidence before teardown, retaining the original scenario failure.
 * @param {import('playwright').Page} page
 * @param {string} file
 * @param {unknown} error
 * @returns {Promise<void>}
 */
async function captureFailure(page, file, error) {
    /** @type {string} */ const message = error instanceof Error ? error.stack ?? error.message : String(error);
    process.stderr.write(`Browser scenario failed: ${file}\n${message}\n`);
    try {
        /** @type {{state:unknown,ui:unknown,readError?:string}} */ const snapshot = await page.evaluate(() => {
            /** @type {boolean} */ const ready = Boolean(typeof Module !== 'undefined' && Module.calledRun);
            return {
                state:ready && typeof Module._thorium_debug_state === 'function'
                    ? JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())) : null,
                ui:ready && typeof Module._thorium_debug_ui === 'function'
                    ? JSON.parse(Module.UTF8ToString(Module._thorium_debug_ui())) : null,
            };
        }).catch(readError => ({state:null, ui:null, readError:String(readError)}));
        /** @type {string} */ const diagnostic = JSON.stringify({file, error:message, ...snapshot}, null, 2);
        process.stderr.write(diagnostic + '\n');
        await mkdir('.playwright-mcp', {recursive:true});
        /** @type {string} */ const stem = `.playwright-mcp/failure-${file.replace(/\.js$/, '')}`;
        await writeFile(`${stem}.json`, diagnostic + '\n', 'utf8');
        await page.screenshot({path:`${stem}.png`, scale:'css', timeout:5000});
    } catch (captureError) {
        process.stderr.write(`Could not finish diagnostics for ${file}: ${String(captureError)}\n`);
    }
}
/** @type {import('playwright').Browser} */
const browser = await chromium.launch({ headless: true });
try {
    for (const file of requested.length ? requested : scenarios) {
        /** @type {import('playwright').Page} */
        const page = await browser.newPage();
        try {
            /** @type {string} */
            const source = await readFile(new URL(`./${file}`, import.meta.url), 'utf8');
            /** @type {(page: import('playwright').Page) => Promise<unknown>} */
            const scenario = new Function(`return (${source});`)();
            /** @type {unknown} */ const result = await scenario(page);
            /** @type {string[]} */ const missing = await page.evaluate(() =>
                typeof Module !== 'undefined' && Module.calledRun && typeof Module._thorium_debug_state === 'function'
                    ? JSON.parse(Module.UTF8ToString(Module._thorium_debug_state())).missingText ?? [] : []).catch(() => []);
            if (missing.length) throw new Error(`Text keys missing from the string tables: ${missing.join(', ')}`);
            process.stdout.write(JSON.stringify({file, result}, null, 2) + '\n');
        } catch (error) {
            await captureFailure(page, file, error);
            throw new Error(`Browser scenario failed: ${file}`, {cause:error});
        } finally {
            await page.close();
        }
    }
} finally {
    await browser.close();
}
