const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(require('path').join(__dirname,'../../src/common/api/web/index.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script);
const show=script.slice(script.indexOf('function show(s)'),script.indexOf('function esc(s)'));
const discs=script.slice(script.indexOf('let discRequest=0;'),script.indexOf("$('sdsel').onchange"));
module.exports=(async()=>{
 const queue=[],select={options:[],hidden:false,disabled:false,add(o){this.options.push(o)},set innerHTML(v){this.options=[]}};
 const c=vm.createContext({assert,Option:function(text,value){this.text=text;this.value=value},
 api:()=>new Promise((resolve,reject)=>queue.push({resolve,reject})),
 $:()=>select});
 vm.runInContext("let machineEpoch=1, switchingTo='spectrum',status={machine:'master'}; const responseEpoch=new WeakMap();",c);
 vm.runInContext(show,c);
 // Rejected old-machine samples must touch no diagnostics or panel.
 vm.runInContext("show({machine:'master'}); let stale={machine:'spectrum'};responseEpoch.set(stale,0);show(stale);",c);
 vm.runInContext(discs,c);
 const first=vm.runInContext('listDiscs()',c);
 vm.runInContext("++machineEpoch;status={machine:'spectrum',disc:'new.tap'}",c);
 const second=vm.runInContext('listDiscs()',c);
 assert.equal(select.options[0].text,'Reading SD card…');
 queue[1].resolve({card:true,discs:[{name:'new.tap',size:819200}]});await second;
 queue[0].resolve({card:true,discs:[{name:'old.ssd',size:200000}]});await first;
 assert.equal(select.options[1].value,'new.tap');assert.equal(select.disabled,false);
 const third=vm.runInContext('listDiscs()',c),fourth=vm.runInContext('listDiscs()',c);
 queue[3].resolve({card:true,discs:[{name:'latest.tap',size:1}]});await fourth;
 queue[2].reject(Error('late failure'));await third;
 assert.equal(select.options[1].value,'latest.tap');
 const handler=script.slice(script.indexOf("for (const b of $('msel').querySelectorAll('button')) b.onclick"),script.indexOf("$('turbo').onclick"));
 assert(!handler.includes('buildKeyboard('),'switch must wait for accepted status before drawing');
 assert(handler.indexOf('switchingTo = m')<handler.indexOf("await api('/api/machine'"));

 // A pending/lost switch acknowledgement must not delay feedback or change
 // keyboard/media. Execute the real handler with a deliberately deferred API.
 let finish,soon=0;
 const buttons=[{dataset:{m:'spectrum'}}],nodes={};
 const node=id=>nodes[id]||(nodes[id]={classList:{remove(){}},querySelectorAll:()=>buttons});
 buttons[0].parentElement=node('msel');
 const switching=vm.createContext({$:node,confirm:()=>true,trace:()=>{},
   performance:{now:()=>100},clearTimeout:()=>{},setTimeout:()=>1,
   api:()=>new Promise((resolve,reject)=>{finish={resolve,reject}}),
   pollSoon:()=>soon++,msg:()=>{}});
 vm.runInContext("const MACHINES={spectrum:'ZX Spectrum 48K'};let status={machine:'master'},switchingTo,switchStarted,machineEpoch=0,pending=[],held=new Set(),capturing=true,volTimer,brightTimer,switchTimer;",switching);
 vm.runInContext(handler,switching);
 const switchingRequest=buttons[0].onclick();
 assert(node('stats').innerHTML.includes('Starting ZX Spectrum 48K'));
 assert.equal(buttons[0].disabled,true);
 assert.equal(node('machineView').hidden,true);
 assert.equal(node('startingScreen').hidden,false);
 assert.equal(node('startingMessage').textContent,'Starting ZX Spectrum 48K…');
 assert.equal(vm.runInContext('capturing',switching),false);
 finish.reject(Error('connection closed during reboot'));await switchingRequest;
 assert.equal(vm.runInContext('switchingTo',switching),'spectrum');
 assert.equal(soon,1);
 assert.equal(node('machineView').hidden,true,'lost acknowledgement must keep controls hidden');
 assert(show.indexOf("$('machineView').hidden=false")>show.indexOf('showApp('),'reveal only after panels are updated');
 assert(show.indexOf("$('machineView').hidden=false")>show.indexOf('showTape('));
 console.log('Machine-switch stale status, disc response/error races, and deferred keyboard: passed');
})();
