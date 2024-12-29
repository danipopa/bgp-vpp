#ifndef __included_bgp_h__
#define __included_bgp_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/ethernet/ethernet.h>
#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <netinet/in.h>  // Required for struct sockaddr_in

// #include <vppinfra/ring.h> // Include VPP's ring implementation
// #include <bgp/bgp_socket.h>
// #include <bgp/bgp_messages.h>
// #include <bgp/bgp_state_machine.h>


// === BGP Route Structure ===
typedef struct {
    ip4_address_t prefix;         // The IP prefix of the route
    u8 mask_length;               // The subnet mask length
    ip4_address_t next_hop;       // The next-hop IP address for the route
    u32 local_pref;               // Local preference (optional)
    u32 as_path_length;           // AS path length (optional)
    char as_path[256];            // AS path string (optional, for debugging)
    u8 origin;                    // Origin attribute (IGP, EGP, incomplete)
    u32 med;                      // Multi-Exit Discriminator (optional)
} bgp_route_t;

typedef struct bgp_message_t {
    uint8_t type;        // BGP message type (e.g., UPDATE, KEEPALIVE)
    uint8_t *data;       // Encoded message data
    uint16_t length;     // Length of the message data
} bgp_message_t;

typedef struct {
    bgp_message_t **buffer; // Array of message pointers
    int head;               // Head index
    int tail;               // Tail index
    int capacity;           // Maximum number of elements
    int count;              // Current number of elements
} custom_queue_t;

// Definition of bgp_socket_t
typedef struct {
    int socket_fd;
    struct sockaddr_in peer_addr;
} bgp_socket_t;

// === BGP Neighbor Structure ===
typedef struct {
    ip4_address_t neighbor_ip;    // Neighbor IP address
    u32 remote_as;                // Remote AS number
    u8 is_route_reflector_client; // Route reflector client flag
    char route_filter_name[64];   // Associated route filter
    u32 hold_timer;               // Hold timer (per neighbor)
    u32 keepalive_timer;          // Keepalive timer (per neighbor)
    u32 state;                    // Current BGP state (BGP_STATE_IDLE, BGP_STATE_CONNECT, etc.)
    custom_queue_t output_queue;    // Ring buffer for outgoing messages
    bgp_socket_t *socket; // Add this field to represent the neighbor's socket

} bgp_neighbor_t;

// === BGP Prefix List and Entries ===
typedef struct {
    ip4_address_t prefix; // Prefix address
    u8 mask_length;       // Mask length
    bool permit;          // Permit/Deny flag
} bgp_prefix_t;

typedef struct {
    char name[64];           // Prefix list name
    bgp_prefix_t **entries;  // Array of prefix entries
} bgp_prefix_list_t;

// === BGP Aggregates ===
typedef struct {
    ip4_address_t prefix;   // Aggregated prefix
    u8 prefix_length;       // Prefix length
    u8 summary_only;        // Summary-only flag
    u8 as_set;              // AS_SET attribute flag
} bgp_aggregate_t;

// === Main BGP Structure ===
typedef struct {
    u16 msg_id_base;                   // Message ID base for API messages
    u8 periodic_timer_enabled;         // Periodic timer status
    u32 periodic_node_index;           // Index of the periodic process

    vlib_main_t *vlib_main;            // VLIB main context
    vnet_main_t *vnet_main;            // VNET main context
    ethernet_main_t *ethernet_main;    // Ethernet main context

    // BGP-specific state
    u32 bgp_router_id;                 // Router ID
    u32 bgp_as_number;                 // Autonomous System (AS) number
    u32 hold_time;                     // Default hold timer
    u32 keepalive_time;                // Default keepalive timer
    u32 cluster_id;                    // Cluster ID for route reflector
    u32 bgp_enabled_interfaces; // Bitmap for enabled interfaces

    clib_spinlock_t lock;              // Spinlock for thread safety
    bgp_neighbor_t *neighbors;         // Pool of BGP neighbors
    bgp_route_t *routes;               // Pool of BGP routes
    bgp_aggregate_t *aggregates;       // Pool of BGP aggregates
    bgp_prefix_list_t **prefix_lists;  // Array of prefix lists
} bgp_main_t;

