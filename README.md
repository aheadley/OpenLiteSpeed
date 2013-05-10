## OpenLiteSpeed

OpenLiteSpeed is a high-performance, lightweight, open source HTTP server
developed and copyrighted by LiteSpeed Technologies, Inc. Users are free to
download, use, distribute, and modify OpenLiteSpeed and its source code in
accordance with the precepts of the [GPL version 3 license](http://www.gnu.org/licenses/gpl-3.0.html). If you're interested
in getting involved, we encourage you to join the [OpenLiteSpeed Development
Group](https://groups.google.com/forum/#!forum/openlitespeed-development) on Google Groups.

### Main Features

    * Minimal memory footprint.
    * WebAdmin GUI with real-time statistics.
    * Apache compatible rewrite engine.
    * Worker processes for scalability. Ability to bind certain processes to
      particular workers.
    * High-performance event-driven architecture using kqueue (FreeBSD and OS
      X), epoll (Linux), /dev/poll (Solaris), and poll.
    * Can handle thousands of concurrent connections.
    * Easy virtual host configuration via templates.

### External Applications Support

    * Supports PHP, Ruby, Python, Perl, and Java external applications.
    * LSAPI, a LiteSpeed-native SAPI, greatly improves PHP and Ruby speeds.
    * Delegates external applications to separate processes, increasing
      efficiency.
    * Buffers requests and responses to external applications to more
      efficiently serve multiple connections.
    * Efficient CGI daemon.
    * Compatibility with third party PHP accelerators.

### Security Features

    * Apache compatible SSI support.
    * SSL support and hardware acceleration.
    * Bandwidth and connection throttling.
    * IP-based access control.
    * HTTP basic authentication.
    * Referer limiting.
    * Response rate limiting.
    * Buffer overrun guards.

### Stability Features

    * Graceful restart feature allows application of configuration changes and
      upgrades without server downtime.
    * Fault tolerance and instant restarting.
    * Runs completely in user space. OS reliability is not affected.

### Basic Features

    * Accept-filters and TCP_DEFER_ACCEPT support.
    * sendfile() support.
    * Pipelined requests.
    * Gzip compression for reduced bandwidth use.
    * Chunked transfer encoding.
    * Keep-alive connections.
    * IPv4 and IPv6.
    * Entity tags.
    * Multi-range requests.
    * Exact/prefix/regex-based matching.
    * Name-based and IP-based virtual hosting.
    * Custom error pages.
    * Autoindexing.
    * MP4 and F4V streaming.
    * IP geolocation.
    * Can send logging to logging server.
    * Simple load balancing.
    * XML or flat file configuration.
    * Can serve as a reverse proxy to accelerate static content delivery,
      compress throughput, or run security.

### Supported OSs

    * CentOS 5 and 6
    * Ubuntu 8.04 and up
    * Debian 4 and up
    * OS X 10.3 and up
    * FreeBSD 4.5 and up

### Downloads

[OpenLiteSpeed_1.0](http://open.litespeedtech.com/packages/openlitespeed.1.0.tgz)

### Documentation

[Quick install guide](http://open.litespeedtech.com/quick_install_guide.html)
[LiteSpeed beginner tutorials](http://www.usefuljaja.com/litespeed)

### Support

[OpenLiteSpeed Development Group](https://groups.google.com/forum/#!forum/openlitespeed-development) on Google Groups

### News

[OpenLiteSpeed 1.0 press release](http://www.litespeedtech.com/latest/announcing-the-release-of-openlitespeed.html)

### Licensing

Users willing to abide by the GPLv3 are welcome to download, use, distribute,
and modify OpenLiteSpeed and its source code. Companies or individuals who wish
to use OpenLiteSpeed in a proprietary product can [contact](http://www.litespeedtech.com/about-litespeed-technologies-inc.html) LiteSpeed
Technologies about obtaining an OEM license.
LiteSpeed Enterprise Edition provides some features above and beyond
OpenLiteSpeed: hosting control panel compatibility, .htaccess file
compatibility, mod_security compatibility, and page caching. Users can find
more about Enterprise Edition on the [LiteSpeed Technologies homepage](http://www.litespeedtech.com/).
