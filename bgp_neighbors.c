#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/plugin/plugin.h>
#include <vlibmemory/api.h>
#include <bgp/bgp.h>
#include <vpp/app/version.h>

#define REPLY_MSG_ID_BASE bmp->msg_id_base
#include <vlibapi/api_helper_macros.h>

// #include <bgp/bgp_messages.h>
// #include <bgp/bgp_socket.h>

// Global BGP instance
// bgp_main_t bgp_main;

// // === Utility Function Definitions ===
// int ip4_address_cmp(const ip4_address_t *a, const ip4_address_t *b) {
//     return memcmp(a, b, sizeof(ip4_address_t));
// }

// // === Initialization Function ===
// static clib_error_t *bgp_init(vlib_main_t *vm) {
//     bgp_main_t *bmp = &bgp_main;

//     bmp->vlib_main = vm;
//     bmp->vnet_main = vnet_get_main();
//     bmp->bgp_router_id = 0;        // Default router ID
//     bmp->bgp_as_number = 0;        // Default AS number
//     bmp->hold_time = 180;          // Default hold timer
//     bmp->keepalive_time = 60;      // Default keepalive timer
//     bmp->prefix_lists = NULL;      // Initialize prefix lists
//     bmp->routes = NULL;            // Initialize routes pool
//     bmp->aggregates = NULL;        // Initialize aggregates pool
//     bmp->neighbors = NULL;         // Initialize neighbors pool

//     clib_spinlock_init(&bmp->lock);

//     clib_warning("BGP plugin initialized.");
//     return 0;
// }

// VLIB_INIT_FUNCTION(bgp_init);

// // === CLI Commands Registration ===
// #include <bgp/bgp_cli.c>

// // === Plugin Registration ===
// VLIB_PLUGIN_REGISTER() = {
//     .version = VPP_BUILD_VER,
//     .description = "BGP Plugin",
// };

// // === Cleanup Function ===
// static clib_error_t *bgp_exit(vlib_main_t *vm) {
//     bgp_main_t *bmp = &bgp_main;

//     clib_spinlock_lock(&bmp->lock);

//     // Free resources
//     bgp_free_prefix_lists(bmp);     // Free prefix lists
//     pool_free(bmp->routes);         // Free routes pool
//     pool_free(bmp->aggregates);     // Free aggregates pool
//     pool_free(bmp->neighbors);      // Free neighbors pool

//     clib_spinlock_unlock(&bmp->lock);
//     clib_spinlock_free(&bmp->lock);

//     clib_warning("BGP plugin cleaned up.");
//     return 0;
// }

// VLIB_MAIN_LOOP_EXIT_FUNCTION(bgp_exit);

/**
 * Initialize a BGP neighbor and its resources.
 */
void bgp_neighbor_init(bgp_neighbor_t *neighbor, ip4_address_t neighbor_ip, u32 remote_as) {
    memset(neighbor, 0, sizeof(bgp_neighbor_t));
    neighbor->neighbor_ip = neighbor_ip;
    neighbor->remote_as = remote_as;
    neighbor->state = BGP_STATE_IDLE;

    queue_init(&neighbor->output_queue, 16); // Initialize the queue with capacity 16

    clib_warning("Initialized BGP neighbor: %U with remote AS %u", 
                 format_ip4_address, &neighbor_ip, remote_as);
}

/* Start a BGP session with a neighbor */
void bgp_start_session(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    bgp_neighbor_t *neighbor = bgp_find_neighbor(bmp, neighbor_ip);
    if (!neighbor) {
        clib_warning("Neighbor %U not found, cannot start session.", format_ip4_address, &neighbor_ip);
        return;
    }

    // Transition to Connect state
    neighbor->state = BGP_STATE_CONNECT;
    clib_warning("Started BGP session with neighbor: %U.", format_ip4_address, &neighbor_ip);

    // Simulate sending an Open message
    u8 *open_message;
    bgp_create_open_message(&open_message, bmp->bgp_as_number, bmp->bgp_router_id);
    bgp_socket_send(neighbor->socket, open_message, vec_len(open_message));
}

