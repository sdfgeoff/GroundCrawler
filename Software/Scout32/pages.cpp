// GENERATED FILE. DO NOT EDIT
// INSTEAD RUN THE SCIPT ./generate_pages.py

#include "Arduino.h"
#include "esp_http_server.h"


static const char PROGMEM STYLE_CSS[] = R"rawliteral(
#main {
  text-align: center;
}

#control {
  width: 300px;
  height: 300px;
  display: inline-block;
  vertical-align: middle;
}

#stream-container {
  width: auto;
  height: 300px;
  display: inline-block;
  vertical-align: middle;
}

#stream {
  width: auto;
  height: 100%;
  margin: auto;
}

#settings {
  position: fixed;
  left: calc(50% - 150px);
  top: 0;
  width: 300px;
  background: white;
  padding: 5px;
}

#settings-background {
  position: fixed;
  left: 0;
  top: 0;
  width: 100vw;
  height: 100vh;
  display: none;
  background: rgba(0,0,0,0.5);
}
#settings {
  margin-top: 10px;
  display: none;
}
.settings-table {
  width:100%;
}

#settings h1 {
  border-bottom: solid black 1px;
}

#settings h2 {
  border-bottom: solid black 1px;
}


)rawliteral";


static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <link rel="stylesheet" href="style.css">
  <script src="control.js"></script>
  <title>Wifi Rover Control</title>
</head>

<body>
  <section id="main">
    <div id="stream-container" class="image-container">
      <a href="capture" download="capture.jpg">
      <img id="stream" src="loading.jpg">
      </a>
    </div>
    <div id="control" , class="control">
      <svg id="dpad" version="1.1" viewBox="-100 -100 200 200" xmlns="http://www.w3.org/2000/svg">
        <g id="padstyle" fill="none" stroke="#000" stroke-width="2">
          <path d="m-10 -20 v-30 h-10 l 20 -30  20  30 h -10 v30" />
          <path d="m10 20   v 30 h 10 l-20  30 -20 -30 h  10 v-30" />
          <path d="m-20 10  h-30 v10  l-30 -20  30 -20 v  10 h30" />
          <path d="m20 -10  h 30 v-10 l 30  20 -30  20 v -10 h-30" />
          <circle id="deadzone" cx="0" cy="0" r="10" />
          <circle id="indicator" cx="0" cy="0" r="20" />
          <rect id="boundary" x="-100" y="-100" width="200" height="200" />
        </g>
      </svg>
      <button onclick="showSettings()">Settings</button>
    </div>
    <div id="settings-background" onclick="hideSettings()"></div>
    <div id="settings">
      <h1>Settings</h1>
      <table class="settings-table">
        <tr>
          <td>Lights</td>
          <td><input type="range" id="flash" min="0" max="255" value="10"
              onchange="setParameter('flash', this.value)"></td>
        </tr>
        
        <tr>
          <td colspan="2"><h2>Video</h2></td>
        </tr>
        <tr>
          <td>Quality</td>
          <td><input type="range" id="quality" min="10" max="63" value="10"
              onchange="setParameter('quality', this.value)"></td>
        </tr>
        <tr>
          <td>Resolution</td>
          <td><input type="range" id="framesize" min="0" max="6" value="5"
              onchange="setParameter('framesize', this.value)"></td>
        </tr>
      </table>
      <button onclick="hideSettings()">Close</button>
      </div>
  </section>
  </section>
</body>

</html>

)rawliteral";


