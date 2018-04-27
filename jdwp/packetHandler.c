#include <pthread.h>
#include "misc/MTypes.h"
#include "initializer.h"

#define NO_ERRORCODE 0
#define PACKET_HEADER_SIZE MIN_PACKET_SIZE
#define TRUE 1
#define FALSE 0

#define __LOCK sync_start();
#define __UNLOCK sync_end();

// unique id for packets, 4 bytes
static volatile u4 PACKET_ID_UNIQUE          = 1;

// Packet id gen Mutex
static pthread_mutex_t MUTEX_T               = PTHREAD_MUTEX_INITIALIZER; // macro init.

// forward internals
static void sync_start();
static void sync_end();

static u4 gen_packetID();
// creators
static Packet* newReplyPacket(u4 id, size_t bytes); // handler's id
static Packet* newCommandPacket(size_t bytes, u1 command, u1 commandset);
static Packet* newPacketFromHeader(u1* header);
static Packet* newPacketFromHeaderPayload(u1* header, u1* payload);
static Packet* newRawPacket(size_t rawbytes);

// custom list functions
static void add_Packet(Packet* p, PacketList* plist);
static size_t get_ByteSize(PacketList* plist);
static void write_packet(Packet* to, Packet* from);
static Packet* write_list(Packet* p, PacketList* plist);

// basic byte readers
static u1 read_u1(Packet* p);
static u2 read_u2(Packet* p);
static u4 read_u4(Packet* p);
static u8 read_u8(Packet* p);

// helpers
static u2 read_u2_buff(u1* buff);
static u4 read_u4_buff(u1* buff);
static u8 read_u8_buff(u1* buff);

// explicit info readers
static u4 read_length(Packet* p);
static u4 read_id(Packet* p);
static u1 read_flags(Packet* p);
static u2 read_errorcode(Packet* p);

// explicit command readers
static u1 read_command(Packet* p);
static u1 read_commandset(Packet* p);

// jdwp specific readers
static tagid_t read_tag(Packet* p);
static methodid_t read_methodid(Packet* p);
static fieldid_t read_fieldid(Packet* p);
static objectid_t read_objectid(Packet* p);
static threadid_t read_threadid(Packet* p);
static threadgroupid_t read_threadgroupid(Packet* p);
static frameid_t read_frameid(Packet* p);
static Location* read_location(Packet* p);
static u1* read_str(Packet* p);

// basic byte writers
static void write_u1(Packet* p, u1 u1);
static void write_u2(Packet* p, u2 u2);
static void write_u4(Packet* p, u4 u4);
static void write_u8(Packet* p, u8 u8);

// jdwp specific writers

// jdwp header
static void write_length(Packet* p, u4 u4);
static void write_command(Packet* p, u1 u1);
static void write_commandset(Packet* p, u1 u1);
static void write_id(Packet* p, u4 u4);
static void write_flags(Packet* p, u1 u1);
static void write_errorcode(Packet* p, u2 u2);

// data variable
static void write_tag(Packet* p, tagid_t t);
static void write_methodid(Packet* p, methodid_t mid);
static void write_fieldid(Packet* p, fieldid_t fid);
static void write_objectid(Packet* p, objectid_t oid);
static void write_threadid(Packet* p, threadid_t tid);
static void write_threadgroupid(Packet* p, threadgroupid_t tgid);
static void write_frameid(Packet* p, frameid_t frid);
static void write_location(Packet* p, Location* loc);
static void write_str(Packet* p, u1* str);
static u1 isPacketReply(Packet* p);
static u1 isPacketCommand(Packet* p);

static u4 internalcUtf_size(u1* str);
static u4 internalUtf_size(u2* utf);

static u4 utf_csize(u1* str);
static u4 utf_size(u2* utf);

static void write_cutf(Packet* p, u1* str);
static void write_utf(Packet* p, u2* utf);

static u2* read_utf(Packet* p);

static void ensurePacketCap(Packet* p, size_t mbytes);

inline static void sync_start() {
    if(pthread_mutex_lock(&MUTEX_T) != 0) // must return 0, to tell we own the lock
        MHandler->error_exit("Fatal Error at generating new Packet ID! Mutex could not be locked!");
}

inline static void sync_end() {
    if(pthread_mutex_unlock(&MUTEX_T) != 0)
        MHandler->error_exit("Fatal Error at generating new Packet ID! Mutex could not be Unlocked!");
}

