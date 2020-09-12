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
