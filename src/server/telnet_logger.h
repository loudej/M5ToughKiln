#ifndef TELNET_LOGGER_H
#define TELNET_LOGGER_H

#include <cstdint>

/// Install the M5.Log callback and prepare the TCP server.
/// Call once from setup() after M5.begin(). The server socket
/// is opened lazily the first time Wi-Fi is connected.
void telnet_logger_setup(uint16_t port = 23);

/// Accept new clients, prune dead ones, restart after reconnect.
/// Call from loop() on every iteration.
void telnet_logger_service();

#endif // TELNET_LOGGER_H
