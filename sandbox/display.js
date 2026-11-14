/* Shared 240 x 320 bitmap display used by the studio and device prototype. */
(function (root) {
  'use strict';
  function createRenderer(canvas,font) {
    const {displayText,project,shade}=root.PoetryEngine;
    const ctx=canvas.getContext('2d',{alpha:false});
    function glyphs(text,callback){for(let c=0;c<text.length;c++){const glyph=font[text.charCodeAt(c)-32];if(!glyph)continue;for(let col=0;col<5;col++){const bits=glyph[col];for(let row=0;row<8;){if(!(bits&(1<<row))){row++;continue;}const first=row;while(row<8&&(bits&(1<<row)))row++;callback(c*6+col,first,1,row-first);}}}}
    function grey(amount){const g=Math.floor(Math.max(0,Math.min(1,amount))*255);return `rgb(${(g>>3)*255/31},${(g>>2)*255/63},${(g>>3)*255/31})`;}
    function author(experience,opacity){ctx.fillStyle=grey(.38*opacity);const text=displayText(experience.poem.by),x=(240-text.length*6)/2,y=experience.arrangement.authorY;glyphs(text,(gx,gy,w,h)=>ctx.fillRect(Math.trunc(x+gx),Math.trunc(y+gy),w,h));}
    return function render(experience){
      ctx.fillStyle='#000';ctx.fillRect(0,0,240,320);
      // Both poems share one depth-sorted space during a change.
      const layers=experience.layers();
      const order=layers.flatMap(layer=>layer.poses.map((pose,i)=>({pose,unit:experience.units(layer.scene)[i],layer}))).sort((a,b)=>a.pose.z-b.pose.z);
      for(const {pose,unit,layer} of order){const {mode,progress,scene,opacity}=layer,size=unit.size,width=unit.text.length*6*size;
        const settling=pose.progress??progress;
        let brightness=shade(pose.z,scene.motion.depth);
        if(mode==='ASSEMBLING')brightness+=(.9-brightness)*settling;
        if(mode==='READING')brightness=.9;
        if(mode==='DISSOLVING')brightness=.9+(brightness-.9)*settling;
        brightness*=opacity;
        if(unit.echo){brightness*=.78;if(mode==='ASSEMBLING')brightness*=1-settling;else if(mode==='READING')brightness=0;else if(mode==='DISSOLVING')brightness*=settling;}
        if(brightness<.003)continue;
        ctx.fillStyle=grey(brightness);
        glyphs(unit.text,(gx,gy,w,h)=>{
          const x=gx*size-width/2,y=gy*size-4*size;
          if(mode==='READING'){ctx.fillRect(Math.trunc(120+pose.x+x),Math.trunc(160+pose.y+y),w*size,h*size);return;}
          const corners=[[x,y],[x+w*size,y],[x+w*size,y+h*size],[x,y+h*size]].map(([px,py])=>project(px,py,pose,scene.motion.perspective));
          if(corners.some(p=>!p||!p.every(Number.isFinite)))return;
          if(corners.every(([x])=>x<0)||corners.every(([x])=>x>240)||corners.every(([,y])=>y<0)||corners.every(([,y])=>y>320))return;
          ctx.beginPath();corners.forEach(([x,y],i)=>i?ctx.lineTo(x,y):ctx.moveTo(x,y));ctx.closePath();ctx.fill();
        });
      }
      const current=layers[0];
      if(current.mode!=='MOVING')author(experience,current.mode==='READING'?1:current.mode==='ASSEMBLING'?current.progress:1-current.progress);
    }
  }
  const api={createRenderer};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.PoetryDisplay=api;
})(globalThis);
