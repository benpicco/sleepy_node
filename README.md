### Border Router Setup

Just run the RIOT [gnrc_border_router](https://github.com/RIOT-OS/RIOT/tree/master/examples/gnrc_border_router) example.
I used the SLIP uplink but it shouldn't make a difference. (Ethos might use a different IP for the host theough)

    make UPLINK=slip flash term

### CoAP Server Setup

The client will connect to the (ASCII) `/time` endpoint of the CoAP server.

For this I used the FreeCoAP [time_server](https://github.com/keith-cullen/FreeCoAP/tree/tinydtls/sample/time_server) example. <br>
Make sure to compile with `ip6=y dtls=n` to enable IPv6 and disable DTLS.

The server is then started with

    ./time_server "fdea:dbee:f::1" 5683

`fdea:dbee:f::1` was configured as an IP for the host computer by the SLIP startup script.

