/*
 * Copyright (c) 2006 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "sys_queue.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <ifaddrs.h>

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <event.h>
#include <evhttp.h>
#include <evdns.h>

#include "dstring_url.h"
#include "io.h"
#include "log.h"
#include "util.h"
#include "rx.h"
#include "notifications.h"

static struct {
	const char *host;
	const char *uri;
} lookup_hosts[] = {
	{ "shakespeer.bzero.se", "/ip.shtml" },
	{ "www.stacken.kth.se", "/~mhe/ip.shtml" },
	{ "backup.bzero.se", "/ip.shtml" },
};

/* data passed to the ip lookup response callback */
struct lookup_info
{
	/* Start and current index into the lookup_hosts array.
	 * If a request fails, the next host is tried. When all hosts are
	 * tried (current index is back at start index), we wait a while and
	 * continue looping until success.
	 */
	int start_index;
	int current_index;

	/* timer set when all hosts failed */
	struct event ev;
};

static const int num_lookup_hosts =
	sizeof(lookup_hosts) / sizeof(lookup_hosts[0]);

#define EXTERNAL_IP_TIMEOUT 10*60 /* looked up IP is valid this many seconds */

static char *external_ip = NULL;
static char *static_ip = NULL;
static bool allow_hub_override = true;
static time_t external_ip_lookup_time = 0;
static bool use_static = false; /* true if manually set, disables automatic detection */

static void extip_update_cache(void);
static void extip_response_handler(struct evhttp_request *req, void *data);
static void extip_send_lookup_request(struct lookup_info *info);
static void extip_schedule_update(struct lookup_info *info);

/* Pass ip = NULL to disable static (manual) IP */
void extip_set_static(const char *ip)
{
    free(static_ip);
    static_ip = NULL;

    if(ip == NULL)
    {
        use_static = false;
	if(external_ip)
	    nc_send_external_ip_detected_notification(nc_default(), external_ip);
    }
    else
    {
        struct in_addr tmp;
        if(inet_aton(ip, &tmp) == 0)
        {
            WARNING("invalid IP address [%s], ignored", ip);
            use_static = false;
        }
        else
        {
            static_ip = strdup(ip);
            use_static = true;
	    nc_send_external_ip_detected_notification(nc_default(), static_ip);
        }
    }
}

void extip_set_override(bool allow_override)
{
	allow_hub_override = allow_override;
}

bool extip_get_override()
{
	return allow_hub_override;
}

int get_netmask(struct in_addr *ip, struct in_addr *mask)
{
#if 1
    struct ifaddrs *ifa, *ifaddrs;

    if(getifaddrs(&ifaddrs) == -1)
        return -1;

    for(ifa = ifaddrs; ifa != NULL; ifa = ifa -> ifa_next)
    {
	if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            if(memcmp(ip, &sin->sin_addr, sizeof(struct in_addr)) == 0)
            {
                sin = (struct sockaddr_in *)ifa->ifa_netmask;
                memcpy(mask, &sin->sin_addr, sizeof(struct in_addr));
                break;
            }
        }
    }

    freeifaddrs(ifaddrs);

