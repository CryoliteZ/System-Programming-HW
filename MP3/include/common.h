#define INIT_JOB_OP 0x00
#define FIND_MINE_OP 0x01
#define SHARE_VERIFY_OP 0x02
#define ASSIGN_JOB_OP 0x03
#define USER_CMD_OP 0x04


typedef struct{
    uint8_t op;
    uint32_t datalen;
    uint8_t bytes[sizeof(uint8_t) + sizeof(uint32_t)];
} AIO_protocol_header;



typedef struct {
    AIO_protocol_header header;
    struct{
        int start;
        int end;
        int nodestart;
        int nodeend;
        int datalen;
        int targetzeros;
    } body;
    
} AIO_protocol_initjob;

typedef struct {
    AIO_protocol_header header;
    struct{
        int start;
        int end;
        int nodestart;
        int nodeend;
        int targetzeros;
    } body;
    
} AIO_protocol_assignjob;

typedef struct {
    AIO_protocol_header header;
    struct{
        int find;
        int minelen;
        int findzero;
    } body;
    
} AIO_protocol_findmine;

typedef struct {
    AIO_protocol_header header;
    struct{
        int ac;
        int minelen;
        int minerid;
        int minerlen;
        int targetzeros;
    } body;
    
} AIO_protocol_verify;

typedef struct {
    AIO_protocol_header header;
    struct{
        int cmd;
    } body;
    
} AIO_protocol_cmd;