const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(__dirname+'/../../src/common/api/web/index.html','utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];new vm.Script(script);
function node(){return {children:[],dataset:{},classList:{add(){},remove(){},toggle(){}},events:{},appendChild(n){this.children.push(n)},setAttribute(){},setPointerCapture(){},addEventListener(k,v){this.events[k]=v}};}
const kb=node(),box=node(),events={},sent=[];
const doc={createElement:node,addEventListener(k,v){events[k]=v},hidden:false};
const c=vm.createContext({document:doc,kb,$:()=>box,ledEls:{},latched:new Set(),keys:v=>sent.push(...v),down:c=>'d'+c.toString(16),up:c=>'u'+c.toString(16),tap:c=>sent.push(c==='b'?'b':'tap'),addEventListener:(k,v)=>events[k]=v});
vm.runInContext(script.slice(script.indexOf('const ZX_ROWS='),script.indexOf('function buildKeyboard(machine)')),c);
vm.runInContext('buildSpectrumKeyboard()',c);
const buttons=kb.children.at(-1).children;assert.equal(buttons.length,6);
const press=i=>buttons[i].events.pointerdown({preventDefault(){},pointerId:i});
press(1);press(4);press(5);assert.deepEqual(sent.splice(0),['d81','d84','d85']);
buttons[1].events.pointercancel();assert.deepEqual(sent.splice(0),['u81']);
events.blur();assert.deepEqual(sent.splice(0),['u84','u85']);
press(2);doc.hidden=true;events.visibilitychange();assert.deepEqual(sent.splice(0),['d82','u82']);
press(3);press(0);assert.deepEqual(sent.splice(0),['d83','u83','b']);
assert(html.includes('Joystick mode uses Kempston; select Kempston in the game.'));
console.log('PASS: Spectrum controls, simultaneous holds, cancellation, blur, hidden page and Reset release');
