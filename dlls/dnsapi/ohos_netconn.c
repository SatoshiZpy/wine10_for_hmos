/*
 * OHOS NetworkKit resolver backend
 *
 * Copyright 2026 Huawei Device Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#ifdef __OHOS__

#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "winbase.h"
#include "windns.h"
#define USE_WS_PREFIX
#include "ws2def.h"
#include "ws2ipdef.h"

#include "wine/debug.h"
#include "wine/unixlib.h"
#include "dnsapi.h"

#include <network/netmanager/net_connection.h>

WINE_DEFAULT_DEBUG_CHANNEL(dnsapi);

struct query_type
{
    WORD type;
    int family;
    WORD len;
};

/* NetConn_NetAddr.family uses the OHOS family type enumeration
 * (FAMILY_TYPE_ALL 0, FAMILY_TYPE_IPV4 1, FAMILY_TYPE_IPV6 2),
 * not the C AF_* constants. */
enum
{
    NETCONN_FAMILY_ALL  = 0,
    NETCONN_FAMILY_IPV4 = 1,
    NETCONN_FAMILY_IPV6 = 2,
};

static const struct query_type query_types[] =
{
    { DNS_TYPE_A,    AF_INET,  sizeof(struct in_addr) },
    { DNS_TYPE_AAAA, AF_INET6, sizeof(struct in6_addr) },
};

static const struct query_type *find_query_type( WORD type )
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(query_types); i++)
        if (query_types[i].type == type) return &query_types[i];
    return NULL;
}

static DNS_STATUS netconn_status( int32_t error )
{
    static const struct
    {
        int32_t error;
        DNS_STATUS status;
    }
    status_map[] =
    {
        { NETCONN_SUCCESS,           ERROR_SUCCESS },
        { NETCONN_PERMISSION_DENIED, DNS_ERROR_RCODE_REFUSED },
        { NETCONN_PARAMETER_ERROR,   ERROR_INVALID_PARAMETER },
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(status_map); i++)
    {
        if (status_map[i].error == error) return status_map[i].status;
    }
    return DNS_ERROR_RCODE_SERVER_FAILURE;
}

static BOOL skip_family( int family, USHORT requested )
{
    return (requested == WS_AF_INET && family != AF_INET) ||
           (requested == WS_AF_INET6 && family != AF_INET6);
}

static DNS_STATUS get_default_properties( NetConn_ConnectionProperties *properties )
{
    NetConn_NetHandle net_handle = {0};
    DNS_STATUS status;
    int ret;

    ret = OH_NetConn_GetDefaultNet( &net_handle );
    if ((status = netconn_status( ret )))
    {
        WARN( "OH_NetConn_GetDefaultNet failed, ret %d status %d\n", ret, status );
        return status;
    }

    memset( properties, 0, sizeof(*properties) );
    ret = OH_NetConn_GetConnectionProperties( &net_handle, properties );
    if ((status = netconn_status( ret )))
    {
        WARN( "OH_NetConn_GetConnectionProperties failed, ret %d status %d\n", ret, status );
        return status;
    }
    return ERROR_SUCCESS;
}

static BOOL netaddr_to_sockaddr( const NetConn_NetAddr *netaddr, struct sockaddr_storage *addr )
{
    struct sockaddr_in *in = (struct sockaddr_in *)addr;
    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;

    if (!memchr( netaddr->address, 0, sizeof(netaddr->address) ))
    {
        WARN( "DNS server address is not terminated: %s\n",
              debugstr_an(netaddr->address, sizeof(netaddr->address)) );
        return FALSE;
    }

    memset( addr, 0, sizeof(*addr) );
    switch (netaddr->family)
    {
    case NETCONN_FAMILY_IPV4:
        if (inet_pton( AF_INET, netaddr->address, &in->sin_addr ) != 1) break;
        in->sin_family = AF_INET;
        return TRUE;
    case NETCONN_FAMILY_IPV6:
        if (inet_pton( AF_INET6, netaddr->address, &in6->sin6_addr ) != 1) break;
        in6->sin6_family = AF_INET6;
        return TRUE;
    default:
        break;
    }
    WARN( "invalid DNS server address %s family %u\n",
          debugstr_a(netaddr->address), netaddr->family );
    return FALSE;
}