// returns int bytes ID of Command Packet
// This function is internally called by sedona vm executer (main_pid) and by jdwp server (jdwp server thread)
inline static u4 gen_packetID() {
    u4 _id_; // can be defined out of lock scope
    __LOCK
     _id_ = PACKET_ID_UNIQUE++; // lets do it..
    __UNLOCK
    return _id_;
}

inline static void ensurePacketCap(Packet* p, size_t mbytes) {
    if(p->offset + mbytes >= p->length) {
        size_t newmsize = mbytes + (p->length << 1);
        p->data = (u1*) realloc(p->data, newmsize);
        p->length = newmsize;

        if(p->data == NULL) {
            MHandler->error_exit("failed allocating packet bytes");
        }
    }
}

void init_packetHandler() {
    int mret;
    mret = pthread_mutex_init(&MUTEX_T, NULL);

    PacketHandler->newReplyPacket = newReplyPacket;
    PacketHandler->newCommandPacket = newCommandPacket;
    PacketHandler->newPacketFromHeader = newPacketFromHeader;
    PacketHandler->newPacketFromHeaderPayload = newPacketFromHeaderPayload;
    PacketHandler->newRawPacket = newRawPacket;

    // custom list functions
    PacketHandler->add_Packet = add_Packet;
    PacketHandler->get_ByteSize = get_ByteSize;
    PacketHandler->write_packet = write_packet;
    PacketHandler->write_list = write_list;

    // basic byte readers
    PacketHandler->read_u1 = read_u1;
    PacketHandler->read_u2 = read_u2;
    PacketHandler->read_u4 = read_u4;
    PacketHandler->read_u8 = read_u8;

    // helpers
    PacketHandler->read_u2_buff = read_u2_buff;
    PacketHandler->read_u4_buff = read_u4_buff;
    PacketHandler->read_u8_buff = read_u8_buff;

    // explicit info readers
    PacketHandler->read_length = read_length;
    PacketHandler->read_id = read_id;
    PacketHandler->read_flags = read_flags;
    PacketHandler->read_errorcode = read_errorcode;

    // explicit command readers
    PacketHandler->read_command = read_command;
    PacketHandler->read_commandset = read_commandset;

    // jdwp specific readers
    PacketHandler->read_tag = read_tag;
    PacketHandler->read_methodid = read_methodid;
    PacketHandler->read_fieldid = read_fieldid;
    PacketHandler->read_objectid = read_objectid;
    PacketHandler->read_threadid = read_threadid;
    PacketHandler->read_threadgroupid = read_threadgroupid;
    PacketHandler->read_frameid = read_frameid;
    PacketHandler->read_location = read_location;
    PacketHandler->read_str = read_str;

    // basic byte writers
    PacketHandler->write_u1 = write_u1;
    PacketHandler->write_u2 = write_u2;
    PacketHandler->write_u4 = write_u4;
    PacketHandler->write_u8 = write_u8;

    // jdwp specific writers

    // jdwp header
    PacketHandler->write_length = write_length;
    PacketHandler->write_command = write_command;
    PacketHandler->write_commandset = write_commandset;
    PacketHandler->write_id = write_id;
    PacketHandler->write_flags = write_flags;
    PacketHandler->write_errorcode = write_errorcode;

    // data variable
    PacketHandler->write_tag = write_tag;
    PacketHandler->write_methodid = write_methodid;
    PacketHandler->write_fieldid = write_fieldid;
    PacketHandler->write_objectid = write_objectid;
    PacketHandler->write_threadid = write_threadid;
    PacketHandler->write_threadgroupid = write_threadgroupid;
    PacketHandler->write_frameid = write_frameid;
    PacketHandler->write_location = write_location;
    PacketHandler->write_str = write_str;
    PacketHandler->isPacketReply = isPacketReply;
    PacketHandler->isPacketCommand = isPacketCommand;

    PacketHandler->utf_csize = utf_csize;
    PacketHandler->utf_size = utf_size;
    PacketHandler->write_utf = write_utf;
    PacketHandler->write_cutf = write_cutf;
    PacketHandler->read_utf = read_utf;
}

inline static u2 read_u2_buff(u1* buff) {
    u2 i = *(buff + 0) << 8;
    i |= *(buff + 1);

    return i;
}

inline static u4 read_u4_buff(u1* buff) {
    u4 i = *(buff + 0) << 24;
    i |= *(buff + 1) << 16;
    i |= *(buff + 2)  << 8;
    i |= *(buff + 3);

    return i;
}

