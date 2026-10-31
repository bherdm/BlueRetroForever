/*
 * SPDX-License-Identifier: Apache-2.0
 */

/* A test ROM for BlueRetroForever's own command (0x7B, docs/n64-commands-design.md), sent through
 * libdragon's raw Joybus call: status on every port, then open pairing and wait for a device on
 * port 1, step the LED's levels, close pairing and disconnect port 1. Each reply is a [cc] line
 * on the SC64's USB debug lane; [cc] done ends the run. */

#include <stdio.h>
#include <stdint.h>
#include <libdragon.h>

#define CC_ID 0x7B
#define CC_REPLY_LEN 10
enum { CC_STATUS, CC_PAIRING_OPEN, CC_PAIRING_CLOSE, CC_DISCONNECT, CC_LED_LEVEL };

static const char *dev_name(uint8_t dev)
{
    static const char *names[] = { "-", "pad", "kb", "mouse" };
    return dev < 4 ? names[dev] : "?";
}

/* One command on one port, logged; false if the reply isn't BlueRetroForever's */
static bool cc( int port, uint8_t verb, uint8_t arg, uint8_t reply[CC_REPLY_LEN] )
{
    const uint8_t send[4] = { CC_ID, verb, arg, 0 };

    joybus_exec_cmd( port, sizeof(send), CC_REPLY_LEN, send, reply );
    if( reply[0] != 0x42 )
    {
        debugf( "[cc] port%d verb=%u no BlueRetroForever reply (%02x %02x %02x)\n",
                port + 1, verb, reply[0], reply[1], reply[2] );
        return false;
    }
    debugf( "[cc] port%d verb=%u result=%u version=%u pairing=%u led=%u ports=%s,%s,%s,%s\n",
            port + 1, reply[2], reply[3], reply[1], reply[4] & 1, reply[5],
            dev_name( reply[6] ), dev_name( reply[7] ), dev_name( reply[8] ), dev_name( reply[9] ) );
    return true;
}

static void status_after( int port, uint32_t ms, uint8_t reply[CC_REPLY_LEN] )
{
    wait_ms( ms );
    cc( port, CC_STATUS, 0, reply );
}

int main( void )
{
    uint8_t r[CC_REPLY_LEN];
    uint8_t level;

    console_init();
    debug_init_usblog();
    printf( "BlueRetroForever commands test\n" );
    debugf( "[cc] start\n" );

    for( int port = 0; port < 4; port++ ) cc( port, CC_STATUS, 0, r );
    if( !cc( 0, CC_STATUS, 0, r ) ) { debugf( "[cc] done\n" ); while( 1 ) {} }
    level = r[5];

    cc( 0, 0x09, 0, r );                     /* unknown verb */
    cc( 0, CC_LED_LEVEL, 0, r );             /* off isn't allowed */

    cc( 0, CC_PAIRING_OPEN, 0, r );
    status_after( 0, 500, r );
    for( int i = 0; i < 40 && r[6] == 0; i++ ) status_after( 0, 500, r );   /* a device on port 1 */

    for( uint8_t l = 1; l <= 5; l++ )
    {
        cc( 0, CC_LED_LEVEL, l, r );
        status_after( 0, 1500, r );
    }
    cc( 0, CC_LED_LEVEL, level, r );

    cc( 1, CC_PAIRING_CLOSE, 0, r );         /* from port 2: any port speaks for the board */
    status_after( 0, 500, r );

    cc( 0, CC_DISCONNECT, 0, r );
    status_after( 0, 1500, r );

    debugf( "[cc] done\n" );
    while( 1 ) {}
}