static void fill_dns_addr( DNS_ADDR *dst, const struct sockaddr_storage *src )
{
    if (src->ss_family == AF_INET6)
    {
        SOCKADDR_IN6 *sa = (SOCKADDR_IN6 *)dst->MaxSa;
        sa->sin6_family = WS_AF_INET6;
        memcpy( &sa->sin6_addr, &((const struct sockaddr_in6 *)src)->sin6_addr,
                sizeof(sa->sin6_addr) );
        dst->Data.DnsAddrUserDword[0] = sizeof(*sa);
    }
    else
    {
        SOCKADDR_IN *sa = (SOCKADDR_IN *)dst->MaxSa;
        sa->sin_family = WS_AF_INET;
        memcpy( &sa->sin_addr.WS_s_addr, &((const struct sockaddr_in *)src)->sin_addr,
                sizeof(sa->sin_addr.WS_s_addr) );
        dst->Data.DnsAddrUserDword[0] = sizeof(*sa);
    }
}

static NTSTATUS ohos_get_searchlist( void *args )
{
    const struct get_searchlist_params *params = args;
    NetConn_ConnectionProperties properties;
    WCHAR *ptr, *end;
    DNS_STATUS status;
    DWORD needed;
    size_t len = 0;

    if ((status = get_default_properties( &properties ))) return status;

    while (len < sizeof(properties.domain) && properties.domain[len]) len++;
    needed = (len ? len + 2 : 1) * sizeof(WCHAR);

    if (!params->list || *params->len < needed)
    {
        *params->len = needed;
        return !params->list ? ERROR_SUCCESS : ERROR_MORE_DATA;
    }
    *params->len = needed;
    if (!len)
    {
        params->list[0] = 0;
        return ERROR_SUCCESS;
    }

    ptr = params->list;
    end = ptr + needed / sizeof(WCHAR);
    ptr += ntdll_umbstowcs( properties.domain, len, ptr, end - ptr - 1 );
    *ptr++ = 0;
    *ptr = 0;
    TRACE( "returning search list domain %s, len %u\n",
           debugstr_an(properties.domain, len), *params->len );
    return ERROR_SUCCESS;
}

static NTSTATUS ohos_get_serverlist( void *args )
{
    const struct get_serverlist_params *params = args;
    DNS_ADDR_ARRAY *addrs = params->addrs;
    NetConn_ConnectionProperties properties;
    struct sockaddr_storage servers[NETCONN_MAX_ADDR_SIZE], addr;
    unsigned int i, count = 0, limit;
    DNS_STATUS status;
    DWORD needed;

    if ((status = get_default_properties( &properties ))) return status;

    limit = properties.dnsListSize > 0 ? properties.dnsListSize : 0;
    if (limit > ARRAY_SIZE(properties.dnsList)) limit = ARRAY_SIZE(properties.dnsList);
    for (i = 0; i < limit; i++)
    {
        if (!netaddr_to_sockaddr( &properties.dnsList[i], &addr )) continue;
        if (skip_family( addr.ss_family, params->family )) continue;
        servers[count++] = addr;
    }
    if (!count)
    {
        TRACE( "no DNS servers found for family %u\n", params->family );
        return DNS_ERROR_NO_DNS_SERVERS;
    }

    needed = FIELD_OFFSET(DNS_ADDR_ARRAY, AddrArray[count]);
    if (!addrs || *params->len < needed)
    {
        *params->len = needed;
        return !addrs ? ERROR_SUCCESS : ERROR_MORE_DATA;
    }

    memset( addrs, 0, needed );
    addrs->AddrCount = addrs->MaxCount = count;
    *params->len = needed;
    for (i = 0; i < count; i++)
        fill_dns_addr( &addrs->AddrArray[i], &servers[i] );
    TRACE( "returning %u DNS servers, len %u\n", count, *params->len );
    return ERROR_SUCCESS;
}

static NTSTATUS ohos_set_serverlist( void *args )
{
    /* NetworkKit can report DNS servers but does not expose a per-query override. */
    if (args)
    {
        TRACE( "setting DNS server list is not supported by OHOS NetworkKit\n" );
        return DNS_ERROR_RCODE_NOT_IMPLEMENTED;
    }
    return ERROR_SUCCESS;
}