inline static u8 read_u8_buff(u1* buff) {
    u8 i = *(buff + 0) << 56;
    i |= *(buff + 1) << 48;
    i |= *(buff + 2) << 40;
    i |= *(buff + 3) << 32;
    i |= *(buff + 4) << 24;
    i |= *(buff + 5) << 16;
    i |= *(buff + 6)  << 8;
    i |= *(buff + 7);

    return i;
}

// basic byte readers
inline static u1 read_u1(Packet* p) {
    return p->data[p->offset++];
}

inline static u2 read_u2(Packet* p) {
    u2 u2 = read_u2_buff(p->data + p->offset);
    p->offset += short__SIZE;
    return u2;
}

inline static u4 read_u4(Packet* p) {
    u4 u4 = read_u4_buff(p->data + p->offset);
    p->offset += int__SIZE;
    return u4;
}

inline static u8 read_u8(Packet* p) {
    u8 u8 = read_u8_buff(p->data + p->offset);
    p->offset += long__SIZE;
    return u8;
}

// explicit info readers
inline static u4 read_length(Packet* p) {
    return read_u4_buff(p->data + LENGTH_POS);
}

inline static u4 read_id(Packet* p) {
    return read_u4_buff(p->data + ID_POS);
}

inline static u1 read_flags(Packet* p) {
    return *(p->data + FLAGS_POS);
}

inline static u2 read_errorcode(Packet* p) {
    return read_u2_buff(p->data + ERROR_CODE_POS);
}

// explicit command readers | only valid for cmd packets or crap is read
inline static u1 read_command(Packet* p) {
    return *(p->data + COMMAND_POS);
}

inline static u1 read_commandset(Packet* p) {
    return *(p->data + COMMAND_SET_POS);
}

// jdwp specific readers
inline static tagid_t read_tag(Packet* p) {
    #if tag__SIZE == byte__SIZE
        return read_u1(p);
    #elif tag__SIZE == short__SIZE
        return read_u2(p);
    #elif tag__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static methodid_t read_methodid(Packet* p) {
    #if methodID__SIZE == byte__SIZE
        return read_u1(p);
    #elif methodID__SIZE == short__SIZE
        return read_u2(p);
    #elif methodID__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static fieldid_t read_fieldid(Packet* p) {
    #if fieldID__SIZE == byte__SIZE
        return read_u1(p);
    #elif fieldID__SIZE == short__SIZE
        return read_u2(p);
    #elif fieldID__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static objectid_t read_objectid(Packet* p) {
    #if objectid__SIZE == byte__SIZE
        return read_u1(p);
    #elif objectid__SIZE == short__SIZE
        return read_u2(p);
    #elif objectid__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static threadid_t read_threadid(Packet* p) {
    #if threadid__SIZE == byte__SIZE
        return read_u1(p);
    #elif threadid__SIZE == short__SIZE
        return read_u2(p);
    #elif threadid__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static threadgroupid_t read_threadgroupid(Packet* p) {
    #if threadGroupID__SIZE == byte__SIZE
        return read_u1(p);
    #elif threadGroupID__SIZE == short__SIZE
        return read_u2(p);
    #elif threadGroupID__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static frameid_t read_frameid(Packet* p) {
    #if frameID__SIZE == byte__SIZE
        return read_u1(p);
    #elif frameID__SIZE == short__SIZE
        return read_u2(p);
    #elif frameID__SIZE == int__SIZE
        return read_u4(p);
    #else
        return read_u8(p);
    #endif
}

inline static Location* read_location(Packet* p) {
    Location* loc   = (Location*) malloc(sizeof(Location));

    loc->tag        = read_tag(p);
    loc->classID    = read_objectid(p);
    loc->methodID   = read_methodid(p);

    loc->index      = read_u8(p);

    return loc;
}

inline static u1* read_str(Packet* p) {
    u4 len = read_u4(p);

    u1* str = (u1*) malloc(len + 1);
    size_t start = 0;

    while(start < len) {
        str[start++] = read_u1(p);
    }

    str[start] = '\0'; // must be null terminated!
    return str;
}

// basic byte writers
inline static void write_u1(Packet* p, u1 u1) {
    ensurePacketCap(p, sizeof(u1));
    p->data[p->offset++] = u1;
}

inline static void write_u2(Packet* p, u2 u2) {
    ensurePacketCap(p, sizeof(u2));
    p->data[p->offset++] = u2 >> 8;
    p->data[p->offset++] = u2 & 0xff;
}

