// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
let Processor;
globalThis.AudioWorkletProcessor=class {constructor(){this.port={postMessage(){}}}};
globalThis.registerProcessor=(_,p)=>Processor=p;
await import('../web/emu/worklet.js');
const w=new Processor(),log=[];let now=0,page=11,ready=false;
w.ex={web_ready:()=>ready,web_now_ms:()=>now,web_buttons:mask=>log.push(['buttons',mask,page]),web_set_page:n=>{page=n;log.push(['page',n]);},web_enc:(role,n)=>log.push(['knob',role,n,page])};
const send=m=>w.port.onmessage({data:m});
send({type:'buttons',mask:8});send({type:'buttons',mask:0});send({type:'page',value:7});
send({type:'enc',role:3,n:1});send({type:'enc',role:3,n:2});send({type:'page',value:9});
w.input();assert.deepEqual(log,[]);ready=true;
while(w.queue.length){w.input();now+=25;}
assert.deepEqual(log,[['buttons',8,11],['buttons',0,11],['page',7],['knob',3,3,7],['page',9]]);
console.log('PASS: fast page navigation cannot reinterpret an earlier button or knob gesture; adjacent turns coalesce in order.');
