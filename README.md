# GOWS - Get only Web Server

GOWS is a simple program to provide GET only HTTP functionality. It is designed
for small, static web pages.

It does not follow the HTTP standard as of this time however it does function to
server HTTP content.

The program must be run with SUDO privileges if using the default port 80.
The default server root is "/srv/http/" the server will send files from this
root.

The server does contain anti-LFI features by preventing the use of the "../"
directory.

Only a single client request can be processed at once.

Because the program is not following the HTTP Standard to a tee, I have added
fun elements such as returning error 418 (I'm a teapot) for attempted LFI
requests in addition to returning the same response when a non-GET request is
received.

`$ sudo ./GOWS`

_ NOTE _
After playing around with this I have recognised that there will be errors
thrown if there is an attempt to put parameters after the filename in a get
request. This will ofcourse break some things however this is designed for
static sites, so please don't try to use it for things it wasn't written for
(until it is).

You can however make references to elements in links such as:
`localhost/index.html#AboutElement`
as these are handled by the browser and not the HTTP server.
