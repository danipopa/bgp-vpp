
#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <bgp/bgp.h>



void bgp_show_summary(vlib_main_t *vm, bgp_main_t *bmp) {
    bgp_neighbor_t *neighbor;

    vlib_cli_output(vm, "BGP Summary:");
    vlib_cli_output(vm, "  Router ID: %U", format_ip4_address, &bmp->bgp_router_id);
    vlib_cli_output(vm, "  Local AS Number: %u", bmp->bgp_as_number);

    vlib_cli_output(vm, "Neighbors:");
    pool_foreach (neighbor, bmp->neighbors) {
        vlib_cli_output(vm, "  Neighbor: %U, Remote AS: %u, State: %s, Hold Timer: %u, Keepalive Timer: %u",
                        format_ip4_address, &neighbor->neighbor_ip,
                        neighbor->remote_as,
                        bgp_state_to_string(neighbor->state),
                        neighbor->hold_timer,
                        neighbor->keepalive_timer);
    }
}


void bgp_show_config(vlib_main_t *vm, bgp_main_t *bmp) {
    bgp_neighbor_t *neighbor;

    vlib_cli_output(vm, "BGP Configuration:");
    vlib_cli_output(vm, "  Router ID: %U", format_ip4_address, &bmp->bgp_router_id);
    vlib_cli_output(vm, "  Local AS Number: %u", bmp->bgp_as_number);
    vlib_cli_output(vm, "  Hold Time: %u", bmp->hold_time);
    vlib_cli_output(vm, "  Keepalive Time: %u", bmp->keepalive_time);
    vlib_cli_output(vm, "  Cluster ID: %u", bmp->cluster_id);

    vlib_cli_output(vm, "\nNeighbors:");
    pool_foreach(neighbor, bmp->neighbors) {
        vlib_cli_output(vm, "  Neighbor: %U, Remote AS: %u, State: %s, Route Reflector Client: %s, Route Filter: %s",
                        format_ip4_address, &neighbor->neighbor_ip,
                        neighbor->remote_as,
                        bgp_state_to_string(neighbor->state),
                        neighbor->is_route_reflector_client ? "Yes" : "No",
                        neighbor->route_filter_name[0] ? neighbor->route_filter_name : "None");
    }

    vlib_cli_output(vm, "\nPrefix Lists:");
    bgp_prefix_list_t **list; // Pointer to a pointer for vec_foreach compatibility
    vec_foreach(list, bmp->prefix_lists) {  // Iterate over pointers to prefix lists
        bgp_prefix_list_t *prefix_list = *list; // Dereference to get the actual prefix list
        vlib_cli_output(vm, "  Prefix List: %s", prefix_list->name);

        bgp_prefix_t **entry;
        vec_foreach(entry, prefix_list->entries) {  // Iterate over entries in the prefix list
            vlib_cli_output(vm, "    %s %U/%d",
                            (*entry)->permit ? "permit" : "deny",
                            format_ip4_address, &(*entry)->prefix, (*entry)->mask_length);
        }
    }

    vlib_cli_output(vm, "\nAdvertised Networks:");
    bgp_route_t *route;
    pool_foreach(route, bmp->routes) {
        vlib_cli_output(vm, "  Network: %U/%d -> Next Hop: %U",
                        format_ip4_address, &route->prefix, route->mask_length,
                        format_ip4_address, &route->next_hop);
    }
}



/* Command: Clear Neighbor */
static clib_error_t *
bgp_neighbor_clear_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    ip4_address_t neighbor_ip;

    if (!unformat(input, "%U", unformat_ip4_address, &neighbor_ip)) {
        return clib_error_return(0, "Please specify a valid neighbor IP address.");
    }

    bgp_reset_neighbor(&bgp_main, neighbor_ip);
    clib_warning("Cleared BGP session with neighbor %U.", format_ip4_address, &neighbor_ip);
    return 0;
}

VLIB_CLI_COMMAND(bgp_neighbor_clear_command, static) = {
    .path = "bgp neighbor clear",
    .short_help = "bgp neighbor clear <ip-address>",
    .function = bgp_neighbor_clear_command_fn,
};

/* Command: Set Router ID */
static clib_error_t *
bgp_set_router_id_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    bgp_main_t *bmp = &bgp_main;
    ip4_address_t router_id;

    if (!unformat(input, "%U", unformat_ip4_address, &router_id)) {
        return clib_error_return(0, "Invalid IPv4 address format");
    }

    bmp->bgp_router_id = router_id.as_u32;
    clib_warning("BGP Router ID set to %U", format_ip4_address, &router_id);
    return 0;
}

VLIB_CLI_COMMAND(bgp_set_router_id_command, static) = {
    .path = "set bgp router-id",
    .short_help = "set bgp router-id <IPv4>",
    .function = bgp_set_router_id_command_fn,
};

/* Command: Set AS Number */
static clib_error_t *
bgp_set_as_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    bgp_main_t *bmp = &bgp_main;
    u32 as_number;

    if (!unformat(input, "%d", &as_number)) {
        return clib_error_return(0, "Invalid AS number");
    }

    bmp->bgp_as_number = as_number;
    clib_warning("BGP AS number set to %u", as_number);
    return 0;
}

