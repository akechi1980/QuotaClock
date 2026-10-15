"""Build the offline preview from the unchanged firmware HTML plus an in-memory API."""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MOCK = r'''<script>
// Preview only: this replaces all fetch calls. Nothing reaches a real device.
const previewState={version:'SDD MONO 0.2.4-dev / PREVIEW',connected:true,ip:'192.168.1.50',csrf:'preview-only',brightness:50,rotation:0,url:'http://192.168.1.100:8766/api/progress',interval_minutes:5,mode:'progress',city:'101010100',weather_interval:10,source_state:'PREVIEW / example values',online:true,hourly_enabled:false,hourly_volume:30,hourly_beeps:3,hourly_window_seconds:5,clock_synced:true,audio_state:'not_tested',audio_busy:false,tasks:[{label:'OpenAI 5 Hours',percent:5,stale:false},{label:'OpenAI Week',percent:54,stale:false},{label:'Cursor Month',percent:18,stale:false}]};
previewState.mode=new URLSearchParams(location.search).get('mode')==='weather'?'weather':'progress';
let previewAudio;
function previewModeNote(){const select=document.getElementById('mode');const note=document.getElementById('previewModeNote');if(select&&note)note.textContent='界面选择：'+(select.value==='weather'?'V1.4天气时钟':'灰阶时钟 / 三行进度')+'；点击底部“保存设置”应用（仅模拟），可随时切换回来。'}
window.fetch=async(input,options={})=>{const path=String(input);let body={},status=200,result={};try{if(options.body)body=JSON.parse(options.body)}catch(e){return new Response(JSON.stringify({error:'invalid JSON'}),{status:400})}
if(path==='/api/status')result=structuredClone(previewState);
else if(path==='/api/config'){Object.assign(previewState,body);result={ok:true};setTimeout(()=>{previewModeNote();document.getElementById('notice').textContent='模拟已保存：'+(previewState.mode==='weather'?'V1.4天气时钟':'灰阶时钟 / 三行进度')+'。没有修改设备。'},0)}
else if(path==='/api/reminder/settings'){if(typeof body.hourly_enabled!=='boolean'||Object.keys(body).length!==1){status=400;result={error:'invalid reminder toggle'}}else if(window.previewFailNextSave){window.previewFailNextSave=false;status=500;result={error:'模拟保存失败'}}else{previewState.hourly_enabled=body.hourly_enabled;result={ok:true,hourly_enabled:previewState.hourly_enabled}}}
else if(path==='/api/reminder/test'){const v=Number(body.volume)/100,n=body.beeps??previewState.hourly_beeps;if(!Number.isInteger(n)||n<1||n>5)return new Response(JSON.stringify({error:'invalid beep count'}),{status:400});if(v){previewAudio??=new(window.AudioContext||window.webkitAudioContext)();await previewAudio.resume();const t=previewAudio.currentTime+.05;for(let i=0;i<n;i++){const o=previewAudio.createOscillator(),g=previewAudio.createGain(),s=t+i*.5;o.type='sine';o.frequency.value=880;g.gain.setValueAtTime(0,s);g.gain.linearRampToValueAtTime(v*.12,s+.015);g.gain.setValueAtTime(v*.12,s+.13);g.gain.linearRampToValueAtTime(0,s+.18);o.connect(g);g.connect(previewAudio.destination);o.start(s);o.stop(s+.2);o.onended=()=>{o.disconnect();g.disconnect()}}}result={ok:true};status=202;setTimeout(()=>document.getElementById('notice').textContent=v?'浏览器本机试听'+n+'声；不是设备喇叭实测。':'0%静音；未修改设备。',0)}
else if(path==='/api/portal'){result={ok:true};setTimeout(()=>document.getElementById('notice').textContent='仅模拟重新配网；没有连接或修改设备。',0)}
else{result={error:'Offline preview blocks network requests'};status=403}
return new Response(JSON.stringify(result),{status,headers:{'Content-Type':'application/json'}})};
window.addEventListener('DOMContentLoaded',()=>{document.getElementById('mode').addEventListener('change',previewModeNote);previewModeNote()});
</script>'''


def main():
    for directory in (ROOT / 'clock_rom', ROOT / 'github_submit/clock_rom'):
        if not (directory / 'web_page.html').exists():
            continue
        original = (directory / 'web_page.html').read_text(encoding='utf-8')
        result = original.replace('<title>MONO / CLOCK · 管理</title>', '<title>QuotaClock · 完整WEB管理交互预览</title>')
        result = result.replace('<main>', '<main><aside style="border:1px solid #75838c;border-radius:8px;padding:14px 18px;margin-bottom:22px;color:#b9c2c7"><strong>离线交互预览 · 与当前固件管理页同源</strong><br>所有数据和IP均为示例，不连接实机、不持久化设置。<br><span id="previewModeNote"></span></aside>', 1)
        result = result.replace('<script>', MOCK + '\n<script>', 1)
        result = result.replace("message=t=>el('notice').textContent=t", "message=t=>el('notice').textContent='仅本机模拟：'+t")
        result = result.replace('modeFields();ready=true', 'modeFields();previewModeNote();ready=true')
        result = result.replace("message('已保存，显示设置已应用。')", "message('已保存：'+(el('mode').value==='weather'?'V1.4天气时钟':'灰阶时钟 / 三行进度')+'；未修改设备。')")
        result = result.replace("'试听已排队，请确认设备三声提示；无需保存。'", "'浏览器本机试听三声；不代表设备音效，无需保存。'")
        (directory / 'hourly_web_preview.html').write_text(result, encoding='utf-8')
    print('Updated source and submission previews from their firmware HTML; production HTML unchanged.')


if __name__ == '__main__':
    main()
