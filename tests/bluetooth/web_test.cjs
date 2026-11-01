const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.CHROME_PATH,headless:true});
 try {
  const page=await browser.newPage();let errors=[],commands=[];
  page.on('pageerror',e=>errors.push(e.message));
  const html=fs.readFileSync(path.join(__dirname,'../../src/common/api/web/index.html'),'utf8');
  let sample={machine:'master',bluetooth:{state:'idle',devices:['01:02:03:04:05:06 Nimbus'],axes:[0,0,0,0],buttons:[0,0,0,0,0,0,0,0]}};
  await page.route('**/*',route=>{
   const u=new URL(route.request().url());
   if(u.pathname==='/')return route.fulfill({contentType:'text/html',body:html});
   if(u.pathname==='/api/bluetooth')commands.push(route.request().postData());
   return route.fulfill({contentType:'application/json',body:JSON.stringify(u.pathname==='/api/status'?sample:{})});
  });
  await page.goto('http://presto.test/');
  await page.locator('#bluetoothPanel').waitFor({state:'visible'});
  await page.getByRole('button',{name:'Scan for controllers',exact:true}).click();
  await page.getByRole('button',{name:'Connect',exact:true}).click();
  await page.getByRole('button',{name:'Disconnect',exact:true}).click();
  assert.deepStrictEqual(commands,['scan','connect 0','disconnect']);
  const pending=page.waitForEvent('download');await page.getByRole('button',{name:'Download controller status'}).click();
  const file=await (await pending).path();assert.equal(JSON.parse(fs.readFileSync(file)).bluetooth.state,'idle');
  sample={machine:'c64'};await page.evaluate(()=>show({machine:'c64'}));assert(await page.locator('#bluetoothPanel').isHidden());
  assert.deepStrictEqual(errors,[]);console.log('Bluetooth controls, status download and disabled-build visibility passed');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
