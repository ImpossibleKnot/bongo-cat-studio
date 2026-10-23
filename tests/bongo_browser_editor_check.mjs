// SPDX-License-Identifier: GPL-3.0-only
const {chromium}=await import(process.env.PLAYWRIGHT_MODULE||'playwright');
const browser=await chromium.launch({channel:'chrome',headless:true,args:['--autoplay-policy=no-user-gesture-required']});
const page=await browser.newPage({viewport:{width:1120,height:1450}});
const errors=[];page.on('pageerror',e=>errors.push(e.message));
await page.goto('http://127.0.0.1:8766/?v=0.4.0-rc1');
await page.getByRole('button',{name:'Switch on',exact:true}).click();await page.waitForFunction(()=>document.querySelector('#start').hidden);
for(const [i,name]of ['Your','Keys','Bass','Drum'].entries()){
 await page.getByRole('button',{name:`Edit ${name} cat`,exact:true}).click();
 await page.waitForFunction(p=>document.querySelector(`[data-part="${p}"]`).getAttribute('aria-pressed')==='true',i);
 await page.getByRole('button',{name:'Sounds',exact:true}).click();
 await page.waitForFunction(()=>document.querySelector('[data-knob-caption="0"]').textContent==='PART');
 await page.getByRole('slider',{name:'KNOB 2',exact:true}).focus();await page.keyboard.press('ArrowUp');
 await page.getByRole('slider',{name:'PRESETS',exact:true}).focus();await page.keyboard.press('ArrowUp');
 for(const [pg,label]of [[7,'Tone'],[8,'Colour'],[9,'Envelope'],[10,'Effects']]){
  await page.getByRole('button',{name:label,exact:true}).click();
  await page.waitForFunction(n=>document.querySelector(`[data-page="${n}"]`).getAttribute('aria-pressed')==='true',pg);
  await page.waitForFunction(()=>!['1','5','PART'].includes(document.querySelector('[data-knob-caption="0"]').textContent));
  await page.getByRole('slider',{name:'KNOB 1',exact:true}).focus();await page.keyboard.press('ArrowUp');
 }
}
await page.getByRole('button',{name:'Edit Bass cat',exact:true}).click();
await page.getByRole('button',{name:'Envelope',exact:true}).click();
await page.waitForFunction(()=>document.querySelector('[data-knob-caption="0"]').textContent==='ATK');
await page.screenshot({path:new URL('../build/bongo-browser-editor-0.4.0-rc1.png',import.meta.url).pathname,fullPage:true});
await page.getByRole('button',{name:'SAVE / SAVE',exact:true}).click();await page.waitForTimeout(1000);
await page.reload();await page.getByRole('button',{name:'Switch on',exact:true}).click();await page.waitForFunction(()=>document.querySelector('#start').hidden);
await page.getByRole('button',{name:'Edit Bass cat',exact:true}).click();
await page.waitForFunction(()=>document.querySelector('[data-page="6"]').getAttribute('aria-pressed')==='true');
if(errors.length)throw new Error(errors.join('\n'));
console.log('PASS: all four cat selectors, engine/preset knobs, five editor pages, live engine labels, save and reload without browser errors.');
await browser.close();
