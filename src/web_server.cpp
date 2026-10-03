#include "web_server.h"

#include <ArduinoJson.h>
#include <WebServer.h>

#include "download_manager.h"
#include "sd_manager.h"
#include "settings.h"
#include "video_player.h"
#include "wifi_manager.h"

namespace {
WebServer server(80);

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Ambient Display</title>
<style>
body{font:16px system-ui,sans-serif;max-width:760px;margin:24px auto;padding:0 16px;background:#111820;color:#eef4f2}h1{font-size:24px;margin:0 0 18px}section{border-top:1px solid #34434a;padding:16px 0}button,select{font:inherit;color:inherit;background:#26353c;border:1px solid #52636a;border-radius:4px;padding:9px 13px;margin:3px;cursor:pointer}button:hover{background:#36545c}.controls{display:flex;gap:8px;flex-wrap:wrap}.muted{color:#a8b8b7}.video{display:flex;align-items:center;justify-content:space-between;gap:8px;padding:8px 0;border-bottom:1px solid #28363d}.video-name{flex:1}.small{font-size:13px}.actions{white-space:nowrap}
</style></head><body><h1>AMBIENT DISPLAY</h1>
<section><div class="muted">Now Playing</div><strong id="current">Offline</strong><div id="connection" class="muted small"></div><div class="controls"><button onclick="action('/api/previous')">Previous</button><button id="toggle" onclick="action('/api/pause')">Play / Pause</button><button onclick="action('/api/next')">Next</button></div></section>
<section><label for="mode">Playback Mode</label><br><select id="mode" onchange="setMode(this.value)"><option value="continuous">Continuous</option><option value="random">Random</option><option value="loop_one">Loop One</option><option value="loop_all">Loop All</option><option value="pause">Pause</option></select></section>
<section><strong>Video updates</strong><div id="update-summary" class="muted">Not checked</div><div class="controls"><button onclick="checkUpdates()">Check for new videos</button><button onclick="downloadUpdates()">Download new videos</button><label><input id="auto-update" type="checkbox" onchange="setAutoUpdate(this.checked)"> Auto update</label></div><div id="progress" class="small"></div><progress id="bar" max="100" value="0" style="width:100%;display:none"></progress></section>
<section><strong>Local videos</strong><div id="videos"></div></section>
<script>
async function action(path){await fetch(path,{method:'POST'});refresh()}
async function setMode(value){await fetch('/api/mode?value='+encodeURIComponent(value),{method:'POST'});refresh()}
async function play(id){await fetch('/api/play?id='+encodeURIComponent(id),{method:'POST'});refresh()}
async function loop(id){await fetch('/api/loop?id='+encodeURIComponent(id),{method:'POST'});refresh()}
async function removeVideo(id){if(confirm('Delete this SD video?')){await fetch('/api/video?id='+encodeURIComponent(id),{method:'DELETE'});refresh()}}
async function checkUpdates(){await fetch('/api/update/check',{method:'POST'});refresh()}
async function downloadUpdates(){await fetch('/api/update/download',{method:'POST'});refresh()}
async function setAutoUpdate(value){await fetch('/api/auto-update?enabled='+value,{method:'POST'})}
function makeButton(label,handler){const b=document.createElement('button');b.textContent=label;b.onclick=handler;return b}
async function refresh(){try{const s=await(await fetch('/api/status')).json();document.querySelector('#current').textContent=s.name||s.current||'No video';document.querySelector('#connection').textContent=(s.wifi?'WiFi '+s.ip:'Offline')+' · '+s.videos+' local videos';document.querySelector('#mode').value=s.mode;document.querySelector('#toggle').textContent=s.playing?'Pause':'Play';document.querySelector('#auto-update').checked=s.auto_update;document.querySelector('#update-summary').textContent='Server videos: '+s.server_videos+' · Local videos: '+s.videos+' · New videos: '+s.new_videos;document.querySelector('#progress').textContent=s.download_id?'Downloading: '+s.download_id+' · '+s.download_percent+'%':s.update;const bar=document.querySelector('#bar');bar.style.display=s.download_id?'block':'none';bar.value=s.download_percent;const list=document.querySelector('#videos');list.replaceChildren();for(const v of s.video_list||[]){const row=document.createElement('div');row.className='video';const name=document.createElement('span');name.className='video-name';name.textContent=v.name;const actions=document.createElement('span');actions.className='actions';actions.append(makeButton('Play',()=>play(v.id)),makeButton('Loop',()=>loop(v.id)),makeButton('Delete',()=>removeVideo(v.id)));row.append(name,actions);list.append(row)}if(!list.childElementCount){const empty=document.createElement('p');empty.className='muted';empty.textContent='No local videos found.';list.append(empty)}}catch(e){}}
refresh();setInterval(refresh,3000);
</script></body></html>)rawliteral";

void sendStatus() {
  JsonDocument document;
  document["mode"] = playbackModeName(currentPlaybackMode());
  document["playing"] = isVideoPlaying();
  document["current"] = currentVideoId();
  document["name"] = currentVideoName();
  document["videos"] = localVideos().size();
  document["wifi"] = wifiConnected();
  document["ip"] = wifiIpAddress();
  document["auto_update"] = applicationSettings().autoUpdate;
  document["server_videos"] = serverVideoCount();
  document["new_videos"] = newVideoCount();
  document["pending_downloads"] = pendingDownloadCount();
  document["download_id"] = downloadingVideoId();
  document["download_percent"] = downloadProgressPercent();
  document["update"] = updateMessage();
  JsonArray entries = document["video_list"].to<JsonArray>();
  for (const VideoEntry& video : localVideos()) {
    JsonObject item = entries.add<JsonObject>();
    item["id"] = video.id;
    item["name"] = video.name;
    item["size"] = video.size;
  }
  String response;
  serializeJson(document, response);
  server.send(200, "application/json", response);
}

void sendError(int status, const char* message) {
  JsonDocument document;
  document["error"] = message;
  String response;
  serializeJson(document, response);
  server.send(status, "application/json", response);
}
}

