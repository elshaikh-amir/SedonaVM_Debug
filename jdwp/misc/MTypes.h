#ifndef _H_VAR
#define _H_VAR

//#include "../../svm/sedona.h"
#define LENGTH_POS 0                        // offset (shared)
#define LENGTH_SIZE 4                       // size   (shared)

#define ID_POS 4                            // offset (shared)
#define ID_SIZE 4                           // size   (shared)

#define FLAGS_POS 8                         // offset (shared)
#define FLAGS_SIZE 1                        // size   (shared)

#define COMMAND_SET_POS 9                   // offset (Command Packet specific)
#define COMMAND_SET_SIZE 1                  // size   (Command Packet specific)

#define COMMAND_POS 10                      // offset (Command Packet specific)
#define COMMAND_SIZE 1                      // size   (Command Packet specific)

#define ERROR_CODE_POS 9                    // offset (Reply Packet specific)
#define ERROR_CODE_SIZE 2                   // size   (Reply Packet specific)

#define DATA_VARIABLE_POS 11                // offset (shared), length is computed on rest length size

#define MIN_PACKET_SIZE 11                  // min size of a command packet

// define id sizes, also unused are defined for now (Writing it all down)
#define byte__SIZE 1                        // byte size                   (JDWP specific)
#define boolean__SIZE byte__SIZE            // boolean size                (JDWP specific)
#define int__SIZE 4                         // int size                    (JDWP specific)
#define long__SIZE 8                        // long size                   (JDWP specific)

#define short__SIZE 2                       // short SIZE                  (Custom       )

// UTF-8 char
#define char__SIZE short__SIZE              // char size                   (JDWP specific)

// can set custom object id(s) for living objects inside the VM, we use long size then
#define objectid__SIZE int__SIZE            // object id size              (Custom              )

// JDWP specific size, linked from object id and forward
#define tag__SIZE byte__SIZE                 // tag size                   (JDWP specific       )

#define taggedobjectid__SIZE (objectid__SIZE + tag__SIZE) // object id size + tag        (JDWP specific)
#define threadid__SIZE objectid__SIZE            // same as objectID            (JDWP specific       )
#define threadGroupID__SIZE objectid__SIZE      // same as objectID            (JDWP specific       )
#define stringID__SIZE objectid__SIZE           // same as objectID            (JDWP specific       )
#define classLoaderID__SIZE objectid__SIZE      // same as objectID            (JDWP specific       )
#define classObjectID__SIZE objectid__SIZE      // same as objectID            (JDWP specific       )
#define arrayID__SIZE objectid__SIZE            // same as objectID            (JDWP specific       )
#define referenceTypeID__SIZE objectid__SIZE    // same as objectID            (JDWP specific       )
#define classID__SIZE referenceTypeID__SIZE     // same as referenceTypeID     (JDWP specific       )
#define interfaceID__SIZE referenceTypeID__SIZE // same as referenceTypeID     (JDWP specific       )
#define arrayTypeID__SIZE referenceTypeID__SIZE // same as referenceTypeID     (JDWP specific       )

// can set custom id sizes for the following, we follow the sedona sizes then
// these id(s) identify class objects like methods and fields inside a class object
#define methodID__SIZE short__SIZE              // short size                  (Sedona   specific   )
#define fieldID__SIZE short__SIZE               // short size                  (Sedona   specific   )
#define frameID__SIZE int__SIZE                 // int size                    (SedonaVM specific   )

// bytes tag + bytes classID + bytes methodID + 8 bytes index
#define location__SIZE (tag__SIZE + classID__SIZE + methodID__SIZE + 8)     // (JDWP specific       )

// encoded int__SIZE length + actual size
#define string__SIZE(s) ((strlen(s) * char__SIZE) + int__SIZE) // (JDWP specific       )

// encoded int__SIZE + utf size
//#define utf__SIZE(s) (PacketHandler->utf_size(s))

// tag__SIZE bytes tag + string__SIZE
#define stringTAGGED__SIZE(s) (string__SIZE(s) + tag__SIZE) // (JDWP specific       )

// raw value size
#define value__SIZE(v) sizeof(v)                // (JDWP specific       )

// encoded bytes tag + actual value bytes
#define valueTAGGED__SIZE(v) (value__SIZE(v) + tag__SIZE)  // (JDWP specific       )

// defines the default reply flag
#define REPLY_FLAG 128

// define a non-reply flag
#define CMD_FLAG 0

// forward cell def
#ifndef CELL_TYPE
    #define CELL_TYPE
    typedef union
    {
      int32_t ival;    // 32-bit signed int
      float   fval;    // 32-bit float
      void*   aval;    // address pointer
    } Cell;
#endif

typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

#if fieldID__SIZE == short__SIZE
    typedef u2 fieldid_t;
#elif fieldID__SIZE == int__SIZE
    typedef u4 fieldid_t;
#else
    typedef u8 fieldid_t;
#endif

#if methodID__SIZE == short__SIZE
    typedef u2 methodid_t;
#elif methodID__SIZE == int__SIZE
    typedef u4 methodid_t;
#else
    typedef u8 methodid_t;
#endif

#if frameID__SIZE == short__SIZE
    typedef u2 frameid_t;
#elif frameID__SIZE == int__SIZE
    typedef u4 frameid_t;
#else
    typedef u8 frameid_t;
#endif // frameID__SIZE

#if objectid__SIZE == short__SIZE
    typedef u2 objectid_t;
    typedef u2 referencetypeid_t;
    typedef u2 threadgroupid_t;
    typedef u2 threadid_t;