/* Stop a BGP session with a neighbor */
void bgp_stop_session(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    bgp_neighbor_t *neighbor = bgp_find_neighbor(bmp, neighbor_ip);
    if (!neighbor) {
        clib_warning("Neighbor %U not found, cannot stop session.", format_ip4_address, &neighbor_ip);
        return;
    }

    // Transition to Idle state
    neighbor->state = BGP_STATE_IDLE;
    clib_warning("Stopped BGP session with neighbor: %U.", format_ip4_address, &neighbor_ip);

    // Clear session-specific resources
    bgp_clear_session_resources(neighbor);
}

/* Add a new BGP neighbor */
void bgp_add_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip, u32 remote_as) {
    clib_spinlock_lock(&bmp->lock);

    bgp_neighbor_t *neighbor;
    pool_get_zero(bmp->neighbors, neighbor);

    neighbor->socket = bgp_socket_init(&neighbor_ip);
    neighbor->neighbor_ip = neighbor_ip;
    neighbor->remote_as = remote_as;
    neighbor->state = BGP_STATE_IDLE;

    if (!neighbor->socket) {
        clib_warning("Failed to initialize socket for neighbor %U", format_ip4_address, &neighbor_ip);
    }

    clib_warning("Added BGP neighbor: %U (AS %u)", format_ip4_address, &neighbor_ip, remote_as);

    clib_spinlock_unlock(&bmp->lock);
}

void bgp_remove_neighbor(bgp_main_t *bmp, bgp_neighbor_t *neighbor) {
    bgp_clear_session_resources(neighbor);
    pool_put(bmp->neighbors, neighbor);
    clib_warning("Removed neighbor %U", format_ip4_address, &neighbor->neighbor_ip);
}

/* Find a BGP neighbor by its IP */
bgp_neighbor_t *bgp_find_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    bgp_neighbor_t *neighbor;
    pool_foreach(neighbor, bmp->neighbors) {
        if (ip4_address_cmp(&neighbor->neighbor_ip, &neighbor_ip) == 0) {
            return neighbor;
        }
    };
    return NULL;
}

/* Reset a BGP neighbor session */
void bgp_reset_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    bgp_neighbor_t *neighbor = bgp_find_neighbor(bmp, neighbor_ip);

    if (!neighbor) {
        clib_warning("Neighbor %U not found, cannot reset session.", format_ip4_address, &neighbor_ip);
        return;
    }

    clib_warning("Resetting BGP neighbor: %U...", format_ip4_address, &neighbor_ip);

    // Transition to Idle state
    neighbor->state = BGP_STATE_IDLE;

    // Clear queued messages for this neighbor
    vec_free(neighbor->output_queue);

    // Clear RIB-in and RIB-out entries for this neighbor
    bgp_clear_rib_in_for_neighbor(bmp, neighbor_ip);
    bgp_clear_rib_out_for_neighbor(bmp, neighbor_ip);

    // Restart the session by transitioning to Connect state
    neighbor->state = BGP_STATE_CONNECT;

    // Notify the periodic process to handle re-connection
    vlib_process_signal_event(bmp->vlib_main, bmp->periodic_node_index, BGP_EVENT1, (uword)neighbor);

    clib_warning("BGP neighbor %U reset complete.", format_ip4_address, &neighbor_ip);
}

int bgp_enable_disable(bgp_main_t *bmp, u32 sw_if_index, int enable_disable) {
    clib_warning("%sabling BGP on interface index %u",
                 enable_disable ? "En" : "Dis", sw_if_index);

    // Ensure the interface index is valid
    vnet_sw_interface_t *sw_interface = vnet_get_sw_interface(bmp->vnet_main, sw_if_index);
    if (!sw_interface) {
        clib_warning("Invalid interface index: %u", sw_if_index);
        return -1;
    }

    // Add logic to enable or disable BGP on the interface
    if (enable_disable) {
        // Logic to enable BGP (e.g., adding interface to BGP processing)
        bmp->bgp_enabled_interfaces |= (1 << sw_if_index);
        clib_warning("BGP enabled on interface: %u", sw_if_index);
    } else {
        // Logic to disable BGP (e.g., removing interface from BGP processing)
        bmp->bgp_enabled_interfaces &= ~(1 << sw_if_index);
        clib_warning("BGP disabled on interface: %u", sw_if_index);
    }

    return 0;
}