// === BGP Session States ===
typedef enum {
    BGP_STATE_IDLE = 0,
    BGP_STATE_CONNECT = 1,
    BGP_STATE_ACTIVE = 2,
    BGP_STATE_OPEN_SENT = 3,
    BGP_STATE_OPEN_CONFIRM = 4,
    BGP_STATE_ESTABLISHED = 5
} bgp_state_t;

// === Global BGP Instance ===
extern bgp_main_t bgp_main;

extern vlib_node_registration_t bgp_node;
extern vlib_node_registration_t bgp_periodic_node;

// Periodic function events
#define BGP_EVENT1 1
#define BGP_EVENT2 2
#define BGP_EVENT_PERIODIC_ENABLE_DISABLE 3


void bgp_create_periodic_process(bgp_main_t *);
void bgp_process_message(void *message, size_t length);

// === Function Declarations ===
// bgp_main.c
void bgp_initialize(vlib_main_t *vm);

// bgp_neighbors.c
void bgp_neighbor_init(bgp_neighbor_t *neighbor, ip4_address_t neighbor_ip, u32 remote_as);
void bgp_start_session(bgp_main_t *bmp, ip4_address_t neighbor_ip);
void bgp_stop_session(bgp_main_t *bmp, ip4_address_t neighbor_ip);
void bgp_add_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip, u32 remote_as);
void bgp_remove_neighbor(bgp_main_t *bmp, bgp_neighbor_t *neighbor);
bgp_neighbor_t *bgp_find_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip);
void bgp_reset_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip);
int bgp_enable_disable(bgp_main_t *bmp, u32 sw_if_index, int enable_disable);
void bgp_hard_reset_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip);
void bgp_soft_reset_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip, bool inbound);
void bgp_clear_session_resources(bgp_neighbor_t *neighbor);
void bgp_clear_rib_in_for_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip);
void bgp_clear_rib_out_for_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip);

// bgp_routes.c
void bgp_add_route(bgp_main_t *bmp, ip4_address_t prefix, u8 mask_length, ip4_address_t next_hop);
void bgp_remove_route(bgp_main_t *bmp, ip4_address_t prefix, u8 mask_length);
void bgp_show_routes(bgp_main_t *bmp);
int bgp_advertise_network(bgp_main_t *bmp, ip4_address_t prefix, u8 mask_length);

// bgp_prefix_list.c
bgp_prefix_list_t *bgp_find_or_create_prefix_list(bgp_main_t *bmp, const char *list_name);
void bgp_update_prefix_list(bgp_main_t *bmp, const char *list_name, ip4_address_t *prefix, u8 mask_length, bool permit);
void bgp_free_prefix_lists(bgp_main_t *bmp);

// bgp_aggregates.c
bgp_aggregate_t *bgp_find_or_create_aggregate(bgp_main_t *bmp, ip4_address_t prefix, u8 prefix_length);
void bgp_add_aggregate(bgp_main_t *bmp, ip4_address_t prefix, u8 prefix_length, u8 summary_only, u8 as_set);
void bgp_remove_aggregate(bgp_main_t *bmp, ip4_address_t prefix, u8 prefix_length);

// bgp_utils.c
int ip4_address_cmp(const ip4_address_t *a, const ip4_address_t *b);
int unformat_fib_prefix(unformat_input_t *input, fib_prefix_t *prefix);
const char *bgp_state_to_string(bgp_state_t state);
int bgp_construct_route_update(bgp_main_t *bmp, bgp_neighbor_t *neighbor, u8 **data, u16 *length);
int bgp_enqueue_message(bgp_neighbor_t *neighbor, bgp_message_t *message);
void *safe_mem_alloc(size_t size);
bgp_message_t *bgp_dequeue_message(bgp_neighbor_t *neighbor);
void queue_init(custom_queue_t *queue, int capacity);
int queue_enqueue(custom_queue_t *queue, bgp_message_t *message) ;
bgp_message_t *queue_dequeue(custom_queue_t *queue);
void queue_free(custom_queue_t *queue);
bool queue_is_empty(custom_queue_t *queue);
bool queue_is_full(custom_queue_t *queue);
int bgp_create_open_message(u8 **message, u16 as_number, u32 bgp_identifier);