inline static void write_u4(Packet* p, u4 u4) {
    ensurePacketCap(p, sizeof(u4));
    p->data[p->offset++] = u4 >> 24;
    p->data[p->offset++] = u4 >> 16;
    p->data[p->offset++] = u4 >> 8;
    p->data[p->offset++] = u4 & 0xff;
}

inline static void write_u8(Packet* p, u8 u8) {
    ensurePacketCap(p, sizeof(u8));
    p->data[p->offset++] = u8 >> 56;
    p->data[p->offset++] = u8 >> 48;
    p->data[p->offset++] = u8 >> 40;
    p->data[p->offset++] = u8 >> 32;

    p->data[p->offset++] = u8 >> 24;
    p->data[p->offset++] = u8 >> 16;
    p->data[p->offset++] = u8 >> 8;
    p->data[p->offset++] = u8 & 0xff;
}

// jdwp specific writers
inline static void write_command(Packet* p, u1 u1) {
    p->data[COMMAND_POS] = u1;
}

inline static void write_commandset(Packet* p, u1 u1) {
    p->data[COMMAND_SET_POS] = u1;
}

inline static void write_length(Packet* p, u4 u4) {
    p->data[LENGTH_POS + 0] = u4 >> 24;
    p->data[LENGTH_POS + 1] = u4 >> 16;
    p->data[LENGTH_POS + 2] = u4 >> 8;
    p->data[LENGTH_POS + 3] = u4 & 0xff;
}

inline static void write_id(Packet* p, u4 u4) {
    p->data[ID_POS + 0] = u4 >> 24;
    p->data[ID_POS + 1] = u4 >> 16;
    p->data[ID_POS + 2] = u4 >> 8;
    p->data[ID_POS + 3] = u4 & 0xff;
}

inline static void write_flags(Packet* p, u1 u1) {
    p->data[FLAGS_POS] = u1;
}

inline static void write_errorcode(Packet* p, u2 u2) {
    p->data[ERROR_CODE_POS + 0] = u2 >> 8;
    p->data[ERROR_CODE_POS + 1] = u2 & 0xff;
}

inline static void write_tag(Packet* p, tagid_t t) {
    #if tag__SIZE == byte__SIZE
        write_u1(p, t);
    #elif tag__SIZE == short__SIZE
        write_u2(p, t);
    #elif tag__SIZE == int__SIZE
        write_u4(p, t);
    #else
        write_u8(p, t);
    #endif
}

inline static void write_methodid(Packet* p, methodid_t mid) {
    #if methodID__SIZE == byte__SIZE
        write_u1(p, mid);
    #elif methodID__SIZE == short__SIZE
        write_u2(p, mid);
    #elif methodID__SIZE == int__SIZE
        write_u4(p, mid);
    #else
        write_u8(p, mid);
    #endif
}

inline static void write_fieldid(Packet* p, fieldid_t fid) {
    #if fieldID__SIZE == byte__SIZE
        write_u1(p, fid);
    #elif fieldID__SIZE == short__SIZE
        write_u2(p, fid);
    #elif fieldID__SIZE == int__SIZE
        write_u4(p, fid);
    #else
        write_u8(p, fid);
    #endif
}

inline static void write_objectid(Packet* p, objectid_t oid) {
    #if objectid__SIZE == byte__SIZE
        write_u1(p, oid);
    #elif objectid__SIZE == short__SIZE
        write_u2(p, oid);
    #elif objectid__SIZE == int__SIZE
        write_u4(p, oid);
    #else
        write_u8(p, oid);
    #endif
}

inline static void write_threadid(Packet* p, threadid_t tid) {
    #if threadid__SIZE == byte__SIZE
        write_u1(p, tid);
    #elif threadid__SIZE == short__SIZE
        write_u2(p, tid);
    #elif threadid__SIZE == int__SIZE
        write_u4(p, tid);
    #else
        write_u8(p, tid);
    #endif
}

inline static void write_threadgroupid(Packet* p, threadgroupid_t tgid) {
    #if threadGroupID__SIZE == byte__SIZE
        write_u1(p, tgid);
    #elif threadGroupID__SIZE == short__SIZE
        write_u2(p, tgid);
    #elif threadGroupID__SIZE == int__SIZE
        write_u4(p, tgid);
    #else
        write_u8(p, tgid);
    #endif
}

inline static void write_frameid(Packet* p, frameid_t frid) {
    #if frameID__SIZE == byte__SIZE
        write_u1(p, frid);
    #elif frameID__SIZE == short__SIZE
        write_u2(p, frid);
    #elif frameID__SIZE == int__SIZE
        write_u4(p, frid);
    #else
        write_u8(p, frid);
    #endif
}