void bgp_hard_reset_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    bgp_neighbor_t *neighbor = bgp_find_neighbor(bmp, neighbor_ip);

    if (!neighbor) {
        clib_warning("Neighbor %U not found.", format_ip4_address, &neighbor_ip);
        return;
    }

    clib_warning("Performing hard reset for BGP neighbor %U...", format_ip4_address, &neighbor_ip);

    // Transition neighbor to Idle state
    neighbor->state = BGP_STATE_IDLE;

    // Clear RIB-in and RIB-out for the neighbor
    bgp_clear_rib_in_for_neighbor(bmp, neighbor_ip);
    bgp_clear_rib_out_for_neighbor(bmp, neighbor_ip);

    // Clear session-specific resources
    bgp_clear_session_resources(neighbor);

    // Remove the neighbor
    bgp_remove_neighbor(bmp, neighbor);

    // Reinitialize the neighbor
    u32 remote_as = neighbor->remote_as; // Save the remote AS number
    bgp_add_neighbor(bmp, neighbor_ip, remote_as);

    // Transition to the Connect state to restart the session
    neighbor = bgp_find_neighbor(bmp, neighbor_ip);
    if (neighbor) {
        neighbor->state = BGP_STATE_CONNECT;

        // Notify periodic process to handle reconnection
        vlib_process_signal_event(bmp->vlib_main, bmp->periodic_node_index, BGP_EVENT1, (uword) neighbor);
        clib_warning("Hard reset for BGP neighbor %U completed.", format_ip4_address, &neighbor_ip);
    } else {
        clib_warning("Failed to reinitialize neighbor %U after hard reset.", format_ip4_address, &neighbor_ip);
    }
}

void bgp_soft_reset_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip, bool inbound) {
    bgp_neighbor_t *neighbor = bgp_find_neighbor(bmp, neighbor_ip);

    if (!neighbor) {
        clib_warning("Neighbor %U not found.", format_ip4_address, &neighbor_ip);
        return;
    }

    clib_warning("Performing soft reset for BGP neighbor %U (%s)...",
                 format_ip4_address, &neighbor_ip, inbound ? "inbound" : "outbound");

    if (inbound) {
        // Reapply inbound route policies and refresh RIB-in
        bgp_clear_rib_in_for_neighbor(bmp, neighbor_ip);
        bgp_request_full_update(neighbor, /*rib_in=*/true);
    } else {
        // Reapply outbound route policies and refresh RIB-out
        bgp_clear_rib_out_for_neighbor(bmp, neighbor_ip);
        bgp_recompute_rib_out(neighbor);
    }

    clib_warning("Soft reset for BGP neighbor %U (%s) completed.",
                 format_ip4_address, &neighbor_ip, inbound ? "inbound" : "outbound");
}

void bgp_clear_session_resources(bgp_neighbor_t *neighbor) {
    if (neighbor->socket) {
        bgp_socket_close(neighbor->socket); // Close the neighbor's socket
        neighbor->socket = NULL;
    }
    queue_free(&neighbor->output_queue); // Free the neighbor's message queue
    clib_warning("Cleared session resources for neighbor %U", format_ip4_address, &neighbor->neighbor_ip);
}

void bgp_clear_rib_in_for_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    // Placeholder for clearing inbound RIB
    clib_warning("Cleared inbound RIB for neighbor %U", format_ip4_address, &neighbor_ip);
}

void bgp_clear_rib_out_for_neighbor(bgp_main_t *bmp, ip4_address_t neighbor_ip) {
    // Placeholder for clearing outbound RIB
    clib_warning("Cleared outbound RIB for neighbor %U", format_ip4_address, &neighbor_ip);
}