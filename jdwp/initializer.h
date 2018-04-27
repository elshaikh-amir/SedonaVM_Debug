#ifndef _H_INT
#define _H_INT

#include "misc/MTypes.h"

extern void init__VM_DEBUG_MODE();

typedef struct {
    void (*onStackOverFlow) (int opcode, Cell* frame_p, Cell* stack_p);

    // special event handler, forwarded from vm code
    void (*onNullPointerException) (int opcode, Cell* frame_p, Cell* stack_p);

    // terminate on fatal error
    void (*error_exit) (char* error_message);

    // jdwp packet sender
    void (*jdwpSend) (Packet* packet);
}
HMainHandler;


typedef struct {
    u1* (*eventKind_to_cstr) (u1 eventKind);
    void (*dispatch_THREAD_START) (void);
    void (*dispatch_VM_INIT) (void);
    void (*add) (EventRequest* event);
    void (*remove) (u1 eventKind, u4 requestID);
    void (*removeAllEventKind) (u1 eventKind);
    EventRequest* (*getByRequestID) (u4 requestID);
    void (*dispatch) (EventRequest* event);
    void (*dispatchAll) (void);
    void (*holdEvents) (u1 holder);
    u1 (*canDispatchAll) (void);
    EventRequest* (*newEvent) (void);
    void (*check_forEvents) (u1 scode, Cell* sp, Cell* fp, methodid_t method);
}
HEventDispatcher;


typedef struct {
    void (*add_field) (RefTypeID* typeID, field* field);
    void (*add_method) (RefTypeID* typeID, method* method);
    void (*set_superClass) (RefTypeID* typeID, RefTypeID* superclazz);
    void (*add_interface) (RefTypeID* typeID, RefTypeID* interface2);

    RefTypeID* (*find_ByClazzname) (u1* cname);
    RefTypeID* (*find_ByID) (referencetypeid_t rID);
    RefTypeID* (*find_BySignature) (u2* signature);

    PacketList* (*methods_to_PacketList) (RefTypeID* clazz); // raw bytes
    PacketList* (*fields_to_PacketList) (RefTypeID* clazz); // raw bytes

    PacketList* (*fields_WithGeneric_to_PacketList) (RefTypeID* clazz); // raw bytes
    PacketList* (*methods_WithGeneric_to_PacketList) (RefTypeID* clazz); // raw bytes

    size_t (*allClassesCount) (void);
    PacketList* (*allClazzRefs_to_PacketList) (void);
    PacketList* (*allClazzRefsWithGenetrics_to_PacketList) (void);

    method* (*get_method) (referencetypeid_t rtid, methodid_t mid);

    //RefTypeID* (*add_ref) (u1* clazzname, u2* signature);
    void (*add_startedOf) (RefTypeID* of);
    RefTypeID* (*add_Classref) (u1* clazzname, RefTypeID* superclass);
    RefTypeID* (*add_Arrayref) (u1* clazzname, RefTypeID* superclass);
    RefTypeID* (*add_Interfaceref) (u1* clazzname, RefTypeID* superclass);

    void (*setStartClassRef) (RefTypeID* ref);
    RefTypeID* (*getStartRef) (void);

    void (*add_RefTypeID) (RefTypeID* typeID);
}
HRefHandler;