inline static void write_location(Packet* p, Location* loc) {
    ensurePacketCap(p, tag__SIZE + classID__SIZE + methodID__SIZE + sizeof(u8));
    write_tag(p, loc->tag);
    write_objectid(p, loc->classID);
    write_methodid(p, loc->methodID);
    write_u8(p, loc->index);
}

inline static void write_str(Packet* p, u1* str) {
    u4 len = strlen(str);
    ensurePacketCap(p, len + sizeof(u4));
    write_u4(p, len * char__SIZE);

    size_t start = 0;
    while(start < len) {
        #if char__SIZE == byte__SIZE
            write_u1(p, str[start++]);
        #elif char__SIZE == short__SIZE
            write_u2(p, str[start++]); // char size 2 byte
        #elif char__SIZE == int__SIZE
            write_u4(p, str[start++]);
        #else
            write_u8(p, str[start++]);
        #endif // char__SIZE
    }
}

inline static u4 utf_csize(u1* str) {
    return int__SIZE + internalcUtf_size(str);
}

inline static u4 utf_size(u2* utf) {
    return int__SIZE + internalUtf_size(utf);
}

inline static u4 internalUtf_size(u2* utf) {
    u4 utfCount   = 0;

    while(*utf != NULL) {
        if(*utf > 0 && *utf <= 127) {
            utfCount++;
        }
        else if(*utf <= 2047) {
            utfCount += 2;
        }
        else {
            utfCount += 3;
        }

        utf++;
    }

    return utfCount;
}

inline static u4 internalcUtf_size(u1* str) {
    u4 utfCount   = 0;

    while(*str != NULL) {
        if(*str > 0 && *str <= 127) {
            utfCount++;
        }
        else { //if(u1Char <= 2047) {
            utfCount += 2;
        }

        str++;
    }

    return utfCount;
}

inline static void write_utf(Packet* p, u2* utf) {
    u4 utfbytesize = internalUtf_size(utf);
    ensurePacketCap(p, utfbytesize + int__SIZE);
    write_u4(p, utfbytesize);

    u2 u2Char;
    while(*utf != NULL) {
        u2Char = *utf;

        if(u2Char > 0 && u2Char <= 127) {
            write_u1(p, u2Char);
        }
        else if(u2Char <= 2047) {
            write_u1(p, (0xc0 | (0x1f & (u2Char >> 6))) & 0xff);
            write_u1(p, (0x80 | (0x3f & u2Char))        & 0xff);
        }
        else {
            write_u1(p, (0xe0 | (0x0f & (u2Char >> 12))) & 0xff);
            write_u1(p, (0x80 | (0x3f & (u2Char >> 6 ))) & 0xff);
            write_u1(p, (0x80 | (0x3f & (u2Char      ))) & 0xff);
        }

        utf++;
    }
}

inline static void write_cutf(Packet* p, u1* str) {
    size_t cutfmsize = internalcUtf_size(str);
    ensurePacketCap(p, cutfmsize + int__SIZE);
    write_u4(p, cutfmsize);

    u2 u2Char;
    while(*str != NULL) {
        u2Char = (u2) *str;

        if(u2Char > 0 && u2Char <= 127) {
            write_u1(p, u2Char);
        }
        else {//if(u2Char <= 2047) {
            write_u1(p, (0xc0 | (0x1f & (u2Char >> 6))) & 0xff);
			write_u1(p, (0x80 | (0x3f & u2Char))        & 0xff);
        }

        str++;
    }
}

inline static u2* read_utf(Packet* p) {
    u4 utfCount = read_u4(p);
    size_t utfStart = 0;
    u2* outstr = (u2*) malloc(sizeof(u2) * utfCount / 3 * 2);
    size_t i = 0;

    u2 u2Char_1;
    u2 u2Char_2;
    u2 u2Char_3;

    while(utfStart < utfCount) {
        u2Char_1 = read_u1(p);

        if((u2Char_1 >> 4) < 12) {
            outstr[i++] = u2Char_1;
            utfStart++;
        }
        else {
            u2Char_2 = read_u1(p);
            if((u2Char_2 >> 4) < 14) {
                if((u2Char_2 & 0xbf) == 0) {
                    MHandler->error_exit("read_utf fatal error!\n");
                    return NULL;
                }

                outstr[i++] = (((u2Char_1 & 0x1F) << 6) | (u2Char_2 & 0x3F));
                utfStart += 2;
            }
            else {
                u2Char_3 = read_u1(p);
                if((u2Char_3 & 0xef) > 0) {
                    if(((u2Char_2 & 0xbf) == 0) || ((u2Char_3 & 0xbf) == 0)) {
                        MHandler->error_exit("read_utf fatal error!\n");
                        return NULL;
                    }

                    outstr[i++] = (((u2Char_1 & 0x0f) << 12) | ((u2Char_2 & 0x3f) << 6) | (u2Char_3 & 0x3f));
                    utfStart += 3;
                }
                else {
                    MHandler->error_exit("read_utf fatal error!\n");
                    return NULL;
                }
            }
        }
    }

    return outstr;
}