#else

    /* Open a socket that we can do some ioctl's on. */
    int fd;
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        WARNING("socket: %s", strerror(errno));
        return -1;
    }

    /* Get interface configuration */
    char buf[1024];
    struct ifconf ifc;
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0)
    {
        WARNING("ioctl(SIOCGIFCONF): %s", strerror(errno));
        close(fd);
        return -1;
    }

    void *p = ifc.ifc_req;
    struct ifreq *ifr = p;

    for(; p < (void *)ifc.ifc_req + ifc.ifc_len; p += _SIZEOF_ADDR_IFREQ(*ifr))
    {
        struct sockaddr *sockaddrp;

        ifr = p;

        sockaddrp = &ifr->ifr_addr;
        if(sockaddrp->sa_family == AF_INET)
        {
            struct sockaddr_in *addrp = (struct sockaddr_in *)sockaddrp;

            DEBUG("found IP %s", inet_ntoa(addrp->sin_addr));

            /* does the IP address match? */
            if(memcmp(ip, &addrp->sin_addr, sizeof(struct in_addr)) == 0)
            {
                DEBUG("interface name: %s", ifr->ifr_name);

                if(ioctl(fd, SIOCGIFNETMASK, ifr) < 0)
                    WARNING("ioctl(SIOCGIFNETMASK): %s", strerror(errno));

                memcpy(mask,
                        &((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr,
                        sizeof(struct in_addr));
                DEBUG("network mask: %s", inet_ntoa(*mask));
                break;
            }
        }
        else
            DEBUG("non-INET interface %s, family = %i",
                    ifr->ifr_name, sockaddrp->sa_family);
    }

    close(fd);
#endif
    return 0;
}

static const char *extip_check_local_hub(struct in_addr *local,
        struct in_addr *remote)
{
    /* look up the local netmask */
    struct in_addr mask;
    return_val_if_fail(get_netmask(local, &mask) == 0, NULL);
    INFO("detected my local netmask as %s", inet_ntoa(mask));

    /* Check if we're in the same subnet as the hub. */
    struct in_addr subnet;
    subnet.s_addr = local->s_addr & mask.s_addr;

    if((remote->s_addr & mask.s_addr) == subnet.s_addr)
    {
        INFO("common subnet [%s], using local IP", inet_ntoa(subnet));

        return inet_ntoa(*local);
    }
    else
    {
        DEBUG("hub ip [%s] not in local subnet", inet_ntoa(*remote));
    }

    return NULL;
}

/* Returns true if the address is private according to RFC1918.
 *
 *  10.0.0.0        -   10.255.255.255  (10/8 prefix)
 *  172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
 *  192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
 */
static bool addr_is_private(struct in_addr *ip)
{
    unsigned long int addr = htonl(ip->s_addr);

    if((addr & 0xFFFF0000) == 0xc0A80000)
        return true;
    if((addr & 0xFFF00000) == 0xAC100000)
        return true;
    if((addr & 0xFF000000) == 0x0A000000)
        return true;

    return false;
}

/* returned string should NOT be free'd by caller */
const char *extip_get(int fd, const char *hub_ip)
{
    struct in_addr hub_addr;
    if(!inet_aton(hub_ip, &hub_addr))
    {
        WARNING("invalid hub IP [%s]", hub_ip);
        return NULL;
    }

    return_val_if_fail(fd != -1, NULL);
    return_val_if_fail(hub_ip, NULL);

    /* Get our local IP address for this connection. */
    struct sockaddr_in sin;
    unsigned int namelen = sizeof(sin);
    getsockname(fd, (struct sockaddr *)&sin, &namelen);
    INFO("detected my local IP as %s", inet_ntoa(sin.sin_addr));

    /* Did the user configure an external IP manually? */
    if(use_static && static_ip)
    {
        /* if so, use it */
	INFO("using static IP %s for hub %s", static_ip, hub_ip);
        return static_ip;
    }
	
    /* Check if the hub is local. If so, return the local address. */
    const char *ip = extip_check_local_hub(&sin.sin_addr, &hub_addr);
    if(ip)
        return ip;

    if(addr_is_private(&sin.sin_addr) && addr_is_private(&hub_addr))
    {
        /* Both we and the hub has a private (RFC1918) address. However, we're
         * not on the same subnet. There's no use trying to use the external
         * public IP, it won't work. */
        INFO("using private local IP for private hub %s", hub_ip);
        return inet_ntoa(sin.sin_addr);
    }

    if(external_ip)
    {
        struct in_addr external_addr;
        inet_aton(external_ip, &external_addr);
        if(external_addr.s_addr == sin.sin_addr.s_addr)
        {
            /* Our detected external IP is the same as the local IP. Great, we
             * have a publically visible IP address, no need to detect this
             * again for quite some time. */
	    INFO("detected use of public IP (ie, no NAT/PAT)");
            external_ip_lookup_time += 24*60*60;
        }
    }

    extip_update_cache();

    if(external_ip == NULL)
    {
        WARNING("external IP unavailable, using local IP");
        return inet_ntoa(sin.sin_addr);
    }

    INFO("using external IP %s for hub %s", external_ip, hub_ip);
    return external_ip;
}

void extip_init(void)
{
    srandom(time(0) * getpid());
    extip_update_cache();
}

static char *extip_detect_from_buf(const char *buf)
{
    return_val_if_fail(buf, NULL);

    rx_subs_t *subs = rx_search(buf, "(([0-9]{1,3}\\.){3}[0-9]{1,3})");

    char *ip = NULL;
    if(subs && subs->nsubs == 2)
    {
        ip = strdup(subs->subs[0]);
        DEBUG("detected external IP %s", ip);
    }

    rx_free_subs(subs);

    return ip;
}

static void extip_try_next_host(struct lookup_info *info)
{
	INFO("failed to lookup external IP, trying another host");

	/* Try the next host in the list. */
	info->current_index++;
	info->current_index %= num_lookup_hosts;

	if(info->current_index == info->start_index)
	{
		/* We're back to the first host. All lookup hosts
		 * have failed. Wait a while until we try again.
		 */
		WARNING("All lookup hosts failed, sleeping");
		extip_schedule_update(info);
	}
	else
	{
		/* There is another host to try. Do it directly. */
		extip_send_lookup_request(info);
	}
}

static void extip_resolve_event(int result, char type, int count, int ttl,
	void *addresses, void *user_data)
{
	struct lookup_info *info = user_data;
	return_if_fail(info);

	const char *host = lookup_hosts[info->current_index].host;
	const char *uri = lookup_hosts[info->current_index].uri;

	if(result != DNS_ERR_NONE)
	{
		WARNING("Failed to lookup '%s': %s", host, evdns_err_to_string(result));
		extip_try_next_host(info);
		return;
	}

	INFO("sending lookup request to %s%s", host, uri);

	struct evhttp_request *req =
		evhttp_request_new(extip_response_handler, info);
	return_if_fail(req);

	DEBUG("setting headers for HTTP connection");
	evhttp_add_header(req->output_headers, "Connection", "close");
	evhttp_add_header(req->output_headers, "Host", host);
	evhttp_add_header(req->output_headers,
		"User-Agent", PACKAGE "/" VERSION);

	char ip_address[16];
	inet_ntop(AF_INET, &((char *)addresses)[0], ip_address, sizeof(ip_address));
	DEBUG("creating new connection to [%s] port 80", ip_address);
	struct evhttp_connection *evcon = evhttp_connection_new(ip_address, 80);
	return_if_fail(evcon);

	DEBUG("sending HTTP request");
	if(evhttp_make_request(evcon, req, EVHTTP_REQ_GET, uri) != 0)
		extip_try_next_host(info);
}

static void extip_send_lookup_request(struct lookup_info *info)
{
	const char *host = lookup_hosts[info->current_index].host;

	INFO("resolving [%s]", host);
	int rc = evdns_resolve_ipv4(host, 0, extip_resolve_event, info);
	if(rc != DNS_ERR_NONE)
	{
		WARNING("Failed to lookup '%s': %s", host, evdns_err_to_string(rc));
		extip_try_next_host(info);
	}
}

static void extip_run_update(int fd, short why, void *data)
{
	extip_send_lookup_request(data);
}

/* Set a timer that will send a lookup request.
 */
static void extip_schedule_update(struct lookup_info *info)
{
	if(event_initialized(&info->ev))
		evtimer_del(&info->ev);
	evtimer_set(&info->ev, extip_run_update, info);

	struct timeval tv = {.tv_sec = 30, .tv_usec = 0};
	evtimer_add(&info->ev, &tv);
}

/* Called when we get a response or error from evhttp. */
static void extip_response_handler(struct evhttp_request *req, void *data)
{
	return_if_fail(req);

	struct lookup_info *info = data;

	INFO("got external IP lookup response, code %i", req->response_code);
	DEBUG("response: [%s]", req->response_code_line);

	char *ip = NULL;

	if(req->response_code == 200)
	{
		/* Got a successful response. Parse it. */

		/* FIXME: is evbuffer data nul-terminated? */
		ip = extip_detect_from_buf(
			(const char *)EVBUFFER_DATA(req->input_buffer));
	}

	if(ip == NULL)
	{
		/* Either the parsing failed, or we got an error response. */

		extip_try_next_host(info);
	}
	else
	{
		free(external_ip);
		external_ip = ip;
		external_ip_lookup_time = time(0);
		nc_send_external_ip_detected_notification(nc_default(), ip);

		/* We're done with the lookup info struct. Free it. */
		free(info);
	}

	/* FIXME: shouldn't we, like, free something here? */
	/* evhttp_request_free(req); */
}

static void extip_update_cache(void)
{
	time_t now = time(0);

	if(external_ip &&
	   external_ip_lookup_time + EXTERNAL_IP_TIMEOUT >= now)
	{
		/* cached IP up-to-date */
		return;
	}

	if(external_ip)
	{
		DEBUG("external IP [%s] has timed out after %i seconds",
			external_ip, EXTERNAL_IP_TIMEOUT);
	}

	DEBUG("scheduling external IP lookup request");

	/* randomly select one lookup host to use
	 */
	struct lookup_info *info = calloc(1, sizeof(struct lookup_info));
	info->start_index = random() % num_lookup_hosts;
	info->current_index = info->start_index;

	extip_send_lookup_request(info);
}

#ifdef TEST

#include "unit_test.h"

static bool address_is_private(const char *address)
{
    struct in_addr inp;

    if(inet_aton(address, &inp) == 0)
    {
        WARNING("invalid IPv4 address: [%s]", address);
        return true;
    }

    return addr_is_private(&inp);
}

int main(void)
{
    sp_log_set_level("debug");

    /*
     * address_is_private
     */
    fail_unless( address_is_private("1.2.3.4") == false );
    fail_unless( address_is_private("10.2.3.4") == true );
    fail_unless( address_is_private("192.169.0.1") == false );
    fail_unless( address_is_private("192.168.0.1") == true );
    fail_unless( address_is_private("192.167.192.168") == false );
    fail_unless( address_is_private("172.16.192.168") == true );
    fail_unless( address_is_private("172.31.192.168") == true );
    fail_unless( address_is_private("171.31.0.0") == false );
    fail_unless( address_is_private("172.32.192.168") == false );
    fail_unless( address_is_private("172.15.255.255") == false );
    fail_unless( address_is_private("172.15.255.255") == false );
    fail_unless( address_is_private("172.22.255.1") == true );

    char *ip = extip_detect_from_buf("192.0.34.166\n");
    fail_unless(ip);
    fail_unless(strcmp(ip, "192.0.34.166") == 0);
    free(ip);

    ip = extip_detect_from_buf("431.123.1567.1"
            "<external-ip>192.0.34.166</external-ip>");
    fail_unless(ip);
    fail_unless(strcmp(ip, "192.0.34.166") == 0);
    free(ip);

    fail_unless(extip_detect_from_buf("no ip address here") == NULL);

    struct in_addr local;
    fail_unless(inet_aton("127.0.0.1", &local) == 1);

    struct in_addr mask;
    memset(&mask, 0, sizeof(mask));
    get_netmask(&local, &mask);
    printf("mask = %s\n", inet_ntoa(mask));

    struct in_addr local_mask;
    fail_unless(inet_aton("255.0.0.0", &local_mask) == 1);

    fail_unless(mask.s_addr == local_mask.s_addr);

    struct in_addr remote;
    fail_unless(inet_aton("127.0.2.3", &remote) == 1);

    ip = (char *)extip_check_local_hub(&local, &remote);
    fail_unless(ip);
    fail_unless(strcmp(ip, "127.0.0.1") == 0);

    remote.s_addr = 0x0A000001; /* 10.0.0.1 */
    ip = (char *)extip_check_local_hub(&local, &remote);
    fail_unless(ip == NULL);

    return 0;
}

#endif