typedef struct {
    // creators
    Packet* (*newReplyPacket) (u4 id, size_t bytes); // handler's id
    Packet* (*newCommandPacket) (size_t bytes, u1 command, u1 commandset);
    Packet* (*newPacketFromHeader) (u1* header);
    Packet* (*newPacketFromHeaderPayload) (u1* header, u1* payload);
    Packet* (*newRawPacket) (size_t rawbytes);
    u4 (*utf_csize) (u1* str);
    u4 (*utf_size) (u2* utf);

    // custom list functions
    void (*add_Packet) (Packet* p, PacketList* plist);
    size_t (*get_ByteSize) (PacketList* plist);
    void (*write_packet) (Packet* to, Packet* from);
    Packet* (*write_list) (Packet* p, PacketList* plist);

    // basic byte readers
    u1 (*read_u1) (Packet* p);
    u2 (*read_u2) (Packet* p);
    u4 (*read_u4) (Packet* p);
    u8 (*read_u8) (Packet* p);

    // helpers
    u2 (*read_u2_buff) (u1* buff);
    u4 (*read_u4_buff) (u1* buff);
    u8 (*read_u8_buff) (u1* buff);

    // explicit info readers
    u4 (*read_length) (Packet* p);
    u4 (*read_id) (Packet* p);
    u1 (*read_flags) (Packet* p);
    u2 (*read_errorcode) (Packet* p);

    // explicit command readers
    u1 (*read_command) (Packet* p);
    u1 (*read_commandset) (Packet* p);

    // jdwp specific readers
    tagid_t (*read_tag) (Packet* p);
    methodid_t (*read_methodid) (Packet* p);
    fieldid_t (*read_fieldid) (Packet* p);
    objectid_t (*read_objectid) (Packet* p);
    threadid_t (*read_threadid) (Packet* p);
    threadgroupid_t (*read_threadgroupid) (Packet* p);
    frameid_t (*read_frameid) (Packet* p);
    Location* (*read_location) (Packet* p);
    u1* (*read_str) (Packet* p);
    u2* (*read_utf) (Packet* p);

    // basic byte writers
    void (*write_u1) (Packet* p, u1 u1);
    void (*write_u2) (Packet* p, u2 u2);
    void (*write_u4) (Packet* p, u4 u4);
    void (*write_u8) (Packet* p, u8 u8);

    // jdwp specific writers

    // jdwp header
    void (*write_length) (Packet* p, u4 u4);
    void (*write_command) (Packet* p, u1 u1);
    void (*write_commandset) (Packet* p, u1 u1);
    void (*write_id) (Packet* p, u4 u4);
    void (*write_flags) (Packet* p, u1 u1);
    void (*write_errorcode) (Packet* p, u2 u2);

    // data variable
    void (*write_tag) (Packet* p, tagid_t t);
    void (*write_methodid) (Packet* p, methodid_t mid);
    void (*write_fieldid) (Packet* p, fieldid_t fid);
    void (*write_objectid) (Packet* p, objectid_t oid);
    void (*write_threadid) (Packet* p, threadid_t tid);
    void (*write_threadgroupid) (Packet* p, threadgroupid_t tgid);
    void (*write_frameid) (Packet* p, frameid_t frid);
    void (*write_location) (Packet* p, Location* loc);
    void (*write_str) (Packet* p, u1* str);
    void (*write_cutf) (Packet* p, u1* str);
    void (*write_utf) (Packet* p, u2* utf);
    u1 (*isPacketReply) (Packet* p);
    u1 (*isPacketCommand) (Packet* p);

}
HPacketHandler;


typedef struct {
    void (*dispose) (void);
    // returns variable with id
    Variable* (*get) (objectid_t id);

    // returns TRUE if v defines a primitive type | BufTypeID is *NOT* primitive "cheating" here
    u1 (*isPrimitive) (Variable* v);

    // returns TRUE if v defines any types of type array, only array, not object!
    u1 (*isArray) (Variable* v);

    // returns TRUE if v defines an object | only object, not array!
    u1 (*isObject) (Variable* v);

    // returns array 1 byte primitive values
    u1* (*getArrayValues_u1) (Variable* v);

    // returns array 2 bytes primitive values
    u2* (*getArrayValues_u2) (Variable* v);

    // returns array 4 bytes primitive values
    u4* (*getArrayValues_u4) (Variable* v);

    // returns array 8 bytes primitive values
    u8* (*getArrayValues_u8) (Variable* v);

    // returns 1 byte primitive value
    u1 (*getValue_u1) (Variable* v);

    // returns 2 bytes primitive value
    u2 (*getValue_u2) (Variable* v);

    // returns 4 bytes primitive value
    u4 (*getValue_u4) (Variable* v);

    // returns 8 bytes primitive value
    u8 (*getValue_u8) (Variable* v);
}
HVariableHandler;

typedef struct {
    u1 (*canExecute) (void);
    void (*setCanExecute) (u1 canExecute);
    u1 (*suspendCount) (void);
    threadid_t (*pid) (void);
    threadid_t (*app_tid) (void);
    void (*exit) (void);
    void (*sleep) (long s);
    void (*suspend) (void);
    void (*dispose) (void);
    void (*resume) (void);
    u1 (*isSuspended) (void);
    u1 (*getCurrentByteCode) (void);
    void (*force_ExitMethod) (Cell* value);
    u1 (*is_nativeMethod) (void);
    Cell* (*invoke_Method) (methodid_t mid, Cell* args, int argc);
    u2 (*get_frameCount) (void);
    FrameContainer** (*getAllFrames) (void);
}
HVMHandler;

typedef struct {
    size_t (*castToInt) (u1* str);
    u2* (*mku2) (u1* str);
    u1* (*readFileRaw) (u1* filename);
    u1** (*readFileLines) (u1* filename);
    u1 (*isSpacer) (u1 c);
    u1* (*substr) (u1* str, size_t from, size_t cnt);
    int64_t (*indexOf) (u1* str, u1 c);
    size_t (*countChar) (u1* str, u1 c);
    u1** (*split) (u1* str, u1 dil);
    u1* (*u2Tou1) (u2* s2);
}
MFManager;


// Globals
extern HMainHandler* MHandler;
extern HEventDispatcher* EventHandler;
extern HRefHandler* RefHandler;
extern HPacketHandler* PacketHandler;
extern HVariableHandler* ObjectHandler;
extern HVMHandler* VMController;
extern MFManager* MFileManager;

#endif // _H_INT
