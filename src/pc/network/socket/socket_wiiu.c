#ifdef TARGET_WII_U

#include "socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char gGetHostName[256] = { 0 };

static bool ns_socket_initialize(enum NetworkType networkType, bool reconnecting) {
    (void)reconnecting;
    // Allow local host sessions until Wii U socket networking is ported.
    return (networkType != NT_CLIENT);
}

static s64 ns_socket_get_id(u8 localIndex) {
    (void)localIndex;
    return 0;
}

static char* ns_socket_get_id_str(u8 localIndex) {
    (void)localIndex;
    return "";
}

static void ns_socket_save_id(u8 localIndex, s64 networkId) {
    (void)localIndex;
    (void)networkId;
}

static void ns_socket_clear_id(u8 localIndex) {
    (void)localIndex;
}

static void* ns_socket_dup_addr(u8 localIndex) {
    (void)localIndex;
    return NULL;
}

static bool ns_socket_match_addr(void* addr1, void* addr2) {
    return addr1 == addr2;
}

static void ns_socket_update(void) {
}

static int ns_socket_send(u8 localIndex, void* addr, u8* data, u16 dataLength) {
    (void)localIndex;
    (void)addr;
    (void)data;
    (void)dataLength;
    return 0;
}

static void ns_socket_get_lobby_id(char* destination, u32 destLength) {
    snprintf(destination, destLength, "%s", "");
}

static void ns_socket_get_lobby_secret(char* destination, u32 destLength) {
    snprintf(destination, destLength, "%s", "");
}

static void ns_socket_shutdown(bool reconnecting) {
    (void)reconnecting;
}

struct NetworkSystem gNetworkSystemSocket = {
    .initialize = ns_socket_initialize,
    .get_id = ns_socket_get_id,
    .get_id_str = ns_socket_get_id_str,
    .save_id = ns_socket_save_id,
    .clear_id = ns_socket_clear_id,
    .dup_addr = ns_socket_dup_addr,
    .match_addr = ns_socket_match_addr,
    .update = ns_socket_update,
    .send = ns_socket_send,
    .get_lobby_id = ns_socket_get_lobby_id,
    .get_lobby_secret = ns_socket_get_lobby_secret,
    .shutdown = ns_socket_shutdown,
    .requireServerBroadcast = true,
    .name = "Socket",
};

#endif
