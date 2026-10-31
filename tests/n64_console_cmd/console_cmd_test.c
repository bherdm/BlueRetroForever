/*
 * SPDX-License-Identifier: Apache-2.0
 */

/* A test ROM for BlueRetroForever's own command (0x7B, docs/n64-commands-design.md), sent through
 * libdragon's raw Joybus call: status on every port, then open pairing and wait for a device on
 * port 1, step the LED's levels, close pairing, set port 1's keyboard settings until power off and
 * read them back, and disconnect port 1. Each reply is a [cc] line on the SC64's USB debug lane;
 * [cc] done ends the run. Nothing it sets is saved but the LED level, which it puts back. */

#include <stdio.h>
#include <stdint.h>
#include <libdragon.h>

#define CC_ID 0x7B
#define CC_REPLY_LEN 10
enum { CC_STATUS, CC_PAIRING_OPEN, CC_PAIRING_CLOSE, CC_DISCONNECT, CC_LED_LEVEL, CC_KB_SETTINGS, CC_KB_SET };

/* The keyboard settings byte: bits 0-2 the layout (0 US to 4 JP), bit 3 PC, bit 4 classic, bit 7
 * until power off */
#define KB_DE 2
#define KB_PC 0x08
#define KB_CLASSIC 0x10
#define KB_UNTIL_OFF 0x80

/* The extended keyboard read, version 2: marker, version, status, settings, ten HID codes */
#define KB_EXT_CMD 0x2A
#define KB_EXT_REPLY_LEN 24

static const char *dev_name(uint8_t dev)
{
    static const char *names[] = { "-", "pad", "kb", "mouse" };
    return dev < 4 ? names[dev] : "?";
}

/* One command on one port, logged; false if the reply isn't BlueRetroForever's */
static bool cc2( int port, uint8_t verb, uint8_t arg0, uint8_t arg1, uint8_t reply[CC_REPLY_LEN] )
{
    const uint8_t send[4] = { CC_ID, verb, arg0, arg1 };

    joybus_exec_cmd( port, sizeof(send), CC_REPLY_LEN, send, reply );
    if( reply[0] != 0x42 )
    {
        debugf( "[cc] port%d verb=%u no BlueRetroForever reply (%02x %02x %02x)\n",
                port + 1, verb, reply[0], reply[1], reply[2] );
        return false;
    }
    if( verb == CC_KB_SETTINGS )
    {
        debugf( "[cc] port%d verb=%u result=%u version=%u settings=%02x,%02x,%02x,%02x\n",
                port + 1, reply[2], reply[3], reply[1], reply[6], reply[7], reply[8], reply[9] );
        return true;
    }
    debugf( "[cc] port%d verb=%u result=%u version=%u pairing=%u led=%u ports=%s,%s,%s,%s\n",
            port + 1, reply[2], reply[3], reply[1], reply[4] & 1, reply[5],
            dev_name( reply[6] ), dev_name( reply[7] ), dev_name( reply[8] ), dev_name( reply[9] ) );
    return true;
}

static bool cc( int port, uint8_t verb, uint8_t arg, uint8_t reply[CC_REPLY_LEN] )
{
    return cc2( port, verb, arg, 0, reply );
}

static void status_after( int port, uint32_t ms, uint8_t reply[CC_REPLY_LEN] )
{
    wait_ms( ms );
    cc( port, CC_STATUS, 0, reply );
}

/* The extended keyboard read's header, logged: a classic or silent port has no marker */
static void kb_ext( int port )
{
    const uint8_t send[2] = { KB_EXT_CMD, 0x00 };
    uint8_t reply[KB_EXT_REPLY_LEN] = { 0 };

    joybus_exec_cmd( port, sizeof(send), sizeof(reply), send, reply );
    debugf( "[cc] port%d 0x2A marker=%02x version=%u status=%02x settings=%02x first=%04x\n",
            port + 1, reply[0], reply[1], reply[2], reply[3], (reply[4] << 8) | reply[5] );
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

    /* Port 1's keyboard settings, set until power off only, so nothing is saved */
    cc( 0, CC_KB_SETTINGS, 0, r );
    uint8_t saved = r[6];
    kb_ext( 0 );
    cc2( 0, CC_KB_SET, 0, 0x07, r );         /* layout 7 is refused */
    cc2( 0, CC_KB_SET, 0, KB_DE | KB_PC | KB_UNTIL_OFF, r );
    wait_ms( 100 );
    cc( 0, CC_KB_SETTINGS, 0, r );
    kb_ext( 0 );                             /* the settings byte follows, no silence */
    cc2( 0, CC_KB_SET, 0, KB_DE | KB_PC | KB_CLASSIC | KB_UNTIL_OFF, r );
    wait_ms( 100 );
    kb_ext( 0 );                             /* silent for 1.5 s: no reply */
    wait_ms( 2000 );
    kb_ext( 0 );                             /* classic: the identity, no marker */
    cc2( 0, CC_KB_SET, 0, ( saved & 0x1F ) | KB_UNTIL_OFF, r );
    wait_ms( 2000 );
    cc( 0, CC_KB_SETTINGS, 0, r );
    kb_ext( 0 );                             /* back as it was saved, until power off */

    cc( 0, CC_DISCONNECT, 0, r );
    status_after( 0, 1500, r );

    debugf( "[cc] done\n" );
    while( 1 ) {}
}
