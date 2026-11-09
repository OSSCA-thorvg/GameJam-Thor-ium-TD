/** Run the same real-input scenario locally against an already served dist/. */
import { chromium } from 'playwright';
import { readFile } from 'node:fs/promises';

/** @type {string[]} */
const scenarios = ['tutorial.js', 'playable.js', 'placement.js', 'card-info.js', 'card-charge.js', 'results.js', 'withdrawal.js', 'feedback.js', 'animation.js', 'shared-frames.js', 'audio.js', 'frontend.js', 'profiling.js'];
/** @type {string[]} */
const requested = process.argv.slice(2);
for (const file of requested) {
    if (!scenarios.includes(file)) throw new Error(`Unknown browser scenario: ${file}`);
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
            process.stdout.write(JSON.stringify({file, result: await scenario(page)}, null, 2) + '\n');
        } finally {
            await page.close();
        }
    }
} finally {
    await browser.close();
}
