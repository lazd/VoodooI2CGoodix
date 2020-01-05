#ifndef VoodooI2CGoodix_h
#define VoodooI2CGoodix_h

#define GOODIX_READ_COOR_ADDR        0x814E
#define GOODIX_GT1X_REG_CONFIG_DATA    0x8050
#define GOODIX_GT9X_REG_CONFIG_DATA    0x8047
#define GOODIX_REG_ID            0x8140

#define GOODIX_CONFIG_MAX_LENGTH    240
#define GOODIX_CONFIG_911_LENGTH    186
#define GOODIX_CONFIG_967_LENGTH    228

#define GOODIX_MAX_HEIGHT        4096
#define GOODIX_MAX_WIDTH        4096
#define GOODIX_INT_TRIGGER        1
#define GOODIX_CONTACT_SIZE        8
#define GOODIX_MAX_CONTACTS        10

#define RESOLUTION_LOC        1
#define MAX_CONTACTS_LOC    5
#define TRIGGER_LOC        6

#define GOODIX_BUFFER_STATUS_READY    BIT(7)
#define GOODIX_BUFFER_STATUS_TIMEOUT    20000000

#define msleep(x)               IOSleep(x)
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define usleep_range(min, max)    msleep(DIV_ROUND_UP(min, 1000))

#endif /* VoodooI2CGoodix_h */