static const void *addrinfo_rdata( const struct addrinfo *ai, const struct query_type *qtype )
{
    if (ai->ai_family != qtype->family) return NULL;
    if (!ai->ai_addr) return NULL;
    if (qtype->type == DNS_TYPE_A && ai->ai_addrlen >= sizeof(struct sockaddr_in))
        return &((const struct sockaddr_in *)ai->ai_addr)->sin_addr;
    if (qtype->type == DNS_TYPE_AAAA && ai->ai_addrlen >= sizeof(struct sockaddr_in6))
        return &((const struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
    return NULL;
}

static size_t wire_name_size( const char *name )
{
    const char *label = name;
    size_t total = 1;

    if (!name || !*name) return 0;
    if (!strcmp( name, "." )) return 1;
    while (*label)
    {
        const char *dot = strchr( label, '.' );
        size_t len;

        if (!dot) dot = label + strlen(label);
        len = dot - label;
        if (!len || len > 63) return 0;
        total += len + 1;
        if (!*dot) break;
        label = dot + 1;
        if (!*label) break;
    }
    return total <= 255 ? total : 0;
}

static BOOL write_bytes( BYTE **pos, BYTE *end, const void *data, size_t len )
{
    if ((size_t)(end - *pos) < len) return FALSE;
    memcpy( *pos, data, len );
    *pos += len;
    return TRUE;
}

static BOOL write_word( BYTE **pos, BYTE *end, WORD value )
{
    WORD net_value = htons( value );
    return write_bytes( pos, end, &net_value, sizeof(net_value) );
}

static BOOL write_dword( BYTE **pos, BYTE *end, DWORD value )
{
    DWORD net_value = htonl( value );
    return write_bytes( pos, end, &net_value, sizeof(net_value) );
}

static BOOL write_name( BYTE **pos, BYTE *end, const char *name )
{
    const char *label = name;

    if (!strcmp( name, "." )) return write_bytes( pos, end, "", 1 );
    while (*label)
    {
        const char *dot = strchr( label, '.' );
        BYTE len;

        if (!dot) dot = label + strlen(label);
        len = dot - label;
        if (!write_bytes( pos, end, &len, sizeof(len) ) ||
            !write_bytes( pos, end, label, len ))
            return FALSE;
        if (!*dot) break;
        label = dot + 1;
        if (!*label) break;
    }
    return write_bytes( pos, end, "", 1 );
}

static DNS_STATUS build_response( const char *name, const struct query_type *qtype,
                                  struct addrinfo *res, void *buf, DWORD *len )
{
    static const BYTE compressed_name[] = { 0xc0, 0x0c };
    DNS_MESSAGE_BUFFER *buffer = buf;
    BYTE *pos = (BYTE *)buffer->MessageBody, *end = (BYTE *)buf + *len;
    struct addrinfo *ai;
    unsigned int count = 0;
    size_t needed, qname_len;

    if (!(qname_len = wire_name_size( name )))
    {
        TRACE( "invalid DNS query name %s\n", debugstr_a(name) );
        return ERROR_INVALID_NAME;
    }
    needed = sizeof(DNS_HEADER) + qname_len + 2 * sizeof(WORD);
    for (ai = res; ai; ai = ai->ai_next)
    {
        if (!addrinfo_rdata( ai, qtype )) continue;
        needed += sizeof(compressed_name) + 2 * sizeof(WORD) + sizeof(DWORD) +
                  sizeof(WORD) + qtype->len;
        count++;
    }
    if (!count)
    {
        TRACE( "no DNS answers for %s type %u\n", debugstr_a(name), qtype->type );
        return DNS_ERROR_RCODE_NAME_ERROR;
    }
    if (*len < needed)
    {
        *len = needed;
        return ERROR_MORE_DATA;
    }

    memset( buf, 0, needed );
    buffer->MessageHead.IsResponse = 1;
    buffer->MessageHead.RecursionDesired = 1;
    buffer->MessageHead.RecursionAvailable = 1;
    buffer->MessageHead.QuestionCount = htons( 1 );
    buffer->MessageHead.AnswerCount = htons( count );
    if (!write_name( &pos, end, name ) || !write_word( &pos, end, qtype->type ) ||
        !write_word( &pos, end, 1 ))
    {
        WARN( "failed to write DNS question for %s type %u\n", debugstr_a(name), qtype->type );
        return DNS_ERROR_BAD_PACKET;
    }

    for (ai = res; ai; ai = ai->ai_next)
    {
        const void *rdata = addrinfo_rdata( ai, qtype );

        if (!rdata) continue;
        if (!write_bytes( &pos, end, compressed_name, sizeof(compressed_name) ) ||
            !write_word( &pos, end, qtype->type ) || !write_word( &pos, end, 1 ) ||
            !write_dword( &pos, end, 1200 ) || !write_word( &pos, end, qtype->len ) ||
            !write_bytes( &pos, end, rdata, qtype->len ))
        {
            WARN( "failed to write DNS response for %s type %u\n",
                  debugstr_a(name), qtype->type );
            return DNS_ERROR_BAD_PACKET;
        }
    }

    *len = pos - (BYTE *)buf;
    return ERROR_SUCCESS;
}

static NTSTATUS ohos_query( void *args )
{
    const struct query_params *params = args;
    const struct query_type *qtype = find_query_type( params->type );
    struct addrinfo hints = {0}, *res = NULL;
    DNS_STATUS status;
    int ret;

    if (!qtype)
    {
        /* OHOS NetworkKit exposes addrinfo DNS only; arbitrary RR queries are unsupported. */
        TRACE( "DNS query type %u for %s is unsupported by OHOS NetworkKit\n",
               params->type, debugstr_a(params->name) );
        return DNS_ERROR_RCODE_NOT_IMPLEMENTED;
    }

    hints.ai_family = qtype->family;
    hints.ai_socktype = SOCK_STREAM;
    ret = OH_NetConn_GetAddrInfo( (char *)params->name, NULL, &hints, &res, 0 );
    if ((status = netconn_status( ret )))
    {
        TRACE( "OH_NetConn_GetAddrInfo failed for %s type %u, ret %d status %d\n",
               debugstr_a(params->name), params->type, ret, status );
        return status;
    }

    status = build_response( params->name, qtype, res, params->buf, params->len );
    if (!status)
        TRACE( "returning DNS response for %s type %u, len %u\n",
               debugstr_a(params->name), params->type, *params->len );
    else if (status != ERROR_MORE_DATA)
        TRACE( "DNS query for %s type %u returned status %d\n",
               debugstr_a(params->name), params->type, status );
    if (res) OH_NetConn_FreeDnsResult( res );
    return status;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    ohos_get_searchlist,
    ohos_get_serverlist,
    ohos_set_serverlist,
    ohos_query,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_funcs) == unix_funcs_count );

#ifdef _WIN64

typedef ULONG PTR32;

static NTSTATUS wow64_ohos_get_searchlist( void *args )
{
    struct
    {
        PTR32 list;
        PTR32 len;
    } const *params32 = args;
    struct get_searchlist_params params = { ULongToPtr(params32->list), ULongToPtr(params32->len) };

    return ohos_get_searchlist( &params );
}

static NTSTATUS wow64_ohos_get_serverlist( void *args )
{
    struct
    {
        USHORT family;
        PTR32 addrs;
        PTR32 len;
    } const *params32 = args;
    struct get_serverlist_params params =
        { params32->family, ULongToPtr(params32->addrs), ULongToPtr(params32->len) };

    return ohos_get_serverlist( &params );
}

static NTSTATUS wow64_ohos_query( void *args )
{
    struct
    {
        PTR32 name;
        WORD  type;
        DWORD options;
        PTR32 buf;
        PTR32 len;
    } const *params32 = args;
    struct query_params params =
        { ULongToPtr(params32->name), params32->type, params32->options,
          ULongToPtr(params32->buf), ULongToPtr(params32->len) };

    return ohos_query( &params );
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    wow64_ohos_get_searchlist,
    wow64_ohos_get_serverlist,
    ohos_set_serverlist,
    wow64_ohos_query,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_wow64_funcs) == unix_funcs_count );

#endif  /* _WIN64 */

#endif /* __OHOS__ */
