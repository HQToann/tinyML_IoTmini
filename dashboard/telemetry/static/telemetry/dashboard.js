const labels={normal:"Bình thường",attention:"Chú ý",high_risk_in_model:"Nguy cơ cao trong mô hình"};
function n(v,d=1){let x=Number(v);return Number.isFinite(x)?x.toLocaleString("vi-VN",{maximumFractionDigits:d}):"—"}
function dt(v){return v?new Intl.DateTimeFormat("vi-VN",{dateStyle:"short",timeStyle:"medium"}).format(new Date(v)):"—"}
function ago(v){let s=Math.max(0,Math.floor((Date.now()-new Date(v).getTime())/1000));return s<5?"Vừa nhận":s<60?s+" giây trước":Math.floor(s/60)+" phút trước"}
function esc(v){return String(v).replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]))}
function showError(m){let e=document.getElementById("error");e.textContent=m;e.classList.add("show");setTimeout(()=>e.classList.remove("show"),4500)}
function render(r,total){
 let stale=Date.now()-new Date(r.created_at).getTime()>15000,b=document.getElementById("banner");b.className="banner status-"+r.status+(stale?" stale":"");
 document.getElementById("symbol").textContent={normal:"✓",attention:"!",high_risk_in_model:"‼"}[r.status]||"…";
 document.getElementById("state").textContent=labels[r.status]||r.status_label||"Không rõ";
 document.getElementById("detail").textContent=stale?"Thiết bị chưa gửi dữ liệu mới hơn 15 giây.":"Trạng thái phân loại gần nhất từ model trên ESP32.";
 document.getElementById("age").textContent=ago(r.created_at);
 document.getElementById("moisture").textContent=n(r.soil_moisture_pct);
 document.getElementById("angle").textContent=n(r.tilt_angle_deg,2);
 document.getElementById("rate").textContent=n(r.tilt_rate_deg_per_min,2);
 document.getElementById("device").textContent=r.device_id;document.getElementById("device2").textContent=r.device_id;
 document.getElementById("lastseen").textContent=ago(r.created_at);document.getElementById("received").textContent=dt(r.created_at);
 let badge=document.getElementById("badge");badge.textContent=stale?"Ngoại tuyến":"Đang gửi";badge.className=stale?"pill offline":"pill online";
 document.getElementById("count").textContent=total+" bản tin gần nhất";
}
function draw(items){
 let c=document.getElementById("chart"),r=c.getBoundingClientRect();if(!r.width||!r.height)return;
 let d=devicePixelRatio||1;c.width=r.width*d;c.height=r.height*d;let x=c.getContext("2d");x.scale(d,d);
 let data=[...items].reverse(),series=[["soil_moisture_pct","ĐỘ ẨM (%)","#49a6ff"],["tilt_angle_deg","GÓC NGHIÊNG (°)","#ffbd4a"],["tilt_rate_deg_per_min","TỐC ĐỘ ĐỔI GÓC (°/PHÚT)","#ac91ff"]];
 series.forEach(([key,title,color],i)=>{let h=r.height/3,top=i*h,yt=top+20,yb=top+h-12,l=48,rr=r.width-6,v=data.map(a=>Number(a[key])).filter(Number.isFinite);
 x.fillStyle="#8294ac";x.font="10px system-ui";x.fillText(title,2,top+13);x.strokeStyle="#26364b";x.beginPath();x.moveTo(l,yb);x.lineTo(rr,yb);x.stroke();if(!v.length)return;
 let mn=Math.min(...v),mx=Math.max(...v);if(mn===mx){mn--;mx++}let p=(mx-mn)*.12;mn-=p;mx+=p;x.strokeStyle=color;x.lineWidth=2;x.beginPath();v.forEach((a,j)=>{let px=l+(v.length<2?0:(rr-l)*j/(v.length-1)),py=yb-(a-mn)/(mx-mn)*(yb-yt);j?x.lineTo(px,py):x.moveTo(px,py)});x.stroke();x.fillStyle="#7588a0";x.fillText(n(mx),2,yt+4);x.fillText(n(mn),2,yb);
 });
}
function table(items){
 let b=document.getElementById("rows");if(!items.length){b.innerHTML='<tr><td colspan="6" class="emptyrow">Chưa có dữ liệu từ ESP32.</td></tr>';return}
 b.innerHTML=items.slice(0,20).map(r=>'<tr><td>'+dt(r.created_at)+'</td><td>'+esc(r.device_id)+'</td><td>'+n(r.soil_moisture_pct)+'%</td><td>'+n(r.tilt_angle_deg,2)+'°</td><td>'+n(r.tilt_rate_deg_per_min,2)+'°/phút</td><td><span class="tag tag-'+r.status+'">'+esc(labels[r.status]||r.status_label)+'</span></td></tr>').join("");
}
async function refresh(){try{let res=await fetch("/api/readings/?limit=60",{cache:"no-store"});if(!res.ok)throw Error("HTTP "+res.status);let items=(await res.json()).items||[];document.getElementById("dot").className=items.length&&Date.now()-new Date(items[0].created_at).getTime()<15000?"online":"";document.getElementById("connection").textContent="Dashboard hoạt động";table(items);draw(items);if(items.length)render(items[0],items.length)}catch(e){document.getElementById("connection").textContent="Mất kết nối";showError("Không tải được dữ liệu: "+e.message)}}
refresh();setInterval(refresh,1000);window.addEventListener("resize",refresh);
