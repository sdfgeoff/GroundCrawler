// Define Servo PWM Values & Stopped Variable
int endpoint = 4095; // Multiplier for servo endpoints. Should be around 60 on modded servos, around 220 for TowerPro 360.
int speed = 8 * endpoint;
int trim = 0;
int neutral = 4915; // Servo Neutral Point
int lneutral = neutral;
int rneutral = neutral;

// Libraries. If you get errors compiling, please downgrade ESP32 by Espressif.
// Use version 1.0.2 (Tools, Manage Libraries).
#include <esp32-hal-ledc.h> 
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"

#include "chassis.h"

#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"

// Stream Encoding
typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    {
        size_t fb_len = 0;
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
        return res;
    }

    dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    if (!image_matrix) {
        esp_camera_fb_return(fb);
        Serial.println("dl_matrix3du_alloc failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    out_buf = image_matrix->item;
    out_len = fb->width * fb->height * 3;
    out_width = fb->width;
    out_height = fb->height;

    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    esp_camera_fb_return(fb);
    if(!s){
        dl_matrix3du_free(image_matrix);
        Serial.println("to rgb888 failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    jpg_chunking_t jchunk = {req, 0};
    s = fmt2jpg_cb(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
    dl_matrix3du_free(image_matrix);
    if(!s){
        Serial.println("JPEG compression failed");
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];
    dl_matrix3du_t *image_matrix = NULL;

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
             {
                if(fb->format != PIXFORMAT_JPEG){
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if(!jpeg_converted){
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                } else {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        Serial.printf("MJPG: %uB %ums (%.1ffps)\n",
            (uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time           
        );
    }

    last_frame = 0;
    return res;
}


// Control Handling from Server
// Setup states of motion for Scout
enum state {fwd,rev,stp};
state actstate = stp;

static esp_err_t cmd_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;
    
    if(!strcmp(variable, "framesize")) 
    {
        Serial.println("framesize");
        if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) 
    {
      Serial.println("quality");
      res = s->set_quality(s, val);
    }
    //Remote Control
    else if(!strcmp(variable, "flash")) 
    {
      setLedBrightness(val);
    }
    else if(!strcmp(variable, "speed")) 
    {
      if      (val > 8) val = 8;
      else if (val <   0) val = 0;       
      speed = val * endpoint;
    }
    else if(!strcmp(variable, "ltrim")) 
    {
      if      (val > 192) val = 192;
      else if (val < -192) val = -192;
      lneutral = neutral+val;
      actstate = stp;
      setLeftMotor(0); // Send no pulse to disable servo (no drift).
      setRightMotor(0); // Send no pulse to disable servo (no drift).  
    }
    else if(!strcmp(variable, "rtrim")) 
    {
      if      (val > 192) val = 192;
      else if (val < -192) val = -192;
      rneutral = neutral-val;
      actstate = stp;
      setLeftMotor(0); // Send no pulse to disable servo (no drift).
      setRightMotor(0); // Send no pulse to disable servo (no drift).
    }
    else if(!strcmp(variable, "car")) {  
      if (val==1) {
        Serial.println("Forward");
        actstate = fwd; // Set state to modify left & right behavior while moving.   
        setLeftMotor(speed);
        setRightMotor(speed);
      }
      else if (val==2) {
        Serial.println("Backward");  
        actstate = rev; // Set state to modify left & right behavior while moving.      
        setLeftMotor(-speed);
        setRightMotor(-speed);  
      }
      else if (val==3) {
        Serial.println("TurnLeft");
        setLeftMotor(-speed);
        setRightMotor(speed);       
      }
      else if (val==4) {
        Serial.println("TurnRight");
        setLeftMotor(speed);
        setRightMotor(-speed);       
      }
      else {
        Serial.println("Stop");
        actstate = stp;
        setLeftMotor(0);
        setRightMotor(0);      
      } 
    }        
    else 
    { 
      Serial.println("variable");
      res = -1; 
    }

    if(res){ return httpd_resp_send_500(req); }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024]; // Tell Jason his name is spelled wrong.

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

// Front End / GUI Webpage
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=400px,user-scalable=no">
        <title>PilotHobbies Scout32 Control</title>
    <link href="data:image/png;base64,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" rel="icon" />
        <style>
          body{width:100%;padding:0;margin:0;overflow-x:hidden;line-height:0;font-family:sans-serif;background:#FFF;color:#000;font-size:14px}
      section table{width:400px;margin:auto;padding:0;}
      section img{width:400px;margin:auto;display:block;padding:0;}
      figure{margin:0;padding:0;}
      tr{margin:0;}
      button{display:block;width:128px;height:64px;margin:2px 0;border:0;color:#fff;background:#734CA7;border-radius:8px;outline:0}
      button{background:linear-gradient(to bottom,#856fa5 0%,#734ca7 50%,#673ba5 51%,#7f63a5 100%);box-shadow:0 2px 1px #15052D;}
      button{-webkit-touch-callout:none;-webkit-user-select:none;-khtml-user-select:none;-moz-user-select:none;user-select:none;}
      button.lr{height:136px;}
      input[type=range]{-webkit-appearance:none;width:100%;height:24px;background:#363636;margin:2px 0;}
      input[type=range]:focus{outline:0}input[type=range]::-webkit-slider-runnable-track{width:100%;height:4px;cursor:pointer;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}
      input[type=range]::-webkit-slider-thumb{border:1px solid rgba(0,0,30,0);height:32px;width:32px;border-radius:50px;background:#734CA7;cursor:pointer;-webkit-appearance:none;margin-top:-14px}
      input[type=range]:focus::-webkit-slider-runnable-track{background:#EFEFEF}
      input[type=range]::-moz-range-track{width:100%;height:4px;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}
      input[type=range]::-moz-range-thumb{border:1px solid rgba(0,0,30,0);height:32px;width:32px;border-radius:50px;background:#734CA7;cursor:pointer}
        </style>
    </head>
    <body>
       <section id="main">
    <img id="banner" src="data:image/png;base64,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">
     <figure>
      <div id="stream-container" class="image-container">
      <img width="400px" height="300px" id="stream" src="">
      </div>
    </figure>
      <section id="buttons">
        <table>
        <tr><td align="center"><button id="forward" onpointerdown="document.dispatchEvent(fwdpress);" onpointerup="document.dispatchEvent(fwdrelease);" onpointerleave="document.dispatchEvent(fwdrelease);">Forward</button></td><td align="center" rowspan="2"><button class="lr" id="turnleft" onpointerdown="document.dispatchEvent(leftpress);" onpointerup="document.dispatchEvent(leftrelease);" onpointerleave="document.dispatchEvent(leftrelease);">Left</button></td><td align="center" rowspan="2"><button class="lr" id="turnright" onpointerdown="document.dispatchEvent(rightpress);" onpointerup="document.dispatchEvent(rightrelease);" onpointerleave="document.dispatchEvent(rightrelease);">Right</button></td></tr>
        <tr><td align="center"><button id="backward"  onpointerdown="document.dispatchEvent(backpress);" onpointerup="document.dispatchEvent(backrelease);" onpointerleave="document.dispatchEvent(backrelease);">Backward</button></td></tr>
        <tr><td align="center">Speed</td><td align="center" colspan="2"><input type="range" id="speed" min="0" max="8" value="8" onchange="try{fetch(document.location.origin+'/control?var=speed&val='+this.value);}catch(e){}"></td></tr>
        <tr><td align="center">Left Trim</td><td align="center" colspan="2"><input type="range" id="speed" min="-192" max="192" value="0" onchange="try{fetch(document.location.origin+'/control?var=ltrim&val='+this.value);}catch(e){}"></td></tr>
        <tr><td align="center">Right Trim</td><td align="center" colspan="2"><input type="range" id="speed" min="-192" max="192" value="0" onchange="try{fetch(document.location.origin+'/control?var=rtrim&val='+this.value);}catch(e){}"></td></tr>
        <tr><td align="center">Lights</td><td align="center" colspan="2"><input type="range" id="flash" min="0" max="255" value="10" onchange="try{fetch(document.location.origin+'/control?var=flash&val='+this.value);}catch(e){}"></td></tr>
        <tr><td align="center">Quality</td><td align="center" colspan="2"><input type="range" id="quality" min="10" max="63" value="10" onchange="try{fetch(document.location.origin+'/control?var=quality&val='+this.value);}catch(e){}"></td></tr>
        <tr><td align="center">Resolution</td><td align="center" colspan="2"><input type="range" id="framesize" min="0" max="6" value="5" onchange="try{fetch(document.location.origin+'/control?var=framesize&val='+this.value);}catch(e){}"></td></tr>
        </table>
      </section>         
    </section>
    <script>  
   // Functions to control streaming
  var source = document.getElementById('stream');
  source.src = document.location.origin+':81/stream';
  // Functions for Controls via Keypress
  var keyforward=0;
    var keybackward=0; 
    var keyleft=0 ;
    var keyright=0;
  // Emulate Keypress with Touch
  var fwdpress = new KeyboardEvent('keydown', {'keyCode':38, 'which':38});
  var fwdrelease = new KeyboardEvent('keyup', {'keyCode':38, 'which':38});
  var backpress = new KeyboardEvent('keydown', {'keyCode':40, 'which':40});
  var backrelease = new KeyboardEvent('keyup', {'keyCode':40, 'which':40});
  var leftpress = new KeyboardEvent('keydown', {'keyCode':37, 'which':37});
  var leftrelease = new KeyboardEvent('keyup', {'keyCode':37, 'which':37});
  var rightpress = new KeyboardEvent('keydown', {'keyCode':39, 'which':39});
  var rightrelease = new KeyboardEvent('keyup', {'keyCode':39, 'which':39});
  //Keypress Events
   document.addEventListener('keydown',function(keyon){
    keyon.preventDefault();
      if ((keyon.keyCode == '38') && (!keybackward) && (!keyforward)) {keyforward = 1;}
      else if ((keyon.keyCode == '40') && (!keyforward) && (!keybackward)){keybackward = 1;}
      else if ((keyon.keyCode == '37') && (!keyright) && (!keyleft)){keyleft = 1;}
      else if ((keyon.keyCode == '39') && (!keyleft) && (!keyright)){keyright = 1;}
    });
    //KeyRelease Events
    document.addEventListener('keyup',function(keyoff){
      if ((keyoff.keyCode == '38') || (keyoff.keyCode == '40')) {keyforward = 0;keybackward = 0;}
      else if ((keyoff.keyCode == '37') || (keyoff.keyCode == '39')) {keyleft = 0;keyright = 0;}
    });
    //Send Commands to Scout
    var currentcommand=0;
    var oldcommand=0;
    window.setInterval(function(){
      if (((keyforward) && (keyleft)) || ((keybackward) && (keyleft)) || (keyleft)) {currentcommand = 3;} // Turn Left
      else if (((keyforward) && (keyright)) || ((keybackward) && (keyright)) || (keyright)) {currentcommand = 4;} // Turn Right
      else if (keyforward) {currentcommand = 1;} //Set Direction Forward
      else if (keybackward) {currentcommand = 2;} // Set Direction Backward
      else {currentcommand = 5;} // Stop
      if (currentcommand != oldcommand){
        fetch(document.location.origin+'/control?var=car&val='+currentcommand);
        oldcommand = currentcommand;}
    }, 100);
    </script>
    </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

// Finally, if all is well with the camera, encoding, and all else, here it is, the actual camera server.
// If it works, use your new camera robot to grab a beer from the fridge using function Request.Fridge("beer","buschlite")
void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

   httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };
    
    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
