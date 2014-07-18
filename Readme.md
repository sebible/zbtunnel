ZbTunnel
========

An enhanced, portable and efficient tunneling tool and library. Similar to stunnel or proxytunnel.

*Current Version*: 1.0

License
-------

zbcoder.cpp under GPL, md5.* under its own license, others under MIT

Features
--------

* **Multi-protocol**: http, https (with openssl), shadowsocks, socks5
* **Proxy-chain**: tunnel through any layers of proxies with any supported protocols in any order
* **Reusable-connection**: outgoing tunnel connections can be pre-spawned, reused and recycled which dramatically improves the connecting time
* **Portable**: compile and run on all major platforms. Small in footprint
* **Efficient**: asynchronous implementation
* **Multi-tunnel**: setup multiple tunnels in one config file
* **Dev-friendly**: both executable and library provided

Compile
-------

**Requirements**: 

* cmake>=2.6
* boost>=1.47.0
* openssl (optional)

To build, type these in the commandline.

    mkdir build && cd build
    cmake ../src
    cmake --build . 

OpenSSL is linked by default. To disable it:

    cmake -D WITH_OPENSSL:BOOL=NO ../src

On posix systems, epoll is enabled by default. In order to use files as standard input like `./zbtunnel tunnel.conf < filename.data`, you have to disable it:

    cmake -D DISABLE_EPOLL:BOOL=YES ../src

Debug version will be built by default. To build other configuration types, type

    cmake -D CMAKE_BUILD_TYPE:STRING=<Release|Debug|MinSizeRel|RelWithDebInfo> --build .

**For Windows Visual Studio**

You might need to edit `src/CMakeList.txt` to set your boost and openssl path
before build. (note: boost and openssl has to be precompiled)

Usage
-----

ZbTunnel takes only one required argument and one optional switch:

    zbtunnel [-] <config_file>

**"-" switch**: When the `-` switch is present, zbtunnel will print log messages to stderr instead of stdout. This should be used when you want to create a tunnel for standard io pipes.

Config
------

The config file is a simple json dictionary file containing one or more of following named dictionaries:

* **global**: global settings can include the following:
  - log_level: int, 0 for Debug, 1 for Info, 2 for Warn, 3 for None. Default is 1
  - log_filter: int, See zbconfig.h.
  - preconnect: int, Spawn x additional outoing tunnel connections for reuse when an incoming connection is accepted. Default is 0.
  - max_reuse: int, Maximum count of reusable connections should be kept. Default is 10.
  - recycle: bool, If a tunnel connection is established and the incoming end breaks, keep the outgoing tunnel for reuse. Default is false

* **any string**: a named tunnel should include an array of hop dictionaries. Every hop should include at least the following:
  - transport: string, http|https|shadow for Shadowsocks|socks5|raw
  - host: string
  - port: int
  
  If authentication is required, you should specify:
  - username: string
  - password: string
  
  And in the first hop, you should specify the following tunnel settings:
  - local_address: string, The tunnel should listen on this local address. Default is 0.0.0.0
  - local_port: int, Listen on this local port. Default is 8080
  - preconnect (optional): int, To override the global preconnect settings for this tunnel
  - max_reuse (optional): int, To override global max_reuse settings for this tunnel
  
  For shadow transport (shadowsocks):
  - key: the key
  - method (optional): currently only table encoding is supported. so it must be ""
  
  For https transport:
  - ssl_type: sslv23|tls1

* **-**: a tunnel named "-" will be a stdio tunnel. All tunnel settings are ignored

Config Exmamples
----------------

**To accept and proxy local connections (like stunnel client)**:

    {
        // Global settings 
        "global": {
            "log_level": 0, 
            "log_filter": 255
        },

        // An item named anything other than "global" will be treated as a new tunnel chain.
        // This defines a single-hop tunnel
        "tunnel-simple": [
            {
                "transport": "https", 
                "host": "somehost.com",
                "port": "443", 
                "local_host": "127.0.0.1", 
                "local_port": "8081"
            }
        ],

        // This defines a three-hop tunnel to a proxy on somehost.com:8080 for web browsing.
        "tunnel-through-lan": [

            // First hop
            {
                "transport": "https",
                "host": "proxy.company.com",
                "port": "443",
                "local_port": "8080",
				"preconnect": 3 // preconnect can dramatically improve connecting time for browsing
            },

            // Second hop
            {
                "transport": "shadow",
                "key": "key_is_required",
                "host": "somehost.com",
                "port": 10002,
            },

            // Third hop
			// Since zbtunnel is not a proxy and shadowsocks server is not a standard proxy server,
			// we need to connect to a standard http proxy server on localhost (in this case somehost.com)
			// to proxy our browser requests.
            {
                "transport": "raw", 
                "host": "localhost",
                "port": 8080
            }
        ]
    }

**To proxy standard io (like proxytunnel)**,

    {
        // A tunnel named "-" will be an IO tunnel connecting the proxy chain and the standard io pipes. 
        // The IO tunnel is exclusive, so no more tunnel configs will be effective in this config.
        "-": [
            {
                "transport": "https",
                "host": "myproxy.com",
                "port": 443
            },
            {
                "transport": "raw",
                "host": "github.com",
                "port": "22"                
        ]
    }
