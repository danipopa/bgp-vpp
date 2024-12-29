#include <vnet/vnet.h>
#include <bgp/bgp.h>

// #include <bgp/bgp_messages.h>

/* Create a BGP KEEPALIVE message */
void *bgp_create_keepalive_message(size_t *out_length) {
    bgp_keepalive_message_t *msg = clib_mem_alloc(sizeof(bgp_keepalive_message_t));
    memset(msg, 0, sizeof(*msg));
    memset(msg->header.marker, 0xFF, 16);  // Set marker
    msg->header.length = clib_host_to_net_u16(sizeof(*msg));
    msg->header.type = BGP_MSG_KEEPALIVE;

    *out_length = sizeof(*msg);
    return msg;
}

/* Parse a BGP message */
bgp_message_type_t bgp_parse_message(void *data, size_t length) {
    if (length < sizeof(bgp_message_header_t)) {
        clib_warning("Invalid BGP message length");
        return -1;
    }

    bgp_message_header_t *header = (bgp_message_header_t *)data;
    if (header->type < BGP_MSG_OPEN || header->type > BGP_MSG_KEEPALIVE) {
        clib_warning("Unknown BGP message type: %d", header->type);
        return -1;
    }

    return header->type;
}
