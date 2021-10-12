# generates a C file containing the contents of the 'serve' folder
import os

FILE_TEMPLATE = """
static const char PROGMEM {varname}[] = R"rawliteral(
{contents}
)rawliteral";

"""

HANDLER_TEMPLATE = """
static esp_err_t {handlername}(httpd_req_t *req){{
    httpd_resp_set_type(req, "{mimetype}");
    return httpd_resp_send(req, (const char *){varname}, strlen({varname}));
}}

"""


input_folder = 'serve'


out = """// GENERATED FILE. DO NOT EDIT
// INSTEAD RUN THE SCIPT ./generate_pages.py

#include "Arduino.h"
#include "esp_http_server.h"

"""


varnames = []
for filename in os.listdir(input_folder):
    if filename.endswith('.jpg') or filename == "drive":
        continue
    filepath = os.path.join(input_folder, filename)
    contents = open(filepath).read()
    varname = filename.replace(".", "_").upper()
    out += FILE_TEMPLATE.format(varname=varname, contents=contents)
    varnames.append((varname, filename))


handlers = []
for (varname, filename) in varnames:
    handlername = varname.lower() + "_handler"
    mimetype = {
        'html': 'text/html',
        'js': 'text/javascript',
        'css': 'text/css',
    }[filename.split('.')[-1]]
    
    handlers.append((handlername, filename))
    
    out += HANDLER_TEMPLATE.format(
        handlername=handlername,
        mimetype=mimetype,
        varname=varname
    )

out += """void registerStaticPages(httpd_handle_t httpd){"""


for (handlername, filename) in handlers:
    out += """
    httpd_uri_t {handler}_uri = {{
        .uri       = "/{filename}",
        .method    = HTTP_GET,
        .handler   = {handler},
        .user_ctx  = NULL
    }};
    httpd_register_uri_handler(httpd, &{handler}_uri);
    
    """.format(handler=handlername, filename=filename)
 
 
# Create root "/" handler as a ref to "index.html" handler
out += """
    httpd_uri_t roothandler_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_html_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(httpd, &roothandler_uri);
"""
        

out += """}
"""
    
    
    

    
open("pages.cpp", "w").write(out)
