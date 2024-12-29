#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vppinfra/socket.h>
#include <fcntl.h>
#include <bgp/bgp.h>


/* Initialize the BGP socket */
bgp_socket_t *bgp_socket_init(ip4_address_t *peer_ip) {
    bgp_socket_t *sock = clib_mem_alloc(sizeof(bgp_socket_t));
    memset(sock, 0, sizeof(*sock));

    sock->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->socket_fd < 0) {
        clib_warning("Failed to create socket");
        clib_mem_free(sock);
        return NULL;
    }

    sock->peer_addr.sin_family = AF_INET;
    sock->peer_addr.sin_port = htons(BGP_PORT);
    sock->peer_addr.sin_addr.s_addr = peer_ip->as_u32;

    return sock;
}

/* Establish a connection to the BGP peer */
int bgp_socket_connect(bgp_socket_t *sock) {
    int flags = fcntl(sock->socket_fd, F_GETFL, 0);
    fcntl(sock->socket_fd, F_SETFL, flags | O_NONBLOCK);

    if (connect(sock->socket_fd, (struct sockaddr *)&sock->peer_addr, sizeof(sock->peer_addr)) < 0) {
        if (errno != EINPROGRESS) {
            clib_warning("Failed to connect to BGP peer: %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

/* Send a BGP message */
int bgp_socket_send(bgp_socket_t *sock, void *message, size_t length) {
    ssize_t sent = send(sock->socket_fd, message, length, 0);
    if (sent < 0) {
        clib_warning("Failed to send message");
        return -1;
    }
    return sent;
}

/* Receive a BGP message */
int bgp_socket_receive(bgp_socket_t *sock, void *buffer, size_t buffer_size) {
    ssize_t received = recv(sock->socket_fd, buffer, buffer_size, 0);
    if (received < 0) {
        clib_warning("Failed to receive message");
        return -1;
    }
    return received;
}

/* Close the BGP socket */
void bgp_socket_close(bgp_socket_t *sock) {
    if (sock->socket_fd >= 0) {
        close(sock->socket_fd);
    }
    clib_mem_free(sock);
}