void beginWebServer() {
  if (!wifiConnected()) {
    Serial.println("[WEB] Waiting for WiFi; server not started");
    return;
  }
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/next", HTTP_POST, []() { playNext(); server.send(200, "text/plain", "OK"); });
  server.on("/api/previous", HTTP_POST, []() { playPrevious(); server.send(200, "text/plain", "OK"); });
  server.on("/api/pause", HTTP_POST, []() {
    if (currentPlaybackMode() == PLAY_PAUSE) playVideo();
    else setPlaybackMode(PLAY_PAUSE);
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/play", HTTP_POST, []() {
    if (!server.hasArg("id") || !selectVideo(server.arg("id"))) {
      sendError(404, "Video not found");
      return;
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/mode", HTTP_POST, []() {
    PlaybackMode mode;
    if (!server.hasArg("value") || !parsePlaybackMode(server.arg("value"), mode)) {
      sendError(400, "Invalid playback mode");
      return;
    }
    setPlaybackMode(mode);
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/loop", HTTP_POST, []() {
    if (!server.hasArg("id") || !selectVideo(server.arg("id"))) {
      sendError(404, "Video not found");
      return;
    }
    setPlaybackMode(PLAY_LOOP_ONE);
    server.send(200, "text/plain", "OK");
  });
  server.on("/api/video", HTTP_DELETE, []() {
    if (!server.hasArg("id")) {
      sendError(400, "Missing video id");
    } else if (server.arg("id") == currentVideoId()) {
      sendError(409, "Select another video before deleting the current one");
    } else if (!removeLocalVideo(server.arg("id"))) {
      sendError(404, "Video not found");
    } else {
      server.send(200, "text/plain", "Deleted");
    }
  });
  server.on("/api/update/check", HTTP_POST, []() {
    if (!checkForNewVideos()) {
      sendError(503, updateMessage().c_str());
      return;
    }
    sendStatus();
  });
  server.on("/api/update/download", HTTP_POST, []() {
    requestDownloadNewVideos();
    sendStatus();
  });
  server.on("/api/update", HTTP_POST, []() {
    if (!checkForNewVideos()) {
      sendError(503, updateMessage().c_str());
      return;
    }
    requestDownloadNewVideos();
    sendStatus();
  });
  server.on("/api/auto-update", HTTP_POST, []() {
    if (!server.hasArg("enabled")) {
      sendError(400, "Missing enabled value");
      return;
    }
    applicationSettings().autoUpdate = server.arg("enabled") == "true";
    saveSettings(applicationSettings());
    server.send(200, "text/plain", "OK");
  });
  server.onNotFound([]() { sendError(404, "Not found"); });
  server.begin();
  Serial.println("[WEB] Local web server started on port 80");
}

void handleWebServer() {
  if (wifiConnected()) {
    server.handleClient();
  }
}