void bgp_request_full_update(bgp_neighbor_t *neighbor, bool rib_in);
void bgp_recompute_rib_out(bgp_neighbor_t *neighbor);
void bgp_send_open_message(bgp_main_t *bmp, bgp_neighbor_t *neighbor);
void bgp_stop_route_exchange(bgp_main_t *bmp, bgp_neighbor_t *neighbor);
bool bgp_tcp_is_connected(bgp_neighbor_t *neighbor);

bool bgp_received_open(bgp_neighbor_t *neighbor);
bool bgp_received_notification(bgp_neighbor_t *neighbor);
bool bgp_received_keepalive(bgp_neighbor_t *neighbor);

void bgp_send_keepalive_message(bgp_neighbor_t *neighbor);

// bgp_cli.c
// void bgp_show_config(vlib_main_t *vm, bgp_main_t *bmp);
// void bgp_show_summary(vlib_main_t *vm, bgp_main_t *bmp);

// bgp.c
// void bgp_init(vlib_main_t *vm);
// void bgp_exit(vlib_main_t *vm);

//bgp_messages
/**
 * BGP Message Types
 */
typedef enum {
    BGP_MSG_OPEN = 1,
    BGP_MSG_UPDATE = 2,
    BGP_MSG_NOTIFICATION = 3,
    BGP_MSG_KEEPALIVE = 4
} bgp_message_type_t;

/**
 * BGP Message Header
 */
typedef struct {
    u8 marker[16];    // All bits set to 1
    u16 length;       // Total length of the message
    u8 type;          // BGP message type
} bgp_message_header_t;

/**
 * BGP OPEN Message
 */
typedef struct {
    bgp_message_header_t header;
    u8 version;
    u16 my_as;
    u16 hold_time;
    ip4_address_t bgp_identifier;
    u8 opt_param_length;
    u8 optional_parameters[];
} bgp_open_message_t;

/**
 * BGP UPDATE Message
 */
typedef struct {
    bgp_message_header_t header;
    u16 withdrawn_routes_length;
    u8 withdrawn_routes[];
    // Path attributes and NLRI follow
} bgp_update_message_t;

/**
 * BGP KEEPALIVE Message
 */
typedef struct {
    bgp_message_header_t header;
} bgp_keepalive_message_t;

/**
 * BGP NOTIFICATION Message
 */
typedef struct {
    bgp_message_header_t header;
    u8 error_code;
    u8 error_subcode;
    u8 data[];
} bgp_notification_message_t;

//bgp_socket
#define BGP_PORT 179

// // Definition of bgp_socket_t
// typedef struct {
//     int socket_fd;
//     struct sockaddr_in peer_addr;
// } bgp_socket_t;

/* Initialize the BGP socket */
bgp_socket_t *bgp_socket_init(ip4_address_t *peer_ip);

/* Establish a connection to the BGP peer */
int bgp_socket_connect(bgp_socket_t *sock);

/* Send a BGP message */
int bgp_socket_send(bgp_socket_t *sock, void *message, size_t length);

/* Receive a BGP message */
int bgp_socket_receive(bgp_socket_t *sock, void *buffer, size_t buffer_size);

/* Close the BGP socket */
void bgp_socket_close(bgp_socket_t *sock);

//bgp_state_machine
void bgp_handle_route_update(bgp_main_t *bmp, bgp_neighbor_t *neighbor);

/**
 * Handles sending and receiving Keepalive messages for a BGP neighbor.
 */
void bgp_handle_keepalive(bgp_neighbor_t *neighbor);


/* Handle entering a specific state */
void bgp_enter_state(bgp_main_t *bmp, bgp_neighbor_t *neighbor, bgp_state_t new_state);

/* Handle exiting a specific state */
void bgp_exit_state(bgp_main_t *bmp, bgp_neighbor_t *neighbor, bgp_state_t old_state);

/* Transition neighbor to a new state */
void bgp_transition_state(bgp_main_t *bmp, bgp_neighbor_t *neighbor, bgp_state_t new_state);

/* Periodic processing of state transitions */
void bgp_process_state(bgp_main_t *bmp, bgp_neighbor_t *neighbor);

/* Timer events for managing neighbor state */
void bgp_handle_timers(bgp_main_t *bmp, bgp_neighbor_t *neighbor);


#endif /* __INCLUDED_BGP_H__ */