VLIB_CLI_COMMAND(bgp_set_as_command, static) = {
    .path = "set bgp as",
    .short_help = "set bgp as <AS-number>",
    .function = bgp_set_as_command_fn,
};

/* Command: Add Neighbor */
static clib_error_t *
bgp_add_neighbor_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    ip4_address_t neighbor_ip;
    u32 remote_as;

    if (!unformat(input, "%U remote-as %d", unformat_ip4_address, &neighbor_ip, &remote_as)) {
        return clib_error_return(0, "Usage: set bgp neighbor <IPv4> remote-as <AS-number>");
    }

    bgp_add_neighbor(&bgp_main, neighbor_ip, remote_as);
    clib_warning("Added BGP neighbor %U with remote AS %u", format_ip4_address, &neighbor_ip, remote_as);
    return 0;
}

VLIB_CLI_COMMAND(bgp_add_neighbor_command, static) = {
    .path = "set bgp neighbor",
    .short_help = "set bgp neighbor <IPv4> remote-as <AS-number>",
    .function = bgp_add_neighbor_command_fn,
};

/* Command: Enable Interface */
static clib_error_t *
bgp_enable_interface_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    u32 sw_if_index = ~0;

    if (!unformat(input, "%U", unformat_vnet_sw_interface, vnet_get_main(), &sw_if_index)) {
        return clib_error_return(0, "Invalid interface name");
    }

    bgp_enable_disable(&bgp_main, sw_if_index, 1);
    clib_warning("Enabled BGP on interface index %u", sw_if_index);
    return 0;
}

VLIB_CLI_COMMAND(bgp_enable_interface_command, static) = {
    .path = "set bgp enable",
    .short_help = "set bgp enable <interface-name>",
    .function = bgp_enable_interface_command_fn,
};

/* Command: Advertise Network */
static clib_error_t *
bgp_advertise_network_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    // fib_prefix_t prefix;
    ip4_address_t prefix;
    u8 mask_length;

    // Ensure both prefix and mask_length are extracted
    if (!unformat(input, "%U/%d", unformat_ip4_address, &prefix, &mask_length)) {
        return clib_error_return(0, "Invalid network prefix format. Usage: set bgp advertise network <prefix>/<mask-length>");
    }

    bgp_advertise_network(&bgp_main, prefix, mask_length);
    clib_warning("Advertised BGP network %U/%d", format_ip4_address, &prefix, mask_length);
    return 0;
}

VLIB_CLI_COMMAND(bgp_advertise_network_command, static) = {
    .path = "set bgp advertise network",
    .short_help = "set bgp advertise network <prefix>",
    .function = bgp_advertise_network_command_fn,
};

/* Command: Show Configuration */
static clib_error_t *
bgp_show_config_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    bgp_show_config(vm, &bgp_main); // Pass both vm and bgp_main
    return 0;
}

VLIB_CLI_COMMAND(bgp_show_config_command, static) = {
    .path = "show bgp config",
    .short_help = "show bgp config",
    .function = bgp_show_config_command_fn,
};

/* Command: Show Summary */
static clib_error_t *
bgp_show_summary_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    bgp_show_summary(vm, &bgp_main);
    return 0;
}

VLIB_CLI_COMMAND(bgp_show_summary_command, static) = {
    .path = "show bgp summary",
    .short_help = "show bgp summary",
    .function = bgp_show_summary_command_fn,
};

/* Command: Reset Neighbor */
static clib_error_t *
bgp_neighbor_reset_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    ip4_address_t neighbor_ip;

    if (!unformat(input, "%U", unformat_ip4_address, &neighbor_ip)) {
        return clib_error_return(0, "Please specify a valid neighbor IP address.");
    }

    bgp_hard_reset_neighbor(&bgp_main, neighbor_ip);
    clib_warning("BGP session with neighbor %U reset.", format_ip4_address, &neighbor_ip);
    return 0;
}

VLIB_CLI_COMMAND(bgp_neighbor_reset_command, static) = {
    .path = "bgp neighbor reset",
    .short_help = "bgp neighbor reset <ip-address>",
    .function = bgp_neighbor_reset_command_fn,
};

/* Command: Soft Reset Neighbor */
static clib_error_t *
bgp_neighbor_soft_reset_command_fn(vlib_main_t *vm, unformat_input_t *input, vlib_cli_command_t *cmd) {
    ip4_address_t neighbor_ip;

    if (!unformat(input, "%U", unformat_ip4_address, &neighbor_ip)) {
        return clib_error_return(0, "Please specify a valid neighbor IP address.");
    }

    bgp_soft_reset_neighbor(&bgp_main, neighbor_ip, /* inbound= */ true);
    clib_warning("BGP session with neighbor %U soft-reset.", format_ip4_address, &neighbor_ip);
    return 0;
}

VLIB_CLI_COMMAND(bgp_neighbor_soft_reset_command, static) = {
    .path = "bgp neighbor soft-reset",
    .short_help = "bgp neighbor soft-reset <ip-address>",
    .function = bgp_neighbor_soft_reset_command_fn,
};
