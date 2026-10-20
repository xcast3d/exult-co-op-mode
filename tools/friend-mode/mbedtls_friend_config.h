/*
 * Friend Mode's additions to Mbed TLS's default configuration, for
 * libdatachannel (tools/friend-mode/build-rtc.sh).  WebRTC's DTLS handshake
 * negotiates SRTP profiles even when no media is sent.
 */
#define MBEDTLS_SSL_DTLS_SRTP