// Packet maker
inline static Packet* newPacket(u4 bytes) {
    return newRawPacket(bytes + MIN_PACKET_SIZE);
}

// for onReceive
inline static Packet* newPacketFromHeader(u1* header) {
    Packet* p = newRawPacket(PACKET_HEADER_SIZE);

    memcpy(p->data, header, PACKET_HEADER_SIZE);

    p->offset = PACKET_HEADER_SIZE;

    return p;
}

inline static Packet* newReplyPacket(u4 id, size_t bytes) {
    Packet* p = newPacket(bytes);

    write_id(p, id);
    write_flags(p, REPLY_FLAG);
    write_errorcode(p, NO_ERRORCODE); // fix un-init. memory segment
    p->offset = MIN_PACKET_SIZE;

    return p;
}

inline static Packet* newCommandPacket(size_t bytes, u1 command, u1 commandset) {
    Packet* p = newPacket(bytes);

    write_id(p, gen_packetID());
    write_command(p, command);
    write_commandset(p, commandset);
    write_flags(p, CMD_FLAG);

    p->offset = MIN_PACKET_SIZE;

    return p;
}

inline static u1 isPacketCommand(Packet* p) {
    if((read_flags(p) & REPLY_FLAG) == REPLY_FLAG)
        return FALSE;

    return TRUE;
}

inline static u1 isPacketReply(Packet* p) {
    return isPacketCommand(p) == FALSE;
}

inline static void add_Packet(Packet* p, PacketList* plist) {
    PacketBuff* pbuff = (PacketBuff*) malloc(sizeof(PacketBuff));

    pbuff->packet = p;
    pbuff->next = plist->head;

    plist->head = pbuff;
    plist->size++;
}

inline static size_t get_ByteSize(PacketList* plist) {
    if(plist->size == 0) {
        return 0;
    }

    size_t bytes = 0;

    PacketBuff* phead = plist->head;
    size_t psize = plist->size;

    while(psize-- > 0) {
        bytes += phead->packet->offset;
        phead = phead->next;
    }

    return bytes;
}

inline static void write_packet(Packet* to, Packet* from) {
    if(from->offset == 0) {
        return;
    }

    if(to->length == 0) {
        to->length = to->offset; // wrap
    }

    while(from->offset >= to->length - to->offset) {
        to->data = (u1*) realloc(to->data, to->length << 1);
        to->length <<= 1;
    }
    memcpy(to->data + to->offset, from->data, from->offset);
    to->offset += from->offset;
}

inline static Packet* write_list(Packet* p, PacketList* plist) {
    //Packet* p = newRawPacket(get_ByteSize(plist));
    PacketBuff* phead = plist->head;

    size_t psize = plist->size;
    while(psize-- > 0) {
        write_packet(p, phead->packet);
        phead = phead->next;
    }

    return p;
}

inline static Packet* newPacketFromList(PacketList* plist) {
    Packet* p = newRawPacket(get_ByteSize(plist));
    write_list(p, plist);
    return p;
}

inline static Packet* newPacketFromHeaderPayload(u1* header, u1* payload) {
    u4 psize = read_u4_buff(header + LENGTH_POS);
    Packet* p = newRawPacket(psize);

    memcpy(p->data, header, PACKET_HEADER_SIZE);
    memcpy(p->data + DATA_VARIABLE_POS, payload, psize - PACKET_HEADER_SIZE);

    p->offset = DATA_VARIABLE_POS;
    return p;
}

inline static Packet* newRawPacket(size_t rawbytes) {
    Packet* p = (Packet*) malloc(sizeof(Packet));
    p->data = (u1*) malloc(rawbytes);
    p->offset = 0;
    p->length = rawbytes;

    return p;
}

