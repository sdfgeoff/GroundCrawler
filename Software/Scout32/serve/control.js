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

