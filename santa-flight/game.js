(() => {
'use strict';
const canvas=document.getElementById('game'),ctx=canvas.getContext('2d');
const $=id=>document.getElementById(id);
const W=960,H=540;
let state='menu',score=0,gifts=0,lives=3,frame=0,speed=4,spawn=0,objects=[],particles=[],stars=[];
let santa={x:180,y:270,vy:0,shield:0};
let targetY=270,highScore=Number(localStorage.getItem('santaFlightBest')||0);
for(let i=0;i<90;i++)stars.push({x:Math.random()*W,y:Math.random()*330,s:Math.random()*2+1,p:Math.random()*6});
const rnd=(a,b)=>Math.random()*(b-a)+a;
function rect(x,y,w,h,c){ctx.fillStyle=c;ctx.fillRect(Math.round(x),Math.round(y),Math.round(w),Math.round(h))}
function text(t,x,y,size,color='#fff',align='left'){ctx.font=`bold ${size}px monospace`;ctx.textAlign=align;ctx.fillStyle=color;ctx.fillText(t,x,y)}
function background(){
 const g=ctx.createLinearGradient(0,0,0,H);g.addColorStop(0,'#07132f');g.addColorStop(.7,'#263d70');g.addColorStop(1,'#e06b72');ctx.fillStyle=g;ctx.fillRect(0,0,W,H);
 stars.forEach(s=>{let a=.45+Math.sin(frame*.04+s.p)*.35;ctx.globalAlpha=a;rect(s.x,s.y,s.s,s.s,'#dff6ff')});ctx.globalAlpha=1;
 // distant snowy peaks
 ctx.fillStyle='#314d79';ctx.beginPath();ctx.moveTo(0,360);for(let x=0;x<=W;x+=80)ctx.lineTo(x,270+Math.sin(x*.018)*55);ctx.lineTo(W,H);ctx.lineTo(0,H);ctx.fill();
 // city silhouette with lit windows
 let offset=(frame*speed*.35)%180;
 for(let i=-1;i<8;i++){let x=i*180-offset;let bh=90+(i*37%75);rect(x,390-bh,150,bh,'#101d3c');rect(x+15,405-bh,120,8,'#263b60');
 for(let wx=x+18;wx<x+140;wx+=28)for(let wy=410-bh;wy<385;wy+=28)if((Math.floor(wx+wy)%4)!==0)rect(wx,wy,9,12,Math.sin(frame*.05+wx+wy)>0?'#ffd77b':'#4c7398');
 rect(x,390-bh,150,4,'#9fc9e8')}
 rect(0,480,W,60,'#101c38');rect(0,480,W,4,'#d4eaff');
 // aurora ribbons
 ctx.globalAlpha=.16;ctx.strokeStyle='#59ffc6';ctx.lineWidth=14;ctx.beginPath();for(let x=0;x<=W;x+=20){let y=100+Math.sin(x*.012+frame*.012)*35; x===0?ctx.moveTo(x,y):ctx.lineTo(x,y)}ctx.stroke();ctx.globalAlpha=1;
}
function drawReindeer(x,y,k=1){rect(x,y,42*k,20*k,'#8c5738');rect(x+28*k,y-12*k,12*k,15*k,'#a86c45');rect(x+34*k,y-17*k,4*k,10*k,'#d7b17a');rect(x+42*k,y-9*k,5*k,5*k,'#e74c59');for(let dx=7;dx<38;dx+=23)rect(x+dx*k,y+18*k,5*k,14*k,'#d7b17a');}
function drawSanta(){
 ctx.save();ctx.translate(santa.x,santa.y);if(santa.shield>0){ctx.strokeStyle='#7ceaff';ctx.lineWidth=4;ctx.beginPath();ctx.arc(0,0,55+Math.sin(frame*.2)*4,0,Math.PI*2);ctx.stroke()}
 drawReindeer(-76,-15,.8);drawReindeer(-38,-8,.85);
 // reins and sleigh
 rect(-35,12,108,4,'#d5ad62');rect(-20,22,102,5,'#ffd36a');rect(35,7,6,20,'#e9c16f');
 // sack
 rect(15,-28,27,28,'#287b54');rect(22,-34,17,8,'#55b879');
 // Santa body/head/hat
 rect(-4,-18,38,37,'#d83e46');rect(3,-30,29,25,'#f0c6a2');rect(0,-34,32,7,'#fff');rect(4,-47,28,15,'#d83e46');rect(22,-53,12,10,'#fff');rect(-3,-6,39,9,'#fff');rect(22,-22,8,8,'#202d45');
 ctx.restore();
}
function drawPlane(o){let x=o.x,y=o.y;rect(x+12,y+13,70,14,'#b9c9d9');rect(x+32,y-8,28,55,'#e2edf5');rect(x+3,y+8,20,24,'#839bb4');rect(x+70,y+9,22,20,'#829bb5');rect(x+42,y+16,7,6,'#e64b55');}
function drawLine(o){let x=o.x,y=o.y;rect(x,y,8,90,'#78533f');rect(x+115,y,8,90,'#78533f');rect(x,y+10,123,5,'#d9c7a5');let colors=['#ed6b9a','#e9e8ed','#77b9ed','#e8bf69'];for(let i=0;i<4;i++){let xx=x+13+i*27;rect(xx,y+15,18,25,colors[i]);rect(xx+4,y+37,11,12,colors[i]);rect(xx+4,y+13,10,4,'#fff')}}
function drawGift(o){rect(o.x,o.y,25,23,'#e94b54');rect(o.x+9,o.y,7,23,'#ffd45a');rect(o.x,o.y+8,25,5,'#ffd45a');rect(o.x+4,o.y-5,8,7,'#ffd45a');rect(o.x+14,o.y-5,8,7,'#ffd45a')}
function spawnObject(){let r=Math.random();if(r<.44)objects.push({type:'gift',x:W+20,y:rnd(100,420)});else if(r<.72)objects.push({type:'plane',x:W+20,y:rnd(100,400)});else objects.push({type:'line',x:W+20,y:rnd(310,390)});}
function hit(a,b,pad=0){return a.x<b.x+b.w-pad&&a.x+a.w>b.x+pad&&a.y<b.y+b.h-pad&&a.y+a.h>b.y+pad}
function burst(x,y,c){for(let i=0;i<12;i++)particles.push({x,y,vx:rnd(-3,3),vy:rnd(-3,3),life:30,c})}
function update(){
 frame++;speed=Math.min(8,4+score/2500);targetY=Math.max(80,Math.min(440,targetY));santa.vy+=(targetY-santa.y)*.018;santa.vy*=.88;santa.y+=santa.vy;
 if(santa.shield>0)santa.shield--;
 if(++spawn>Math.max(36,92-score/180)){spawn=0;spawnObject()}
 objects.forEach(o=>{o.x-=speed+(o.type==='gift'?0:1);if(o.type==='gift'&&hit({x:santa.x-20,y:santa.y-22,w:55,h:48},{x:o.x,y:o.y,w:25,h:25})){o.dead=true;gifts++;score+=100;burst(o.x,o.y,'#ffd45a')}
 if(o.type!=='gift'&&!o.dead&&hit({x:santa.x-24,y:santa.y-22,w:68,h:45},o.type==='plane'?{x:o.x,y:o.y,w:88,h:48}:{x:o.x,y:o.y,w:123,h:45},8)){o.dead=true;if(santa.shield===0){lives--;burst(santa.x,santa.y,'#ff6670');santa.shield=100;if(lives<=0)endGame()}}
 });objects=objects.filter(o=>!o.dead&&o.x>-160);
 particles.forEach(p=>{p.x+=p.vx;p.y+=p.vy;p.life--});particles=particles.filter(p=>p.life>0);
 score+=1;updateHud();
}
function updateHud(){$('score').textContent=String(score).padStart(6,'0');$('gifts').textContent=gifts;$('lives').textContent='♥ '.repeat(lives).trim()||'—'}
function draw(){
 background();objects.forEach(o=>o.type==='gift'?drawGift(o):o.type==='plane'?drawPlane(o):drawLine(o));drawSanta();
 particles.forEach(p=>{ctx.globalAlpha=p.life/30;rect(p.x,p.y,5,5,p.c)});ctx.globalAlpha=1;
 if(state==='playing'&&frame%7<4){text('✦',santa.x-65,santa.y-42,16,'#ffe58b')}
}
function loop(){if(state==='playing')update();draw();requestAnimationFrame(loop)}
function start(){score=0;gifts=0;lives=3;frame=0;objects=[];particles=[];spawn=0;santa.y=270;santa.vy=0;santa.shield=0;targetY=270;state='playing';$('overlay').classList.add('hidden');updateHud()}
function endGame(){state='over';highScore=Math.max(highScore,score);localStorage.setItem('santaFlightBest',highScore);$('panel-title').innerHTML='ПОЛЁТ<br><span>ЗАВЕРШЁН</span>';$('panel-copy').textContent=`Твой счёт: ${score}. Подарков собрано: ${gifts}. Рекорд: ${highScore}. Попробуем ещё раз?`;$('start').textContent='ЕЩЁ ОДИН ПОЛЁТ';$('overlay').classList.remove('hidden')}
$('start').addEventListener('click',start);
window.addEventListener('keydown',e=>{if(['ArrowUp','ArrowDown',' '].includes(e.key))e.preventDefault();if(e.key==='ArrowUp'||e.key.toLowerCase()==='w')targetY-=65;if(e.key==='ArrowDown'||e.key.toLowerCase()==='s')targetY+=65;if(e.key===' '&&state==='playing'){state='paused';$('panel-title').innerHTML='ПАУЗА';$('panel-copy').textContent='Нажми кнопку, чтобы продолжить полёт.';$('start').textContent='ПРОДОЛЖИТЬ';$('overlay').classList.remove('hidden');$('start').onclick=()=>{state='playing';$('overlay').classList.add('hidden');$('start').onclick=start}}});
let pointer=false;function move(e){if(state!=='playing')return;const r=canvas.getBoundingClientRect();targetY=(e.clientY-r.top)*H/r.height}
canvas.addEventListener('pointerdown',e=>{pointer=true;canvas.setPointerCapture(e.pointerId);move(e)});canvas.addEventListener('pointermove',e=>{if(pointer)move(e)});canvas.addEventListener('pointerup',()=>pointer=false);canvas.addEventListener('pointercancel',()=>pointer=false);
updateHud();loop();
})();