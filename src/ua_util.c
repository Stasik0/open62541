/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 *
 *    Copyright 2014, 2017 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2014 (c) Florian Palm
 *    Copyright 2017 (c) Stefan Profanter, fortiss GmbH
 */

#include <open62541/types_generated_handling.h>
#include <open62541/util.h>

#include "ua_util_internal.h"
#include "base64.h"

size_t
UA_readNumberWithBase(const UA_Byte *buf, size_t buflen, UA_UInt32 *number, UA_Byte base) {
    UA_assert(buf);
    UA_assert(number);
    u32 n = 0;
    size_t progress = 0;
    /* read numbers until the end or a non-number character appears */
    while(progress < buflen) {
        u8 c = buf[progress];
        if(c >= '0' && c <= '9' && c <= '0' + (base-1))
           n = (n * base) + c - '0';
        else if(base > 9 && c >= 'a' && c <= 'z' && c <= 'a' + (base-11))
           n = (n * base) + c-'a' + 10;
        else if(base > 9 && c >= 'A' && c <= 'Z' && c <= 'A' + (base-11))
           n = (n * base) + c-'A' + 10;
        else
           break;
        ++progress;
    }
    *number = n;
    return progress;
}

size_t
UA_readNumber(const UA_Byte *buf, size_t buflen, UA_UInt32 *number) {
    return UA_readNumberWithBase(buf, buflen, number, 10);
}

