const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{
const browser=await chromium.launch({executablePath:process.env.CHROME_PATH,headless:true});
try {
 const page=await browser.newPage();let errors=[];page.on('pageerror',e=>errors.push(e.message));
 const html=fs.readFileSync(path.join(__dirname,'../../src/common/api/web/index.html'),'utf8').replace('const WEBDIAG = true;','const WEBDIAG = false;');
 await page.route('**/*',r=>r.fulfill({contentType:r.request().url().endsWith('/')?'text/html':'application/json',body:r.request().url().endsWith('/')?html:JSON.stringify({machine:'master'})}));
 await page.goto('http://presto.test/');
 await page.evaluate(()=>{window.profileSample={machine:'c64',c64:{uptime_ms:10000},c64_profile:{sequence:1,frames:250,window_us:5000000,rendered:250,turbo:0,cpu_us:100,vic_us:200,sid_us:300,cia_us:400,emulation_us:1000,conversion_us:20,wait_us:30,frontend_us:40,pacing_us:50}};showProfile(window.profileSample);showProfile(window.profileSample);});
 assert(await page.locator('#profilepanel').isVisible());
 assert((await page.locator('#profileout').textContent()).includes('1 samples collected'));
 const pending=page.waitForEvent('download');await page.click('#profilesave');const d=await pending;const file=await d.path();assert(fs.readFileSync(file,'utf8').includes('C64PROF window_us=5000000'));
 await page.click('#profileclear');
 await page.evaluate(()=>{showProfile(window.profileSample);window.profileSample.c64_profile.sequence=2;showProfile(window.profileSample);});
 assert((await page.locator('#profileout').textContent()).includes('1 samples collected'));
 await page.evaluate(()=>showProfile({machine:'master'}));assert(await page.locator('#profilepanel').isHidden());
 assert.deepStrictEqual(errors,[]);console.log('Web profile visibility, deduplication, clear and download passed');
} finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
