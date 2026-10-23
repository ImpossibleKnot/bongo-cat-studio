// SPDX-License-Identifier: GPL-3.0-only
await import('./bongo_browser_editor_check.mjs');
await import('./bongo_browser_check.mjs');
const {chromium}=await import(process.env.PLAYWRIGHT_MODULE||'playwright');
const browser=await chromium.launch({channel:'chrome',headless:true,args:['--autoplay-policy=no-user-gesture-required']});
const page=await browser.newPage({viewport:{width:1120,height:1450}});
const errors=[];page.on('pageerror',e=>errors.push(e.message));
try {
 await page.goto('http://127.0.0.1:8766/?v=0.4.0-rc1');
 await page.getByRole('button',{name:'Switch on',exact:true}).click();
 await page.waitForFunction(()=>document.querySelector('#start').hidden);
 await page.getByRole('button',{name:'Live',exact:true}).click();
 await page.waitForFunction(()=>document.querySelector('[data-page="11"]').getAttribute('aria-pressed')==='true');
 if(!await page.locator('#pagehint').textContent().then(s=>s.includes('ENV taps tempo')))throw Error('Live instructions missing');
 await page.getByRole('slider',{name:'KNOB 3',exact:true}).focus();await page.keyboard.press('ArrowUp');await page.keyboard.press('ArrowUp');
 await page.waitForTimeout(300);await page.screenshot({path:new URL('../build/bongo-browser-live-0.4.0-rc1.png',import.meta.url).pathname,fullPage:true});
 await page.getByRole('button',{name:/^LFO \/ /}).click();
 await page.getByRole('button',{name:/^ENV \/ /}).click();await page.waitForTimeout(450);await page.getByRole('button',{name:/^ENV \/ /}).click();
 await page.waitForFunction(()=>document.querySelector('[data-part="2"]').dataset.solo==='false');
 // Enter the editor from Live, so the new page cannot get mistaken for a synth page.
 await page.getByRole('button',{name:'Edit Bass cat',exact:true}).click();
 await page.waitForFunction(()=>document.querySelector('[data-page="6"]').getAttribute('aria-pressed')==='true');
 await page.getByRole('button',{name:'Envelope',exact:true}).click();
 await page.getByRole('slider',{name:'KNOB 1',exact:true}).focus();await page.keyboard.press('ArrowUp');
 await page.getByRole('button',{name:/^LFO \/ /}).click();
 await page.getByRole('button',{name:'Studio',exact:true}).click();
 await page.getByRole('button',{name:/^REC \/ /}).click();await page.waitForTimeout(150);
 await page.getByRole('button',{name:'Play a demo jam ↗',exact:true}).click();
 if(!(await page.locator('#status').textContent()).includes('Finish your recording'))throw Error('Demo must refuse an armed take');
 await page.getByRole('button',{name:/^REC \/ /}).click();await page.getByRole('button',{name:/^PLAY \/ /}).click();
 await page.getByRole('button',{name:'Live',exact:true}).click();
 await page.setViewportSize({width:390,height:1250});await page.waitForTimeout(500);
 await page.getByRole('button',{name:'Live',exact:true}).click();
 await page.screenshot({path:new URL('../build/bongo-browser-mobile-0.4.0-rc1.png',import.meta.url).pathname,fullPage:true});
 if(await page.locator('[data-knob-caption="0"]').textContent()!=="LEAD")throw Error("Page labels lost on resize");
 if(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+2))throw Error('Mobile viewport overflows horizontally');
 if(errors.length)throw Error(errors.join('\n'));
 console.log('PASS: Live controls, tap tempo, editor from Live, sound A/B, recording-safe demo and phone layout. No browser errors.');
} finally {await browser.close();}