UA_StatusCode
UA_parseEndpointUrl(const UA_String *endpointUrl, UA_String *outHostname,
                    u16 *outPort, UA_String *outPath) {
    UA_Boolean ipv6 = false;

    /* Url must begin with "opc.tcp://" or opc.udp:// (if pubsub enabled) */
    if(endpointUrl->length < 11) {
        return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
    }
    if (strncmp((char*)endpointUrl->data, "opc.tcp://", 10) != 0) {
#ifdef UA_ENABLE_PUBSUB
        if (strncmp((char*)endpointUrl->data, "opc.udp://", 10) != 0 &&
                strncmp((char*)endpointUrl->data, "opc.mqtt://", 11) != 0) {
            return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
        }
#else
        return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
#endif
    }

    /* Where does the hostname end? */
    size_t curr = 10;
    if(endpointUrl->data[curr] == '[') {
        /* IPv6: opc.tcp://[2001:0db8:85a3::8a2e:0370:7334]:1234/path */
        for(; curr < endpointUrl->length; ++curr) {
            if(endpointUrl->data[curr] == ']')
                break;
        }
        if(curr == endpointUrl->length)
            return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
        curr++;
        ipv6 = true;
    } else {
        /* IPv4 or hostname: opc.tcp://something.something:1234/path */
        for(; curr < endpointUrl->length; ++curr) {
            if(endpointUrl->data[curr] == ':' || endpointUrl->data[curr] == '/')
                break;
        }
    }

    /* Set the hostname */
    if(ipv6) {
        /* Skip the ipv6 '[]' container for getaddrinfo() later */
        outHostname->data = &endpointUrl->data[11];
        outHostname->length = curr - 12;
    } else {
        outHostname->data = &endpointUrl->data[10];
        outHostname->length = curr - 10;
    }
    if(curr == endpointUrl->length)
        return UA_STATUSCODE_GOOD;

    /* Set the port */
    if(endpointUrl->data[curr] == ':') {
        if(++curr == endpointUrl->length)
            return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
        u32 largeNum;
        size_t progress = UA_readNumber(&endpointUrl->data[curr], endpointUrl->length - curr, &largeNum);
        if(progress == 0 || largeNum > 65535)
            return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
        /* Test if the end of a valid port was reached */
        curr += progress;
        if(curr == endpointUrl->length || endpointUrl->data[curr] == '/')
            *outPort = (u16)largeNum;
        if(curr == endpointUrl->length)
            return UA_STATUSCODE_GOOD;
    }

    /* Set the path */
    UA_assert(curr < endpointUrl->length);
    if(endpointUrl->data[curr] != '/')
        return UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
    if(++curr == endpointUrl->length)
        return UA_STATUSCODE_GOOD;
    outPath->data = &endpointUrl->data[curr];
    outPath->length = endpointUrl->length - curr;

    /* Remove trailing slash from the path */
    if(endpointUrl->data[endpointUrl->length - 1] == '/')
        outPath->length--;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_parseEndpointUrlEthernet(const UA_String *endpointUrl, UA_String *target,
                            UA_UInt16 *vid, UA_Byte *pcp) {
    /* Url must begin with "opc.eth://" */
    if(endpointUrl->length < 11) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    if(strncmp((char*) endpointUrl->data, "opc.eth://", 10) != 0) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    /* Where does the host address end? */
    size_t curr = 10;
    for(; curr < endpointUrl->length; ++curr) {
        if(endpointUrl->data[curr] == ':') {
           break;
        }
    }

    /* set host address */
    target->data = &endpointUrl->data[10];
    target->length = curr - 10;
    if(curr == endpointUrl->length) {
        return UA_STATUSCODE_GOOD;
    }

    /* Set VLAN */
    u32 value = 0;
    curr++;  /* skip ':' */
    size_t progress = UA_readNumber(&endpointUrl->data[curr],
                                    endpointUrl->length - curr, &value);
    if(progress == 0 || value > 4096) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    curr += progress;
    if(curr == endpointUrl->length || endpointUrl->data[curr] == '.') {
        *vid = (UA_UInt16) value;
    }
    if(curr == endpointUrl->length) {
        return UA_STATUSCODE_GOOD;
    }

    /* Set priority */
    if(endpointUrl->data[curr] != '.') {
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    curr++;  /* skip '.' */
    progress = UA_readNumber(&endpointUrl->data[curr],
                             endpointUrl->length - curr, &value);
    if(progress == 0 || value > 7) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    curr += progress;
    if(curr != endpointUrl->length) {
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    *pcp = (UA_Byte) value;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_ByteString_toBase64(const UA_ByteString *byteString,
                       UA_String *str) {
    UA_String_init(str);
    if(!byteString || !byteString->data)
        return UA_STATUSCODE_GOOD;

    str->data = (UA_Byte*)
        UA_base64(byteString->data, byteString->length, &str->length);
    if(!str->data)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode UA_EXPORT
UA_ByteString_fromBase64(UA_ByteString *bs,
                         const UA_String *input) {
    UA_ByteString_init(bs);
    if(input->length == 0)
        return UA_STATUSCODE_GOOD;
    bs->data = UA_unbase64((const unsigned char*)input->data,
                           input->length, &bs->length);
    /* TODO: Differentiate between encoding and memory errors */
    if(!bs->data)
        return UA_STATUSCODE_BADINTERNALERROR;
    return UA_STATUSCODE_GOOD;
}

/* Config Parameters */

UA_StatusCode
UA_ConfigParameter_setParameter(UA_ConfigParameter **cp, const char *name,
                                const UA_Variant *parameter) {
    /* Parameter exists already */
    const UA_Variant *param = UA_ConfigParameter_getParameter(*cp, name);
    if(param) {
        UA_Variant copyV;
        UA_StatusCode res = UA_Variant_copy(parameter, &copyV);
        if(res != UA_STATUSCODE_GOOD)
            return res;
        UA_Variant_clear(&(*cp)->param);
        (*cp)->param = copyV;
        return UA_STATUSCODE_GOOD;
    }

    /* Create a new entry */
    UA_ConfigParameter *cp2 = (UA_ConfigParameter*)
        UA_malloc(sizeof(UA_ConfigParameter) + strlen(name) + 1);
    if(!cp2)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    strcpy(cp2->name, name);
    UA_StatusCode res = UA_Variant_copy(parameter, &cp2->param);
    if(res != UA_STATUSCODE_GOOD) {
        UA_free(cp2);
        return res;
    }

    /* Add to linked list */
    cp2->next = *cp;
    *cp = cp2;
    return UA_STATUSCODE_GOOD;
}

const UA_Variant *
UA_ConfigParameter_getParameter(UA_ConfigParameter *cp, const char *name) {
    while(cp) {
        if(strcmp(name, cp->name) == 0)
            return &cp->param;
        cp = cp->next;
    }
    return NULL;
}

/* Returns NULL if the parameter is not defined or not of the right datatype */
const UA_Variant *
UA_ConfigParameter_getScalarParameter(UA_ConfigParameter *cp, const char *name,
                                      const UA_DataType *type) {
    const UA_Variant *v = UA_ConfigParameter_getParameter(cp, name);
    if(!v || !UA_Variant_hasScalarType(v, type))
        return NULL;
    return v;
}

const UA_Variant *
UA_ConfigParameter_getArrayParameter(UA_ConfigParameter *cp, const char *name,
                                     const UA_DataType *type) {
    const UA_Variant *v = UA_ConfigParameter_getParameter(cp, name);
    if(!v || !UA_Variant_hasArrayType(v, type))
        return NULL;
    return v;
}

void
UA_ConfigParameter_deleteParameter(UA_ConfigParameter **cp,
                                   const char *name) {
    UA_ConfigParameter **prevPtr = cp; /* the pointer to current from the previous */
    UA_ConfigParameter *current = *cp;
    while(current) {
        if(strcmp(name, current->name) == 0) {
            UA_Variant_clear(&current->param);
            *prevPtr = current->next;
            UA_free(current);
            return;
        }
        prevPtr = &current->next;
        current = current->next;
    }
}

void
UA_ConfigParameter_delete(UA_ConfigParameter **cp) {
    UA_ConfigParameter *cp2 = *cp;
    while(cp2) {
        UA_ConfigParameter *cp3 = cp2->next;;
        UA_Variant_clear(&cp2->param);
        UA_free(cp2);
        cp2 = cp3;
    }
    *cp = NULL;
}