static const char PROGMEM CONTROL_JS[] = R"rawliteral(
"use strict";
/*
Copyright (c) 2021 Ross Peters, Geoffrey Irons

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

const SPEED_CHANGE_THRESHOLD = 0.1 // How much the speed has to change by in order for a new speed to be sent. Shold be smaller than SPEED_MIN_THRESHOLD
const SPEED_MIN_THRESHOLD = 0.15 // Speeds less than this are rounded to zero.
const SPEED_MIN_SEND_DELAY_MS = 50 // Minimum time between messages
const SPEED_MAX_SEND_DELAY_MS = 500 // Maximum time between messages

// Functions for Controls via Keypress
var speed = {
    forward: 0,
    steer: 0,
}

// Used for debouncing commands to the rover
var oldSpeed = {
    forward: 0,
    steer: 0,
    timeSent: 0,
}

var mouseDown = false;


window.addEventListener("load", () => {
    // Functions to control streaming
    let source = document.getElementById('stream');
    source.src = document.location.origin + ':81/stream';

    let dpad = document.getElementById('dpad')
    const padStroke = document.getElementById('padstyle').getAttribute('stroke-width')
    const dpadsize = 1 + SPEED_CHANGE_THRESHOLD + padStroke
    dpad.setAttribute('viewBox', `${-100 * dpadsize} ${-100 * dpadsize} ${200 * dpadsize} ${200 * dpadsize}`)
    document.getElementById('deadzone').setAttribute('r', SPEED_MIN_THRESHOLD * 100)
    document.getElementById('indicator').setAttribute('r', SPEED_CHANGE_THRESHOLD * 100)

    dpad.addEventListener('mousedown', (event) => {
        setSpeedFromClientPosition(event.clientX, event.clientY)
        mouseDown = true
    });
    dpad.addEventListener('mousemove', (event) => {
        if (mouseDown) {
            setSpeedFromClientPosition(event.clientX, event.clientY)
            event.preventDefault();
        }
        
    });

    document.addEventListener('mouseup', (event) => {
        speed.steer = 0.0
        speed.forward = 0.0
        mouseDown = false
    });
    
    document.addEventListener('touchstart', (event) => {
        setSpeedFromClientPosition(event.changedTouches[0].clientX, event.changedTouches[0].clientY)
    });
    document.addEventListener('touchmove', (event) => {
        setSpeedFromClientPosition(event.changedTouches[0].clientX, event.changedTouches[0].clientY)
    });
    document.addEventListener('touchend', (event) => {
        speed.steer = 0.0
        speed.forward = 0.0
    });
    
    function setSpeedFromClientPosition(x, y) {
        let bounds = dpad.getBoundingClientRect()
        speed.steer = 2.0 * ((x - bounds.left) / bounds.width - 0.5) * dpadsize
        speed.forward = -2.0 * ((y - bounds.top) / bounds.height - 0.5) * dpadsize
    }
});




//Keypress Events
document.addEventListener('keydown', function (keyon) {
    keyon.preventDefault();
    switch (keyon.code) {
        case 'ArrowUp':
            speed.forward = 1.0;
            break;
        case 'ArrowDown':
            speed.forward = -1.0;
            break;
        case 'ArrowLeft':
            speed.steer = -1.0;
            break;
        case 'ArrowRight':
            speed.steer = 1.0;
            break;
    }
});
//KeyRelease Events
document.addEventListener('keyup', function (keyoff) {
    switch (keyoff.code) {
        case 'ArrowUp':
        case 'ArrowDown':
            speed.forward = 0.0;
            break;
        case 'ArrowLeft':
        case 'ArrowRight':
            speed.steer = 0.0;
            break;
    }
});

window.setInterval(function () {
    if ((speed.forward ** 2 + speed.steer ** 2) < SPEED_MIN_THRESHOLD ** 2) {
        speed.forward = 0.0
        speed.steer = 0.0
    }
    speed.forward = Math.min(1.0, Math.max(-1.0, speed.forward))
    speed.steer = Math.min(1.0, Math.max(-1.0, speed.steer))

    const speedChanged = (Math.abs(speed.forward - oldSpeed.forward) > SPEED_CHANGE_THRESHOLD) || (Math.abs(speed.steer - oldSpeed.steer) > SPEED_CHANGE_THRESHOLD)
    const time = new Date().getTime();

    if (speedChanged || (time - oldSpeed.timeSent > SPEED_MAX_SEND_DELAY_MS)) {

        Object.assign(oldSpeed, speed);
        oldSpeed.timeSent = time;
        drive(speed.forward, speed.steer);
    }
}, SPEED_MIN_SEND_DELAY_MS);


function setParameter(variable, value) {
    fetch(document.location.origin + `/config?${variable}=${value}`).catch((e) => { })
}

function drive(forward, steer) {
    fetch(document.location.origin + `/drive?forward=${Math.round(forward * 100)}&steer=${Math.round(steer * 100)}`).catch((e) => { })
    document.getElementById('indicator').setAttribute('cy', -forward * 100)
    document.getElementById('indicator').setAttribute('cx', steer * 100)
}

function showSettings() {
    document.getElementById('settings-background').style.display = "block";
    document.getElementById('settings').style.display = "block";
}
function hideSettings() {
    document.getElementById('settings-background').style.display = "none";
    document.getElementById('settings').style.display = "none";
}


)rawliteral";


static esp_err_t style_css_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req, (const char *)STYLE_CSS, strlen(STYLE_CSS));
}


static esp_err_t index_html_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}


static esp_err_t control_js_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/javascript");
    return httpd_resp_send(req, (const char *)CONTROL_JS, strlen(CONTROL_JS));
}

void registerStaticPages(httpd_handle_t httpd){
    httpd_uri_t style_css_handler_uri = {
        .uri       = "/style.css",
        .method    = HTTP_GET,
        .handler   = style_css_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpd, &style_css_handler_uri);
    
    
    httpd_uri_t index_html_handler_uri = {
        .uri       = "/index.html",
        .method    = HTTP_GET,
        .handler   = index_html_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpd, &index_html_handler_uri);
    
    
    httpd_uri_t control_js_handler_uri = {
        .uri       = "/control.js",
        .method    = HTTP_GET,
        .handler   = control_js_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpd, &control_js_handler_uri);
    
    
    httpd_uri_t roothandler_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_html_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpd, &roothandler_uri);
}
