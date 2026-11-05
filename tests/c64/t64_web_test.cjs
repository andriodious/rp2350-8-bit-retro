const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.CHROME_PATH,headless:true});
 const page=await browser.newPage();const errors=[],loads=[],uploads=[],ejects=[];
 page.on('pageerror',e=>errors.push(e.message));
 const html=fs.readFileSync(path.join(__dirname,'../../src/common/api/web/index.html'),'utf8');
 let state={machine:'c64',board:'presto',core0:50,core1:20,mhz:250,temp:35,disc:'test.t64',tape:{generation:7,count:2,selected:0,state:'ready'}};
 await page.route('**/*',route=>{
  const u=new URL(route.request().url());let data={};
  if(u.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
  if(u.pathname==='/api/status')data=state;
  if(u.pathname==='/api/discs')data={card:true,discs:[{name:'test.t64',size:256}]};
  if(u.pathname==='/api/tape')data={generation:7,entries:[{index:0,name:'<BASIC> "test"',start:2049,size:18,run:true},{index:1,name:'Machine code',start:49152,size:12,run:false}]};
  if(u.pathname==='/api/cartridge/eject')ejects.push(true);
  if(u.pathname==='/api/tape/load')loads.push(route.request().postData());
  if(u.pathname==='/api/disc')uploads.push(u.searchParams.get('name'));
  return route.fulfill({contentType:'application/json',body:JSON.stringify(data)});
 });
 await page.goto('http://presto.test/');
 await page.waitForFunction(()=>document.querySelector('#tapeProgram').options.length===2);
 assert(await page.locator('#tapePanel').isVisible());
 assert.strictEqual(await page.locator('#tapeProgram option').first().textContent(),'<BASIC> "test"');
 assert(await page.locator('#tapeRun').isEnabled());
 await page.locator('#tapeRun').click();assert.deepStrictEqual(loads,['7 0 1']);
 await page.locator('#tapeProgram').selectOption('1');
 assert(await page.locator('#tapeRun').isDisabled());assert(await page.locator('#tapeLoad').isEnabled());
 await page.locator('#tapeLoad').click();assert.deepStrictEqual(loads,['7 0 1','7 1 0']);
 await page.locator('#file').setInputFiles({name:'demo.T64',mimeType:'application/octet-stream',buffer:Buffer.alloc(128)});
 await page.waitForFunction(()=>document.querySelector('#prog').hidden);
 await page.waitForTimeout(100);assert.deepStrictEqual(uploads,['demo.T64']);
 await page.evaluate(()=>show({...status,disc:'disk.d64',tape:{generation:8,count:0}}));
 assert(await page.locator('#tapePanel').isHidden());
 for(const name of ['COMMANDO.P00','Monkey_Island.crt']){
  await page.locator('#file').setInputFiles({name,mimeType:'application/octet-stream',buffer:Buffer.alloc(128)});
  await page.waitForFunction(()=>document.querySelector('#prog').hidden);await page.waitForTimeout(100);
 }
 assert.deepStrictEqual(uploads,['demo.T64','COMMANDO.P00','Monkey_Island.crt']);
 await page.evaluate(()=>show({...status,cartridge:{active:true}}));
 const eject=page.getByRole('button',{name:'Eject cartridge'});assert(await eject.isVisible());
 await eject.click();assert.deepStrictEqual(ejects,[true]);
 await page.evaluate(()=>show({...status,cartridge:{active:false}}));assert(await eject.isHidden());
 await page.evaluate(()=>show({...status,machine:'master'}));
 assert.strictEqual(await page.locator('#file').getAttribute('accept'),'.ssd,.dsd,.zip');
 assert(await page.locator('#tapePanel').isHidden());assert.deepStrictEqual(errors,[]);
 console.log('T64 browser: selection, load/run, machine-code restriction, upload, safe names and machine switching passed');
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