#elif objectid__SIZE == int__SIZE
    typedef u4 objectid_t;
    typedef u4 referencetypeid_t;
    typedef u4 threadgroupid_t;
    typedef u4 threadid_t;
#else
    typedef u8 objectid_t;
    typedef u8 referencetypeid_t;
    typedef u8 threadgroupid_t;
    typedef u8 threadid_t;
#endif

#if tag__SIZE == byte__SIZE
    typedef u1 tagid_t;
#elif tag__SIZE == short__SIZE
    typedef u2 tagid_t;
#elif tag__SIZE == int__SIZE
    typedef u4 tagid_t;
#else
    typedef u8 tagid_t;
#endif

typedef struct {
    u8 lineCodeIndex;
    u4 lineNumber;
}
LineTable;

typedef struct {
    methodid_t mid;
    u1* methodName;
    u2* methodJNISignature;
    u4 modBits;

    // lineTable
    u4 lines; // count
    u8 startline;
    u8 endline;

    LineTable** lineTable;
}
method;

typedef struct {
    fieldid_t fid;
    u1* fieldName;
    u2* fieldJNIsignature;
    u4 modBits;
}
field;

// Reference type | Class
struct RefTypeID_s {
    referencetypeid_t ref_clazzid;
    u1 typeTag;

    u1* sourceFile;
    u1* clazzname;
    u2* signature;
    u4 modBits;
    u4 status;

    u2 max_interfaces;
    u2 max_fields;
    u2 max_methods;

    u2 num_interfaces;
    u2 num_fields;
    u2 num_methods;

    struct RefTypeID_s* superClazz;
    struct RefTypeID_s** interfaces;
    field** fields;
    method** methods;
};

typedef struct RefTypeID_s RefTypeID;

// Def of a location
typedef struct {
    tagid_t tag;
    objectid_t classID;
    methodid_t methodID;
    u8 index;
}
Location;

// Variable holder definition
typedef struct {
    // JDWP assigned unique identifier
    objectid_t v_id;

    // variable type
    u1 v_type;

    // if object -> is Class
    RefTypeID* ref;

    // Var size
    u4 v_size;

    // Meta-Assigned var name (Optional)
    u1* v_name;

    // Stack location
    Cell* v_stack;

    // location
    Location* loc;
}
Variable;

struct VariableBuff_s {
    struct VariableBuff_s* next;
    Variable* variable;
};

typedef struct VariableBuff_s VariableBuff;

// Var list def
typedef struct {
    VariableBuff* head;
    size_t size;
}
VariableList;

// EventRequest types moded according to ModKinds 1 - 12
typedef union {
    u1 modKind;

    struct {
        u1 modKind;
        u4 count;
    }
    count;

    struct {
        u1 modKind;
        u4 exprId;
    }
    conditional;

    struct {
        u1 modKind;
        threadid_t threadId;
    }
    threadOnly;

    struct {
        u1 modKind;
        referencetypeid_t refTypeId;
    }
    classOnly;

    struct {
        u1 modKind;
        u1* classPattern;
    }
    classMatch;

    struct {
        u1 modKind;
        u1* classPattern;
    }
    classExclude;

    struct {
        u1 modKind;
        Location loc;
    }
    locationOnly;

    struct {
        u1 modKind;
        u1 caught;
        u1 uncaught;
        referencetypeid_t   refTypeId;
    }
    exceptionOnly;

    struct {
        u1 modKind;
        referencetypeid_t refTypeId;
        fieldid_t fieldId;
    }
    fieldOnly;

    struct {
        u1 modKind;
        threadid_t threadId;
        u4 size;
        u4 depth;
    }
    step;

    struct {
        u1 modKind;
        objectid_t objectId;
    }
    instanceOnly;

    struct {
        u1 modKind;
        u1* sourceNamePattern;
    }
    sourceNameMatch;
}
ModKind;

typedef struct {
    u4 requestId;
    u1 eventKind;
    u1 suspendPolicy;
    u1 triggered;
    u1 isremoved;
    u4 modCount;

    ModKind mods[1];
}
EventRequest;

struct EventRequestBuff_s {
    struct EventRequestBuff_s* next;
    EventRequest* eventRequest;
};

typedef struct EventRequestBuff_s EventRequestBuff;

// EventRequest LinkedList
typedef struct {
    EventRequestBuff* head;
    size_t size;
}
EventRequestList;

// Defines cmd and reply packets
typedef struct {
    u1* data;
    size_t offset;
    size_t length; // yoloer
}
Packet;

// Defines a buffers list
struct PacketBuff_s {
    struct PacketBuff_s* next;

    Packet* packet;
};

typedef struct PacketBuff_s PacketBuff;

typedef struct {
    PacketBuff* head;
    size_t size;
}
PacketList;

// objectid buff object
struct refTypeIDBuff_s {
    struct refTypeIDBuff_s* next;
    RefTypeID* reftypeid;
};

typedef struct refTypeIDBuff_s refTypeIDBuff;

// objectid list for refID list
typedef struct {
    refTypeIDBuff* head;
    size_t size;
}
refTypeID_list;

struct u1Buff_s {
    struct u1Buff_s* next;
    u1 u1;
};

typedef struct u1Buff_s u1Buff;

typedef struct {
    u1Buff* head;
    size_t size;
}
u1List;

struct u1ListBuff_s {
    struct u1ListBuff_s* next;
    u1List* word;
};

typedef struct u1ListBuff_s u1ListBuff;

typedef struct {
    u1ListBuff* head;
    size_t size;
}
strBuilder;

typedef struct {
    size_t frameID;
    Cell* framePointer;
    Location* frameLocation;
}
FrameContainer;

#endif // _H_VAR
