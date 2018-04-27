//#define LOG_PACKETS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "misc/MConstants.h"
#include "misc/MTypes.h"
#include "misc/MEventkind.h"
#include "commandsets/MCommandsets.h"

#include "commandsets/VirtualMachine.h"
#include "commandsets/ReferenceType.h"
#include "commandsets/ClassType.h"
#include "commandsets/ArrayType.h"
#include "commandsets/Method.h"
#include "commandsets/ObjectReference.h"
#include "commandsets/StringReference.h"
#include "commandsets/ThreadReference.h"
#include "commandsets/ThreadGroupReference.h"
#include "commandsets/ArrayReference.h"
#include "commandsets/ClassLoaderReference.h"
#include "commandsets/EventRequest.h"
#include "commandsets/Event.h"

#include "initializer.h"

#ifdef _WIN32
    #include <winsock.h>
    #include <io.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#define JDWP_INIT_WAIT 1                    // halt check delay

#define LOCAL_IP "127.0.0.1"                // output purpose "local" or "remote"
#define PORT 8000                           // default JDWP

#define HANDSHAKE_SIZE  14                  // default JDWP
#define HANDSHAKE_STR "JDWP-Handshake"      // default JDWP ASCII - 1 byte per char, hence 14 size byte total string

// internal forwards
#ifdef LOG_PACKETS
    static void logPacket(Packet* p);
#endif // LOG_PACKETS
static void release_jdwpInitHalt();
static void jdwpSend_error(u2 errorcode);
static void sleep(long s);
static void close_socket();
// special event handler, forwarded from vm code
static void onStackOverFlow(int opcode, Cell* frame_p, Cell* stack_p);

// special event handler, forwarded from vm code
static void onNullPointerException(int opcode, Cell* frame_p, Cell* stack_p);

// terminate on fatal error
static void error_exit(char* error_message);

// jdwp packet sender
static void jdwpSend(Packet* packet);

// constants used in scope
const static u1 THREAD_COUNT            = 4; // 3 here, 1 magic jni
const static u1 THREAD_GROUP_COUNT      = 2;

const static u1 THREAD_GROUP_MAIN_COUNT = 2; // tgid for pid, jdwp tid and 2 magic jni
const static u1 THREAD_GROUP_SUB_COUNT  = 1;

const static u1 THREAD_GROUP_MAIN_TGID  = 1;
const static u1 THREAD_GROUP_SUB_TGID   = 2;

// Thread constants
const static u1* MAIN_THREADGROUP_NAME  = (u1*) "Main_ThreadGroup";
const static u1* SUB_THREADGROUP_NAME   = (u1*) "Sub_ThreadGroup";

const static u1* MAIN_THREAD_NAME       = (u1*) "SVM_Process";
const static u1* JDWP_THREAD_NAME       = (u1*) "JDWP_Thread";
const static u1* MAIN_APPLICATION_NAME  = (u1*) "APP_Thread";
const static u1* JNI_THREAD_NAME        = (u1*) "JNI_Thread";

// CS VM VERSION constants to send
const u1* __CS_VM_VERSION_description   = (u1*) "Sedona VM_DEBUG_MODE v1.28d";
const u1  __CS_VM_VERSION_jdwpMajor     = 1;
const u1  __CS_VM_VERSION_jdwpMinor     = 1;
const u1* __CS_VM_VERSION_vmVersion     = (u1*) "1.28";
const u1* __CS_VM_VERSION_vmName        = (u1*) "SedonaVM";

// Internal vars
static Packet* _MPacket;                        // pointer to oncommand recv packet
static Packet* _PACKET_ERROR;                   // constant packet error var
static pthread_t THREAD_RECV;                   // ref for thread start
static volatile u1 init_jdwp;              // waiter till vm can be launched
static threadid_t JDWP_TID              = 0; // thread id of jdwp
static threadid_t JNI_TID               = 0;

#ifdef _WIN32
    static SOCKET _SERVERSOCKET;
    static SOCKET _CLIENTSOCKET;
    static void close_socket() {
        closesocket(_CLIENTSOCKET);
    }
    static void sleep(long s) {
        Sleep(s);
    }
#else
    static int _SERVERSOCKET;
    static int _CLIENTSOCKET;
    static void close_socket() {
        close(_CLIENTSOCKET);
    }
#endif

// packet handler functions forwards for server usage
inline static u1 readu1() {
    return PacketHandler->read_u1(_MPacket);
}

inline static u2 readu2() {
    return PacketHandler->read_u2(_MPacket);
}

inline static u4 readu4() {
    return PacketHandler->read_u4(_MPacket);
}

inline static uint64_t readu8() {
    return PacketHandler->read_u8(_MPacket);
}

inline static u4 readlength() {
    return PacketHandler->read_length(_MPacket);
}

inline static u4 readid() {
    return PacketHandler->read_id(_MPacket);
}

inline static u1 readflags() {
    return PacketHandler->read_flags(_MPacket);
}

inline static u2 readerrorcode() {
    return PacketHandler->read_errorcode(_MPacket);
}

inline static u1 readcommand() {
    return PacketHandler->read_command(_MPacket);
}

inline static u1 readcommandset() {
    return PacketHandler->read_commandset(_MPacket);
}

inline static tagid_t readtag() {
    return PacketHandler->read_tag(_MPacket);
}

inline static methodid_t readmethodid() {
    return PacketHandler->read_methodid(_MPacket);
}

inline static fieldid_t readfieldid() {
    return PacketHandler->read_fieldid(_MPacket);
}
inline static objectid_t readobjectid() {
    return PacketHandler->read_objectid(_MPacket);
}

inline static threadid_t readthreadid() {
    return PacketHandler->read_threadid(_MPacket);
}

inline static threadgroupid_t readthreadgroupid() {
    return PacketHandler->read_threadgroupid(_MPacket);
}

inline static Location* readlocation() {
    return PacketHandler->read_location(_MPacket);
}

inline static u1* readstr() {
    return PacketHandler->read_str(_MPacket);
}

inline static u2* readutf() {
    return PacketHandler->read_utf(_MPacket);
}

inline static Packet* newReply(size_t bytes) {
    return PacketHandler->newReplyPacket(readid(), bytes);
}

inline static void jdwpSend_Not_Implemented() {
    jdwpSend_error(NOT_IMPLEMENTED);
}

// START - CS-Handles: ######################## Virtual Machine ########################

// Sends the JDWP version implemented by the target VM
inline static void onSend_CS_VM_Version() {
    printf("onSend_CS_VM_Version()\n");

    size_t psize = string__SIZE(__CS_VM_VERSION_description) +
                   (int__SIZE << 1) +
                   string__SIZE(__CS_VM_VERSION_vmVersion) +
                   string__SIZE(__CS_VM_VERSION_vmName);

    Packet* p = newReply(psize);
    PacketHandler->write_str(p, __CS_VM_VERSION_description);
    PacketHandler->write_u4 (p, __CS_VM_VERSION_jdwpMajor);
    PacketHandler->write_u4 (p, __CS_VM_VERSION_jdwpMinor);
    PacketHandler->write_str(p, __CS_VM_VERSION_vmVersion);
    PacketHandler->write_str(p, __CS_VM_VERSION_vmName);

    jdwpSend(p);
}

// Sends reference types for all the classes loaded by the target VM which match the given signature.
// Multiple reference types will be returned if two or more class loaders have loaded a class of the same name.
// The search is confined to loaded classes only; no attempt is made to load a class of the given signature.
inline static void onSend_CS_VM_ClassesBySignature() { // I need to figure out if this is needed first!
    printf("onSend_CS_VM_ClassesBySignature()\n");

    Packet* p;
    u1* signstr = readstr();

    RefTypeID* typeID = RefHandler->find_BySignature(signstr);
    if(typeID == NULL) {
        printf("NOT found class = %s\n", signstr);
        p = newReply(int__SIZE);
        PacketHandler->write_u4(p, 0);
    }
    else {
        printf("found class = %s, clazzid = %i\n", typeID->clazzname, typeID->ref_clazzid);
        p = newReply(int__SIZE + byte__SIZE + referenceTypeID__SIZE + int__SIZE);
        PacketHandler->write_u4(p, 1); // classes of signature
        PacketHandler->write_tag(p, TYPETAG_CLASS); // no interfaces, just classes
        PacketHandler->write_objectid(p, typeID->ref_clazzid);
        PacketHandler->write_u4(p, typeID->status);
    }

    jdwpSend(p);
}

// Sends reference types for all classes currently loaded by the target VM.
inline static void onSend_CS_VM_AllClasses() { // I need to figure out if this is needed first!
    printf("onSend_CS_VM_AllClasses()\n");

    PacketList* clazzes = RefHandler->allClazzRefs_to_PacketList();
    Packet* p = newReply(PacketHandler->get_ByteSize(clazzes) + int__SIZE);

    PacketHandler->write_u4(p, clazzes->size);
    PacketHandler->write_list(p, clazzes);

    jdwpSend(p);
}


// Sends all threads currently running in the target VM, No JNI natives or Multi threading
inline static void onSend_CS_VM_AllThreads() {
    printf("onSend_CS_VM_AllThreads()\n");

    Packet* p = newReply(int__SIZE + threadid__SIZE * THREAD_COUNT);

    PacketHandler->write_u4(p, THREAD_COUNT);
    PacketHandler->write_threadid(p, VMController->pid());
    PacketHandler->write_threadid(p, JDWP_TID);

    PacketHandler->write_threadid(p, VMController->app_tid());

    if(JNI_TID != 0) { // a weirdo. i force a creation by tracking it from tgr
        PacketHandler->write_threadid(p, JNI_TID);
    }

    jdwpSend(p);
}

inline static void onSend_CS_VM_TopLevelThreadGroups() {
    printf("onSend_CS_VM_TopLevelThreadGroups()\n");

    Packet* p = newReply(int__SIZE + threadGroupID__SIZE * THREAD_GROUP_COUNT);

    PacketHandler->write_u4(p, THREAD_GROUP_COUNT);
    PacketHandler->write_threadgroupid(p, THREAD_GROUP_MAIN_TGID);
    PacketHandler->write_threadgroupid(p, THREAD_GROUP_SUB_TGID);

    jdwpSend(p);
}

inline static void onSend_CS_VM_Dispose() { // Invalidates this virtual machine mirror
    printf("onSend_CS_VM_Dispose()\n");
    VMController->dispose();
    error_exit(EXIT_SUCCESS);
}

inline static void onSend_CS_VM_IDSizes() { // Sends id sizes for fields, methods, etc
    printf("onSend_CS_VM_IDSizes()\n");

    // Order: fieldID, methodID, objectID, referenceTypeID, frameID
    Packet* p = newReply(int__SIZE * 5);
    PacketHandler->write_u4(p, fieldID__SIZE);
    PacketHandler->write_u4(p, methodID__SIZE);
    PacketHandler->write_u4(p, objectid__SIZE);
    PacketHandler->write_u4(p, referenceTypeID__SIZE);
    PacketHandler->write_u4(p, frameID__SIZE);

    jdwpSend(p);
}

// Suspends the execution of the application running in the target VM
// All threads currently running will be suspended
inline static void onSend_CS_VM_Suspend() {
    printf("onSend_CS_VM_Suspend()\n");
    VMController->suspend();
    jdwpSend(newReply(0));
}

// Resumes execution of the application after the suspend command or an event has stopped it
// Suspensions of the Virtual Machine and individual threads are counted
// If a particular thread is suspended n times, it must resumed n times before it will continue
inline static void onSend_CS_VM_Resume() {
    printf("onSend_CS_VM_Resume()\n");
    jdwpSend(newReply(0));
    VMController->resume();
}

// Terminates the target VM with the given exit code
inline static void onSend_CS_VM_Exit() {
    printf("onSend_CS_VM_Exit()\n");

    Packet* p = newReply(int__SIZE);
    PacketHandler->write_u4(p, 0);

    jdwpSend(p);

    VMController->exit();
}

// Injection, doesn't look needed at all
inline static void onSend_CS_VM_CreateString() {
    printf("onSend_CS_VM_CreateString()\n");

    jdwpSend_Not_Implemented();
}

// apparently this is very nice for monitoring vars
// sadly, i think this requires the jnit or whatever this tooling interface is called :/
inline static void onSend_CS_VM_Capabilities() {
    printf("onSend_CS_VM_Capabilities()\n");

    Packet* p = newReply(7);

    PacketHandler->write_u1(p, FALSE); // canWatchFieldModification
    PacketHandler->write_u1(p, FALSE); // canWatchFieldAccess
    PacketHandler->write_u1(p, FALSE); // canGetBytecodes
    PacketHandler->write_u1(p, TRUE); // canGetSyntheticAttribute
    PacketHandler->write_u1(p, FALSE); // canGetOwnedMonitorInfo
    PacketHandler->write_u1(p, FALSE); // canGetCurrentContendedMonitor
    PacketHandler->write_u1(p, FALSE); // canGetMonitorInfo

    jdwpSend(p);
}

// list of classpath, bootclasspath
inline static void onSend_CS_VM_ClassPaths() {
    printf("onSend_CS_VM_ClassPaths()\n");

    // TODO: impl.
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_VM_DisposeObjects() {
    printf("onSend_CS_VM_DisposeObjects()\n");

    ObjectHandler->dispose();
}

inline static void onSend_CS_VM_HoldEvents() {
    printf("onSend_CS_VM_HoldEvents()\n");

    EventHandler->holdEvents(TRUE);
}

inline static void onSend_CS_VM_ReleaseEvents() {
    printf("onSend_CS_VM_ReleaseEvents()\n");

    EventHandler->holdEvents(FALSE);
}

// Since JDWP version 1.4
inline static void onSend_CS_VM_CapabilitiesNew() {
    printf("onSend_CS_VM_CapabilitiesNew()\n");

    Packet* p = newReply(32);

    PacketHandler->write_u1(p, FALSE); // canWatchFieldModification
    PacketHandler->write_u1(p, FALSE); // canWatchFieldAccess
    PacketHandler->write_u1(p, FALSE); // canGetBytecodes
    PacketHandler->write_u1(p, TRUE); // canGetSyntheticAttribute
    PacketHandler->write_u1(p, FALSE); // canGetOwnedMonitorInfo
    PacketHandler->write_u1(p, FALSE); // canGetCurrentContendedMonitor
    PacketHandler->write_u1(p, FALSE); // canGetMonitorInfo

    // start new ones
    PacketHandler->write_u1(p, FALSE); // Can the VM redefine classes?
    PacketHandler->write_u1(p, FALSE); // Can the VM add methods when redefining classes?
    PacketHandler->write_u1(p, FALSE); // Can the VM redefine classesin arbitrary ways?
    PacketHandler->write_u1(p, FALSE); // Can the VM pop stack frames?
    PacketHandler->write_u1(p, FALSE); // Can the VM filter events by specific object?
    PacketHandler->write_u1(p, FALSE); // Can the VM get the source debug extension?
    PacketHandler->write_u1(p, FALSE); // Can the VM request VM death events?
    PacketHandler->write_u1(p, FALSE); // Can the VM set a default stratum?
    PacketHandler->write_u1(p, FALSE); // Can the VM return instances, counts of instances of classes and referring objects?
    PacketHandler->write_u1(p, FALSE); // Can the VM request monitor events?
    PacketHandler->write_u1(p, FALSE); // Can the VM get monitors with frame depth info?
    PacketHandler->write_u1(p, FALSE); // Can the VM filter class prepare events by source name?
    PacketHandler->write_u1(p, FALSE); // Can the VM return the constant pool information?
    PacketHandler->write_u1(p, FALSE); // Can the VM force early return from a method?

    // 11 ones reserved for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved22 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved23 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved24 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved25 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved26 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved27 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved28 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved29 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved30 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved31 for future capability
    PacketHandler->write_u1(p, FALSE); // Reserved32 for future capability

    jdwpSend(p);
}

// Injection, this is so risky to allow new classes, we wont support this
// Disabled by CapabilitiesNew
// This should not be sent!
inline static void onSend_CS_VM_RedefineClasses() {
    printf("onSend_CS_VM_RedefineClasses()\n");

    jdwpSend_Not_Implemented(); // error code NOT defined
}

// Disabled by CapabilitiesNew
// This should not be sent!
inline static void onSend_CS_VM_SetDefaultStratum() {
    printf("onSend_CS_VM_SetDefaultStratum()\n");

    jdwpSend_Not_Implemented(); // error code NOT defined
}

// This is java vm specific, and i believe its not required at all (generics)
// This should not be sent!
inline static void onSend_CS_VM_AllClassesWithGeneric() {
    printf("onSend_CS_VM_AllClassesWithGeneric()\n");

    PacketList* clazzes = RefHandler->allClazzRefsWithGenetrics_to_PacketList();
    Packet* p = newReply(PacketHandler->get_ByteSize(clazzes) + int__SIZE);

    PacketHandler->write_u4(p, clazzes->size);
    PacketHandler->write_list(p, clazzes);

    jdwpSend(p);
}

// Disabled by CapabilitiesNew
// This should not be sent!
inline static void onSend_CS_VM_InstanceCounts() {
    printf("onSend_CS_VM_InstanceCounts()\n");

    jdwpSend_Not_Implemented(); // No error code defined
}

// On unknown commands in "Virtual Machine" Command Set
inline static void onSend_CS_VM_UnknownCommand() {
    printf("onSend_CS_VM_UnknownCommand()\n");

    jdwpSend_Not_Implemented(); // Set default error code on not implemented
}

// END - CS-Handles: ######################## Virtual Machine ########################

// Command Set "Virtual Machine" Main Handler
inline static void onCSVirtualMachine() {
    printf("onCSVirtualMachine\n");

    switch(readcommand()) {
        case VMVERSION:
            onSend_CS_VM_Version();
            break;
        case VMClassesBySignature:
            onSend_CS_VM_ClassesBySignature();
            break;
        case VMAllClasses:
            onSend_CS_VM_AllClasses();
            break;
        case VMAllThreads:
            onSend_CS_VM_AllThreads();
            break;
        case VMTopLevelThreadGroups:
            onSend_CS_VM_TopLevelThreadGroups();
            break;
        case VMIDSizes:
            onSend_CS_VM_IDSizes();
            break;
        case VMDispose:
            onSend_CS_VM_Dispose();
            break;
        case VMSuspend:
            onSend_CS_VM_Suspend();
            break;
        case VMResume:
            onSend_CS_VM_Resume();
            break;
        case VMExit:
            onSend_CS_VM_Exit();
            break;
        case VMCreateString:
            onSend_CS_VM_CreateString();
            break;
        case VMCapabilities:
            onSend_CS_VM_Capabilities();
            break;
        case VMClassPaths:
            onSend_CS_VM_ClassPaths();
            break;
        case VMDisposeObjects:
            onSend_CS_VM_DisposeObjects();
            break;
        case VMHoldEvents:
            onSend_CS_VM_HoldEvents();
            break;
        case VMReleaseEvents:
            onSend_CS_VM_ReleaseEvents();
            break;
        case VMCapabilitiesNew:
            onSend_CS_VM_CapabilitiesNew();
            break;
        case VMRedefineClasses:
            onSend_CS_VM_RedefineClasses();
            break;
        case VMSetDefaultStratum:
            onSend_CS_VM_SetDefaultStratum();
            break;
        case VMAllClassesWithGeneric:
            onSend_CS_VM_AllClassesWithGeneric();
            break;
        case VMInstanceCounts:
            onSend_CS_VM_InstanceCounts();
            break;
        default:
            onSend_CS_VM_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## ReferenceType ########################
inline static void onSend_CS_RT_UnknownCommand() {
    printf("onSend_CS_RT_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// Sends the JNI signature of a reference type
inline static void onSend_CS_RT_Signature() {
    printf("onSend_CS_RT_Signature\n");

    referencetypeid_t rtid = readobjectid();
    RefTypeID* typeID = RefHandler->find_ByID(rtid);

    printf("found typeID = %s\n", typeID->clazzname);
    Packet* p = newReply(PacketHandler->utf_size(typeID->signature));
    PacketHandler->write_utf(p, typeID->signature);

    jdwpSend(p);
}

inline static void onSend_CS_RT_ClassLoader() {
    printf("onSend_CS_RT_ClassLoader\n");

    // TODO: impl. (need to meet prof. bockisch for these stuff)
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_RT_Modifiers() {
    printf("onSend_CS_RT_Modifiers\n");

    referencetypeid_t rtid = readobjectid();
    RefTypeID* typeID = RefHandler->find_ByID(rtid);

    if(typeID == NULL) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p = newReply(int__SIZE);
    PacketHandler->write_u4(p, typeID->modBits);
    jdwpSend(p);
}

inline static void onSend_CS_RT_Fields() {
    printf("onSend_CS_RT_Fields\n");

    referencetypeid_t rtid = readobjectid();
    RefTypeID* typeID = RefHandler->find_ByID(rtid);

    if(typeID == NULL) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p;
    if(typeID->num_fields == 0) {
        p = newReply(int__SIZE);
        PacketHandler->write_u4(p, 0);
    }
    else {
        // raw bytes
        PacketList* fieldBytes = RefHandler->fields_to_PacketList(typeID);
        // int num of declared fields
        p = newReply(int__SIZE + PacketHandler->get_ByteSize(fieldBytes));
        PacketHandler->write_u4(p, typeID->num_fields);
        PacketHandler->write_list(p, fieldBytes);
    }

    jdwpSend(p);
}

inline static void onSend_CS_RT_Methods() {
    printf("onSend_CS_RT_Methods\n");

    referencetypeid_t rtid  = readobjectid();
    RefTypeID* typeID       = RefHandler->find_ByID(rtid);

    if(typeID == NULL) {
        jdwpSend_error(INVALID_CLASS);
        return;
    }

    Packet* p = NULL;
    if(typeID->num_methods == 0) {
        p = newReply(int__SIZE);
        PacketHandler->write_u4(p, 0);
    }
    else {
        PacketList* methodRawBytes = RefHandler->methods_to_PacketList(typeID);
        p = newReply(int__SIZE + PacketHandler->get_ByteSize(methodRawBytes));
        PacketHandler->write_u4(p, typeID->num_methods);
        PacketHandler->write_list(p, methodRawBytes);
    }

    jdwpSend(p);
}

inline static void onSend_CS_RT_Values() {
    printf("onSend_CS_RT_Values\n");

    referencetypeid_t rtid  = readobjectid();
    u4 fields         = readu4();
    fieldid_t* field_ids    = (fieldid_t*) malloc(fieldID__SIZE * fields);
    u4 start          = 0;

    while(start++ < fields) {
        *field_ids = readfieldid();
        field_ids++;
    }

    RefTypeID* typeID = RefHandler->find_ByID(rtid);
    // variablehandler retrieves the values of fields! TODO :)

}

inline static void onSend_CS_RT_NestedTypes() {
    printf("onSend_CS_RT_NestedTypes\n");
    // no nested classes
    jdwpSend_error(INVALID_CLASS);
}

inline static void onSend_CS_RT_Interfaces() {
    printf("onSend_CS_RT_Interfaces\n");

    referencetypeid_t rtid = readobjectid();
    RefTypeID* typeID = RefHandler->find_ByID(rtid);

    if(typeID == NULL) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p;
    u2 interfaces = typeID->num_interfaces;
    if(interfaces == 0) {
        p = newReply(int__SIZE);
        PacketHandler->write_u4(p, 0);
    }
    else {
        p = newReply(referenceTypeID__SIZE * interfaces + int__SIZE);
        PacketHandler->write_u4(p, interfaces);

        u2 start = 0;
        while(start < interfaces) {
            PacketHandler->write_objectid(p, typeID->interfaces[start]->ref_clazzid);
            start++;
        }
    }

    jdwpSend(p);
}

inline static void onSend_CS_RT_ClassObject() {
    printf("onSend_CS_RT_ClassObject\n");

    referencetypeid_t rtid = readobjectid();
    RefTypeID* typeID      = RefHandler->find_ByID(rtid);

    if(typeID == NULL) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p = newReply(referenceTypeID__SIZE);
    PacketHandler->write_objectid(p, typeID->ref_clazzid);

    jdwpSend(p);
}

inline static void onSend_CS_RT_SourceDebugExtension() {
    printf("onSend_CS_RT_SourceDebugExtension\n");
    // no extension | capability disables this anyway
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_RT_SignatureWithGeneric() {
    printf("onSend_CS_RT_SignatureWithGeneric\n");

    referencetypeid_t rtid  = readobjectid();
    RefTypeID* typeID       = RefHandler->find_ByID(rtid);

    Packet* p = newReply(PacketHandler->utf_size(typeID->signature) + int__SIZE);
    PacketHandler->write_utf(p, typeID->signature);

    // always assume there is no generic!
    PacketHandler->write_u4(p, 0); // empty string | generic signature

    jdwpSend(p);
}

inline static void onSend_CS_RT_FieldsWithGeneric() {
    printf("onSend_CS_RT_FieldsWithGeneric\n");

    referencetypeid_t rtid  = readobjectid();
    RefTypeID* typeID       = RefHandler->find_ByID(rtid);

    PacketList* pList = RefHandler->fields_WithGeneric_to_PacketList(typeID);
    Packet* p  = newReply(PacketHandler->get_ByteSize(pList));
    PacketHandler->write_list(p, pList);

    jdwpSend(p);
}

inline static void onSend_CS_RT_MethodsWithGeneric() {
    printf("onSend_CS_RT_MethodsWithGeneric\n");

    referencetypeid_t rtid  = readobjectid();
    RefTypeID* typeID       = RefHandler->find_ByID(rtid);

    PacketList* pList = RefHandler->methods_WithGeneric_to_PacketList(typeID);
    Packet* p  = newReply(PacketHandler->get_ByteSize(pList));
    PacketHandler->write_list(p, pList);

    jdwpSend(p);
}

inline static void onSend_CS_RT_Instances() {
    printf("onSend_CS_RT_Instances\n");

    // disabled by capabilities
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_RT_ClassFileVersion() {
    printf("onSend_CS_RT_ClassFileVersion\n");

    Packet* p = newReply(int__SIZE << 1);
    PacketHandler->write_u4(p, __CS_VM_VERSION_jdwpMajor);
    PacketHandler->write_u4(p, __CS_VM_VERSION_jdwpMinor);

    jdwpSend(p);
}

inline static void onSend_CS_RT_ConstantPool() {
    printf("onSend_CS_RT_ConstantPool\n");

    // disabled by capabilities
    jdwpSend_Not_Implemented();
}

// END - CS-Handles: ######################## ReferenceType ########################

// Command Set "ReferenceType" Main Handler
inline static void onCSReferenceType() {
    printf("onCSReferenceType\n");

     switch(readcommand()) {
        case RTSignature:
            onSend_CS_RT_Signature();
            break;
        case RTClassLoader:
            onSend_CS_RT_ClassLoader();
            break;
        case RTModifiers:
            onSend_CS_RT_Modifiers();
            break;
        case RTFields:
            onSend_CS_RT_Fields();
            break;
        case RTMethods:
            onSend_CS_RT_Methods();
            break;
        case RTGetValues:
            onSend_CS_RT_Values();
            break;
        case RTNestedTypes:
            onSend_CS_RT_NestedTypes();
            break;
        case RTInterfaces:
            onSend_CS_RT_Interfaces();
            break;
        case RTClassObject:
            onSend_CS_RT_ClassObject();
            break;
        case RTSourceDebugExtension:
            onSend_CS_RT_SourceDebugExtension();
            break;
        case RTSignatureWithGeneric:
            onSend_CS_RT_SignatureWithGeneric();
            break;
        case RTFieldsWithGeneric:
            onSend_CS_RT_FieldsWithGeneric();
            break;
        case RTMethodsWithGeneric:
            onSend_CS_RT_MethodsWithGeneric();
            break;
        case RTInstances:
            onSend_CS_RT_Instances();
            break;
        case RTClassFileVersion:
            onSend_CS_RT_ClassFileVersion();
            break;
        case RTConstantPool:
            onSend_CS_RT_ConstantPool();
            break;
        default:
            onSend_CS_RT_UnknownCommand();
            break;
     }
}

// START - CS-Handles: ######################## ClassType ########################
inline static void onSend_CS_CT_SuperClass() {
    printf("onSend_CS_CT_SuperClass\n");

    referencetypeid_t clazzid = readobjectid(); // check if has superclass
    RefTypeID* typeID = RefHandler->find_ByID(clazzid);

    if(typeID == NULL) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p = newReply(referenceTypeID__SIZE); // No SuperClasses yet!
    if(typeID->superClazz == NULL) {
        PacketHandler->write_objectid(p, 0);
    }
    else {
        PacketHandler->write_objectid(p, typeID->superClazz->ref_clazzid);
    }

    jdwpSend(p);
}

// Injection
// Sets the value of one or more static fields
// Each field must be member of the class type or one of its superclasses, superinterfaces, or implemented interfaces
inline static void onSend_CS_CT_SetValues() {
    printf("onSend_CS_CT_SetValues\n");

    // TODO: Meeting with prof. Bockisch
    jdwpSend_Not_Implemented();
}

// Injection
// Allows to invoke static methods synchronously
inline static void onSend_CS_CT_InvokeMethod() {
    printf("onSend_CS_CT_InvokeMethod\n");

    // TODO: Meeting with prof. Bockisch
    jdwpSend_Not_Implemented();
}

// Injection
inline static void onSend_CS_CT_NewInstance() {
    printf("onSend_CS_CT_NewInstance\n");

    // TODO: Meeting with prof. Bockisch
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_CT_UnknownCommand() {
    printf("onSend_CS_CT_UnknownCommand\n");

    jdwpSend_Not_Implemented();
}
// END - CS-Handles: ######################## ClassType ########################

// Command Set "ClassType" Main Handler
inline static void onCSClassType() {
    printf("onCSClassType\n");

    switch(readcommand()) {
        case CTSuperclass:
            onSend_CS_CT_SuperClass();
            break;
        case CTSetValues:
            onSend_CS_CT_SetValues();
            break;
        case CTInvokeMethod:
            onSend_CS_CT_InvokeMethod();
            break;
        case CTNewInstance:
            onSend_CS_CT_NewInstance();
            break;
        default:
            onSend_CS_CT_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## ArrayType ########################

inline static void onSend_CS_AT_NewInstance() {
    printf("onSend_CS_AT_NewInstance\n");

    // TODO: Meeting with prof. Bockisch
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_AT_UnknownCommand() {
    printf("onSend_CS_AT_UnknownCommand\n");

    // TODO: Meeting with prof. Bockisch
    jdwpSend_Not_Implemented();
}
// END - CS-Handles: ######################## ArrayType ########################

// Command Set "ArrayType" Main Handler
inline static void onCSArrayType() {
    printf("onCSArrayType\n");

    switch(readcommand()) {
        case ATNewInstance:
            onSend_CS_AT_NewInstance();
            break;
        default:
            onSend_CS_AT_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## InterfaceType ########################
// -------------------------------------------- This CommandSet is Empty ------------
// END -   CS-Handles: ######################## InterfaceType ########################

// Command Set "InterfaceType" Main Handler
inline static void onCSInterfaceType() {
    printf("onCSInterfaceType\n");

    jdwpSend_Not_Implemented(); // CS is Empty!
}

// START - CS-Handles: ######################## Method ########################

// -Meta Data here-
// Sends line number information for the method, if present. The line table maps source line numbers to the initial code index of the line
// The line table is ordered by code index (from lowest to highest)
// The line number information is constant unless a new class de([Ljava/lang/String;)Vfinition is installed using RedefineClasses

inline static void onSend_CS_M_LineTable() {
    printf("onSend_CS_M_LineTable\n");

    referencetypeid_t rtid = readobjectid();
    methodid_t mid         = readmethodid();

    printf("rtid = %i, mid = %i\n", rtid, mid);

    method* m = RefHandler->get_method(rtid, mid);
    u4 lines = m->lines;

    printf("found m with lines = %i\n", lines);

    Packet* p = newReply((long__SIZE << 1) + int__SIZE + (long__SIZE + int__SIZE) * lines);

    PacketHandler->write_u8(p, m->startline);
    PacketHandler->write_u8(p, m->endline);
    PacketHandler->write_u4(p, lines);

    u4 start = 0;
    while(start < lines) {
        PacketHandler->write_u8(p, m->lineTable[start]->lineCodeIndex);
        PacketHandler->write_u4(p, m->lineTable[start]->lineNumber);
        start++;
    }

    jdwpSend(p);
}

// -Meta Data here-
// Sends variable information for the method. The variable table includes arguments and locals declared within the method.
// For instance methods, the "this" reference is included in the table. Also, synthetic variables may be present.
inline static void onSend_CS_M_VariableTable() {
    printf("onSend_CS_M_VariableTable\n");

    // TO_WAIT: I need to impl. meta-data setups first
    jdwpSend_Not_Implemented();
}

// Retrieve the method's bytecodes as defined in the JVM Specification.Requires canGetBytecodes capability - see CapabilitiesNew.
// Disabled by CapabilitiesNew
inline static void onSend_CS_M_Bytecodes() { // we can enable this infact, it requires Java bytecode but we can map sedona to java!!
    printf("onSend_CS_M_Bytecodes\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

// Determine if this method is obsolete
// A method is obsolete if it has been replaced by a non-equivalent method using the RedefineClasses command
// We won't support this!
inline static void onSend_CS_M_IsObsolete() {
    printf("onSend_CS_M_IsObsolete\n");

    Packet* p = newReply(byte__SIZE);
    PacketHandler->write_u1(p, FALSE); // all methods are org and no enhancements by compiler too

    jdwpSend(p);
}

// Since JDWP version 1.5
// Is it supported?
inline static void onSend_CS_M_VariableTableWithGeneric() {
    printf("onSend_CS_M_VariableTableWithGeneric\n");

    // TODO: make sure its not supported
    jdwpSend_Not_Implemented();
}

static void onSend_CS_M_UnknownCommand() {
    printf("onSend_CS_M_UnknownCommand\n");

    jdwpSend_Not_Implemented();
}

// END - CS-Handles: ######################## Method ########################

// Command Set "Method" Main Handler
inline static void onCSMethod() {
    printf("onCSMethod\n");

    switch(readcommand()) {
        case MLineTable:
            onSend_CS_M_LineTable();
            break;
        case MVariableTable:
            onSend_CS_M_VariableTable();
            break;
        case MBytecodes:
            onSend_CS_M_Bytecodes();
            break;
        case MIsObsolete:
            onSend_CS_M_IsObsolete();
            break;
        case MVariableTableWithGeneric:
            onSend_CS_M_VariableTableWithGeneric();
            break;
        default:
            onSend_CS_M_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## Field ########################
// -------------------------------------------- This CommandSet is Empty ------------
// END -   CS-Handles: ######################## Field ########################

// Command Set "Field" Main Handler
inline static void onCSField() {
    printf("onCSField\n");
    jdwpSend_Not_Implemented();
}

// START - CS-Handles: ######################## ObjectReference ########################

// Sends the runtime type of the object. The runtime type will be a class or an array.
inline static void onSend_CS_OR_ReferenceType() { // ObjectHandler will handle this!
    printf("onSend_CS_OR_ReferenceType\n");

    referencetypeid_t rtid = readobjectid();
    RefTypeID* typeID = RefHandler->find_ByID(rtid); // assume all reftypes are objids!!!

    printf("ReferenceTypeID = %i, found = %s\n", rtid, typeID != NULL ? "TRUE" : "FALSE");
    if(typeID == NULL) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p = newReply(tag__SIZE + referenceTypeID__SIZE);
    PacketHandler->write_tag(p, TYPETAG_CLASS); // later i need to add a field for array types
    PacketHandler->write_objectid(p, rtid);

    jdwpSend(p);
}

// Sends Returns the value of one or more instance fields
// Each field must be member of the object's type or one of its superclasses, superinterfaces,
// or implemented interfaces. Access control is not enforced; for example, the values of private fields can be obtained.
inline static void onSend_CS_OR_GetValues() {
    printf("onSend_CS_OR_GetValues\n");

    // TODO: impl.
    jdwpSend_Not_Implemented();
}

// Injection!
inline static void onSend_CS_OR_SetValues() {
    printf("onSend_CS_OR_SetValues\n");

    // TODO: impl.
    jdwpSend_Not_Implemented();
}

// Returns monitor information for an object. All threads in the VM must be suspended
// Disabled by GetMonitorInfo capability
inline static void onSend_CS_OR_MonitorInfo() {
    printf("onSend_CS_OR_MonitorInfo\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

// Injected! We wont support this!
inline static void onSend_CS_OR_InvokeMethod() {
    printf("onSend_CS_OR_InvokeMethod\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

// Blocks GC to clean given objectid
// We just have "Static" context is valid until vm termination
inline static void onSend_CS_OR_DisableCollection() {
    printf("onSend_CS_OR_DisableCollection\n");
}

// Unblocks GC to clean given objectid
// We just have "Static" context is valid until vm termination
inline static void onSend_CS_OR_EnableCollection() {
    printf("onSend_CS_OR_EnableCollection\n");
}

// Determines whether an object has been garbage collected in the target VM
// We just have "Static" context is valid until vm termination
// => Anyway: We must send "false" always, to notify IDE that the objectid is not collected by GC!
inline static void onSend_CS_OR_IsCollected() {
    printf("onSend_CS_OR_IsCollected\n");
    objectid_t objid = readobjectid();

    printf("Reporting objectID as a-live: %i\n", objid);
    Packet* p = newReply(byte__SIZE);
    PacketHandler->write_u1(p, FALSE);

    jdwpSend(p);
}

// Disabled by canGetInstanceInfo capability
inline static void onSend_CS_OR_ReferringObjects() {
    printf("onSend_CS_OR_ReferringObjects\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_OR_UnknownCommand() {
    printf("onSend_CS_OR_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// END - CS-Handles: ######################## ObjectReference ########################

// Command Set "ObjectReference" Main Handler
inline static void onCSObjectReference() {
    printf("onCSObjectReference\n");

    switch(readcommand()) {
        case ORReferenceType:
            onSend_CS_OR_ReferenceType();
            break;
        case ORGetValues:
            onSend_CS_OR_GetValues();
            break;
        case ORSetValues:
            onSend_CS_OR_SetValues();
            break;
        case ORMonitorInfo:
            onSend_CS_OR_MonitorInfo();
            break;
        case ORInvokeMethod:
            onSend_CS_OR_InvokeMethod();
            break;
        case ORDisableCollection:
            onSend_CS_OR_DisableCollection();
            break;
        case OREnableCollection:
            onSend_CS_OR_EnableCollection();
            break;
        case ORIsCollected:
            onSend_CS_OR_IsCollected();
            break;
        case ORReferringObjects:
            onSend_CS_OR_ReferringObjects();
            break;
        default:
            onSend_CS_OR_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## StringReference ########################

// Sends the characters contained in the string.
inline static void onSend_CS_SR_Value() {
    printf("onSend_CS_SR_Value\n");

    // TODO: impl.
    jdwpSend_Not_Implemented();
}

inline static void onSend_CS_SR_UnknownCommand() {
    printf("onSend_CS_SR_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// END -   CS-Handles: ######################## StringReference ########################

// Command Set "StringReference" Main Handler
inline static void onCSStringReference() {
    printf("onCSStringReference\n");

    switch(readcommand()) {
        case SRValue:
            onSend_CS_SR_Value();
            break;
        default:
            onSend_CS_SR_UnknownCommand();
            break;
    }

}

// START - CS-Handles: ######################## ThreadReference ########################

// Sends the thread name.
inline static void onSend_CS_TR_Name() {
    printf("onSend_CS_TR_Name\n");

    Packet* p = NULL;
    threadid_t tid = readthreadid();
    printf("ThreadID = %i\n", tid);

    if(VMController->pid() == tid) {
        p = newReply(string__SIZE(MAIN_THREAD_NAME));
        PacketHandler->write_str(p, MAIN_THREAD_NAME);
    }
    else if(JDWP_TID == tid) {
        p = newReply(string__SIZE(JDWP_THREAD_NAME));
        PacketHandler->write_str(p, JDWP_THREAD_NAME);
    }
    else if(VMController->app_tid() == tid) { // APP_TID
        p = newReply(string__SIZE(MAIN_APPLICATION_NAME));
        PacketHandler->write_str(p, MAIN_APPLICATION_NAME);
    }
    else if(JNI_TID == tid) {
        p = newReply(string__SIZE(JNI_THREAD_NAME));
        PacketHandler->write_str(p, JNI_THREAD_NAME);
    }
    else {
        jdwpSend_error(INVALID_THREAD);
        return;
    }

    jdwpSend(p);
}

// Danger! Just 2 Main threads are running, none can be suspended!
inline static void onSend_CS_TR_Suspend() {
    printf("onSend_CS_TR_Suspend\n");

    threadid_t tid = readthreadid();

    if(VMController->app_tid() == tid) {
        jdwpSend(newReply(0));
        VMController->suspend();
    }
    else {
        jdwpSend_error(INVALID_THREAD);
    }
}

// Suspension is not valid, so resuming should not be sent!
inline static void onSend_CS_TR_Resume() {
    printf("onSend_CS_TR_Resume\n");

    threadid_t tid = readthreadid();

    if(VMController->app_tid() == tid) {
        if(VMController->suspendCount() <= 1) {
            EventHandler->dispatch_THREAD_START();
        }

        VMController->resume();
        jdwpSend(newReply(0));
    }
    else {
        jdwpSend_error(INVALID_THREAD);
    }
}

// Whether it is the Main thread of JDWP thread, we send status running!
inline static void onSend_CS_TR_Status() {
    printf("onSend_CS_TR_Status\n");

    threadid_t tid = readthreadid();
    printf("threadid = %i\n", tid);

    if(tid != VMController->app_tid() && tid != VMController->pid() && tid != JNI_TID && tid != JDWP_TID) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    Packet* p = newReply(int__SIZE << 1);

    if(tid == VMController->app_tid()) {
        VMController->suspend(); // test case
        if(VMController->isSuspended()) {
            PacketHandler->write_u4(p, SLEEPING);
            PacketHandler->write_u4(p, SUSPEND_STATUS_SUSPENDED);
        }
        else {
            PacketHandler->write_u4(p, RUNNING);
            PacketHandler->write_u4(p, 0);
        }
    }
    else {
        PacketHandler->write_u4(p, RUNNING);
        PacketHandler->write_u4(p, 0);
    }

    jdwpSend(p);
}

// Sends the thread group that contains a given thread
inline static void onSend_CS_TR_ThreadGroup() {
    printf("onSend_CS_TR_ThreadGroup\n");
    threadid_t tid = readthreadid();

    Packet* p = newReply(threadGroupID__SIZE);

    if(tid == VMController->app_tid()) {
        PacketHandler->write_threadgroupid(p, THREAD_GROUP_SUB_TGID);
    }
    else {
        PacketHandler->write_threadgroupid(p, THREAD_GROUP_MAIN_TGID);

        if(tid != JDWP_TID && tid != VMController->pid()) {
            JNI_TID = tid;
        }
    }

    jdwpSend(p);
}

// Not sure if this is needed.. i need to check later!
inline static void onSend_CS_TR_Frames() {
    printf("onSend_CS_TR_Frames\n");

    Packet* p           = NULL;
    threadid_t tid      = readthreadid();
    u4 startFrame       = readu4();
    int32_t length      = readu4();

    if(length < 0){
        length = VMController->get_frameCount() - startFrame;
    }

    printf("tid = %i, startFrame = %i, Length = %i\n", tid, startFrame, length);

    p = newReply(int__SIZE + length * (frameID__SIZE + location__SIZE));
    PacketHandler->write_u4(p, length);

    FrameContainer** allFrames = VMController->getAllFrames();
    while(startFrame < length) {
        printf("frameID = %i, frameLocation_clazz = %i, frameLocation_mid = %i, frameLocation_index = %i\n",
               allFrames[startFrame]->frameID,
               allFrames[startFrame]->frameLocation->classID,
               allFrames[startFrame]->frameLocation->methodID,
               allFrames[startFrame]->frameLocation->index);

        PacketHandler->write_frameid(p, allFrames[startFrame]->frameID);
        PacketHandler->write_location(p, allFrames[startFrame]->frameLocation);
        startFrame++;
    }

    jdwpSend(p);
}

inline static void onSend_CS_TR_FrameCount() {
    printf("onSend_CS_TR_FrameCount\n");

    threadid_t tid = readthreadid();

    if(VMController->app_tid() == tid) {
        Packet* p = newReply(int__SIZE);
        PacketHandler->write_u4(p, VMController->get_frameCount());
        jdwpSend(p);
    }
    else {
        jdwpSend_error(INVALID_THREAD);
    }
}

// Sends the objects whose monitors have been entered by this thread
// The thread must be suspended, and the returned information is relevant only while the thread is suspended
// Disabled by canGetOwnedMonitorInfo capability
inline static void onSend_CS_TR_OwnedMonitors() {
    printf("onSend_CS_TR_OwnedMonitors\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

// Disabled by canGetCurrentContendedMonitor capability
inline static void onSend_CS_TR_CurrentContendedMonitor() {
    printf("onSend_CS_TR_CurrentContendedMonitor\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

// Stops the thread with an asynchronous exception, as if done by java.lang.Thread.stop
// This means stop the main and only application thread? just close the whole vm then
inline static void onSend_CS_TR_Stop() {
    printf("onSend_CS_TR_Stop\n");
    VMController->exit();
}

// Interrupt the thread, as if done by java.lang.Thread.interrupt
// again here we usually go and terminate the thread in java, and do other stuff. in sedona theres no other stuff
inline static void onSend_CS_TR_Interrupt() {
    printf("onSend_CS_TR_Interrupt\n");
    VMController->exit();
}

// Get the suspend count for this thread
// The suspend count is the number of times the thread has been suspended through the thread-level
// or VM-level suspend commands without a corresponding resume
inline static void onSend_CS_TR_SuspendCount() {
    printf("onSend_CS_TR_SuspendCount\n");
    threadid_t tid = readthreadid();

    Packet* p = newReply(int__SIZE);

    if(tid == VMController->app_tid() && VMController->isSuspended()) {
        PacketHandler->write_u4(p, VMController->suspendCount()); // just one, no count stuff needed.
    }
    else {
        PacketHandler->write_u4(p, 0);
    }

    jdwpSend(p);
}

// Disabled by canGetMonitorFrameInfo capability
inline static void onSend_CS_TR_OwnedMonitorsStackDepthInfo() {
    printf("onSend_CS_TR_OwnedMonitorsStackDepthInfo\n");

    // Should not be sent!
    jdwpSend_Not_Implemented();
}

// Disabled by canForceEarlyReturn capability
inline static void onSend_CS_TR_ForceEarlyReturn() {
    printf("onSend_CS_TR_ForceEarlyReturn\n");

    if(VMController->isSuspended() == FALSE) {
        jdwpSend_error(THREAD_NOT_SUSPENDED);
        return;
    }

    threadid_t tid = readthreadid();
    if(VMController->app_tid() != tid) {
        jdwpSend_error(INVALID_THREAD);
        return;
    }

    switch(readtag()) {
        case TAG_ARRAY: {
            // yo
            break;
        }
        case TAG_BYTE: {
            VMController->force_ExitMethod((Cell*)readu1()); // dummy
            break;
        }
        case TAG_CHAR: {
            VMController->force_ExitMethod((Cell*)readu2()); // dummy
            break;
        }
        case TAG_OBJECT: {
            // yo
            break;
        }
        case TAG_FLOAT: {
            VMController->force_ExitMethod((Cell*)readu4());
            break;
        }
        case TAG_DOUBLE: {
            VMController->force_ExitMethod((Cell*)readu8());
            break;
        }
        case TAG_INT: {
            VMController->force_ExitMethod((Cell*)readu4());
            break;
        }
        case TAG_LONG: {
            VMController->force_ExitMethod((Cell*)readu8());
            break;
        }
        case TAG_SHORT: {
            VMController->force_ExitMethod((Cell*)readu2());
            break;
        }
        case TAG_VOID: {
            VMController->force_ExitMethod((Cell*) NULL);
            break;
        }
        case TAG_BOOLEAN: {
            VMController->force_ExitMethod((Cell*)readu1());
            break;
        }
        case TAG_STRING: {
            VMController->force_ExitMethod((Cell*)readstr());
            break;
        }
        case TAG_THREAD: {
            jdwpSend_error(TYPE_MISMATCH);
            break;
        }
        case TAG_THREAD_GROUP: {
            jdwpSend_error(TYPE_MISMATCH);
            break;
        }
        case TAG_CLASS_LOADER: {
            jdwpSend_error(TYPE_MISMATCH);
            break;
        }
        case TAG_CLASS_OBJECT: {
            // well well well...
            break;
        }
        default: {
            jdwpSend_error(INVALID_OBJECT);
            break;
        }
    }

    VMController->resume();
}

inline static void onSend_CS_TR_UnknownCommand() {
    printf("onSend_CS_TR_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// END   - CS-Handles: ######################## ThreadReference ########################

// Command Set "ThreadReference" Main Handler
static void onCSThreadReference() {
    printf("onCSThreadReference\n");

    switch(readcommand()) {
        case TRName:
            onSend_CS_TR_Name();
            break;
        case TRSuspend:
            onSend_CS_TR_Suspend();
            break;
        case TRResume:
            onSend_CS_TR_Resume();
            break;
        case TRStatus:
            onSend_CS_TR_Status();
            break;
        case TRThreadGroup:
            onSend_CS_TR_ThreadGroup();
            break;
        case TRFrames:
            onSend_CS_TR_Frames();
            break;
        case TRFrameCount:
            onSend_CS_TR_FrameCount();
            break;
        case TROwnedMonitors:
            onSend_CS_TR_OwnedMonitors();
            break;
        case TRCurrentContendedMonitor:
            onSend_CS_TR_CurrentContendedMonitor();
            break;
        case TRStop:
            onSend_CS_TR_Stop();
            break;
        case TRInterrupt:
            onSend_CS_TR_Interrupt();
            break;
        case TRSuspendCount:
            onSend_CS_TR_SuspendCount();
            break;
        case TROwnedMonitorsStackDepthInfo:
            onSend_CS_TR_OwnedMonitorsStackDepthInfo();
            break;
        case TRForceEarlyReturn:
            onSend_CS_TR_ForceEarlyReturn();
            break;
        default:
            onSend_CS_TR_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## ThreadGroupReference ########################

// Sends the thread group name
// We just have the one -> send it
inline static void onSend_CS_TGR_Name() {
    printf("onSend_CS_TGR_Name\n");

    threadgroupid_t tgid = readthreadgroupid();
    printf("tgid = %i\n", tgid);
    Packet* p;

    if(tgid == THREAD_GROUP_MAIN_TGID) {
        p = newReply(string__SIZE(MAIN_THREADGROUP_NAME));
        PacketHandler->write_str(p, MAIN_THREADGROUP_NAME);
    }
    else if(tgid == THREAD_GROUP_SUB_TGID) {
        p = newReply(string__SIZE(SUB_THREADGROUP_NAME));
        PacketHandler->write_str(p, SUB_THREADGROUP_NAME);
    }
    else {
        jdwpSend_error(INVALID_THREAD_GROUP);
        return;
    }

    jdwpSend(p);
}

// Sends the thread group, if any, which contains a given thread group
// -> We just got 1 group -> send empty list
inline static void onSend_CS_TGR_Parent() {
    printf("onSend_CS_TGR_Parent\n");

    threadgroupid_t tgid = readthreadgroupid();
    printf("TGR = %i\n", tgid);

    if(tgid != THREAD_GROUP_MAIN_TGID && tgid != THREAD_GROUP_SUB_TGID) {
        jdwpSend_error(INVALID_THREAD_GROUP);
        return;
    }

    Packet* p = newReply(threadGroupID__SIZE);
    PacketHandler->write_threadgroupid(p, 0);

    //or null if the given thread group is a top-level thread group
    jdwpSend(p);
}

// Here we just get the main threadgroupid
// We just send the 2 main threads
inline static void onSend_CS_TGR_Children() { // not sure if this is right since i count 5 but send 3
    printf("onSend_CS_TGR_Children\n");

    threadgroupid_t tgid = readthreadgroupid();
    Packet* p;

    if(tgid == THREAD_GROUP_MAIN_TGID) {
        p = newReply(int__SIZE + (threadid__SIZE + int__SIZE) * THREAD_GROUP_MAIN_COUNT);
        PacketHandler->write_u4(p, THREAD_GROUP_MAIN_COUNT); // childThreads number

        // childThread
        PacketHandler->write_threadid(p, VMController->pid());
        PacketHandler->write_u4(p, 0); // childGroups

        // childThread
        PacketHandler->write_threadid(p, JDWP_TID);
        PacketHandler->write_u4(p, 0); // childGroups
    }
    else if(tgid == THREAD_GROUP_SUB_TGID) {
        p = newReply(int__SIZE + (threadid__SIZE + int__SIZE) * THREAD_GROUP_SUB_COUNT);
        PacketHandler->write_u4(p, THREAD_GROUP_SUB_COUNT);

        PacketHandler->write_threadid(p, VMController->app_tid());
        PacketHandler->write_u4(p, 0);
    }
    else {
        jdwpSend_error(INVALID_THREAD_GROUP);
        return;
    }

    jdwpSend(p);
}

inline static void onSend_CS_TGR_UnknownCommand() {
    printf("onSend_CS_TGR_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// END   - CS-Handles: ######################## ThreadGroupReference ########################

// Command Set "ThreadGroupReference" Main Handler
inline static void onCSThreadGroupReference() {
    printf("onCSThreadGroupReference\n");

    switch(readcommand()) {
        case TGRName:
            onSend_CS_TGR_Name();
            break;
        case TGRParent:
            onSend_CS_TGR_Parent();
            break;
        case TGRChildren:
            onSend_CS_TGR_Children();
            break;
        default:
            onSend_CS_TGR_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## ArrayReference ########################

// Sends the number of components in a given array
// @Out Data: ArrayID (objectid)
inline static void onSend_CS_AR_Length() {
    objectid_t o_id = readobjectid();
    Variable* var = ObjectHandler->get(o_id);

    Packet* p;
    if(var == NULL || !ObjectHandler->isArray(var)) {
        jdwpSend_error(INVALID_OBJECT);
        return;
    }

    p = newReply(int__SIZE);
    PacketHandler->write_u4(p, var->v_size);

    jdwpSend(p);
}

// Sends a range of array components. The specified range must be within the bounds of the array.
// @Out Data: ArrayID (objectid), int: first_index, int:length
// @Reply Data: The get values. If the values are objects, they are tagged-values; otherwise, they are untagged-values
inline static void onSend_CS_AR_GetValues() {
    objectid_t o_id     = readobjectid();
    u4 firstIndex = readu4();
    u4 length     = readu4();

    Variable* var = ObjectHandler->get(o_id);
    // switch type and call type getter from variable handler
        if(var == NULL || !ObjectHandler->isArray(var)) {
        jdwpSend_error(INVALID_ARRAY);
        return;
    }

    jdwpSend_Not_Implemented();
}

// END   - CS-Handles: ######################## ArrayReference ########################

// Command Set "ArrayReference" Main Handler
inline static void onCSArrayReference() {
    printf("onCSArrayReference\n");

    switch(readcommand()) {
        case ARLength:
            onSend_CS_AR_Length();
            break;
        case ARGetValues:
            onSend_CS_AR_GetValues();
            break;
        case ARSetValues:
            break;
        default:
            break;
    }
}

// START - CS-Handles: ######################## ClassLoaderReference ########################

inline static void onSend_CS_CLR_VisibleClasses() {
    printf("onSend_CS_CLR_VisibleClasses\n");

    jdwpSend_error(INVALID_CLASS_LOADER);
}

inline static void onSend_CS_CLR_UnknownCommand() {
    printf("onSend_CS_CLR_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// END   - CS-Handles: ######################## ClassLoaderReference ########################

// Command Set "ClassLoaderReference" Main Handler
inline static void onCSClassLoaderReference() {
    printf("onCSClassLoaderReference\n");

    switch(readcommand()) {
        case CLRVisibleClasses:
            onSend_CS_CLR_VisibleClasses();
            break;
        default:
            onSend_CS_CLR_UnknownCommand();
            break;
    }
}

// START - CS-Handles: ######################## EventRequest ########################
// Basic functionality, this needs to be expanded later on!
inline static void onSend_CS_ER_Set() {
    printf("onSend_CS_ER_Set\n");

    EventRequest* e          = EventHandler->newEvent();
    e->eventKind             = readu1();
    e->suspendPolicy         = readu1();
    e->modCount              = readu4();

    switch(e->eventKind) { // always mute these events, they never occur in a Sedona application
        case VM_DEATH:
        case THREAD_DEATH_END:
        case EXCEPTION:
        case CLASS_UNLOAD:
            //e->isremoved = TRUE;
            e->triggered = FALSE;
            break;
    }

    printf("EventRequest received: %s\n", EventHandler->eventKind_to_cstr(e->eventKind));

    if(e->modCount > 0) {
        u4 modifiers_start = 0;

        while(modifiers_start < e->modCount) {
            switch(readu1()) { // modKind u1 byte
                case MODKIND_COUNT: { // 1
                    e->mods[modifiers_start].count.count = readu4();
                    printf("MODKIND_COUNT: count = %i\n", e->mods[modifiers_start].count.count);
                    break;
                }
                case MODKIND_Conditional: { // 2
                    e->mods[modifiers_start].conditional.exprId = readu4();
                    printf("MODKIND_Conditional: exprid = %i\n", e->mods[modifiers_start].conditional.exprId);
                    break;
                }
                case MODKIND_ThreadOnly: { // 3
                    e->mods[modifiers_start].threadOnly.threadId = readthreadid();
                    printf("MODKIND_ThreadOnly: threadid = %i\n", e->mods[modifiers_start].threadOnly.threadId);
                    break;
                }
                case MODKIND_ClassOnly: { // 4
                    e->mods[modifiers_start].classOnly.refTypeId = readobjectid();
                    printf("MODKIND_ClassOnly: clazzid = %i\n", e->mods[modifiers_start].classOnly.refTypeId);
                    break;
                }
                case MODKIND_ClassMatch: { // 5
                    e->mods[modifiers_start].classMatch.classPattern = readstr();
                    printf("MODKIND_ClassMatch: classPattern = %s\n", e->mods[modifiers_start].classMatch.classPattern);
                    // skip inner classes ?
                    if(strchr(e->mods[modifiers_start].classMatch.classPattern, '*') != NULL) {
                        printf("MODKIND_ClassMatch: *stared* classPattern = %s\n", e->mods[modifiers_start].classMatch.classPattern);

                        size_t lenstr = strlen(e->mods[modifiers_start].classMatch.classPattern);
                        if(e->mods[modifiers_start].classMatch.classPattern[lenstr - 2] == '$') { // star only -> all classes?
                            e->mods[modifiers_start].classMatch.classPattern[lenstr - 2] = '\0';
                        }
                        else {
                            e->mods[modifiers_start].classMatch.classPattern[lenstr - 1] = '\0';
                        }

                        printf("MODKIND_ClassMatch: unstared classPattern = %s\n", e->mods[modifiers_start].classMatch.classPattern);
                    }
                    else {
                        printf("MODKIND_ClassMatch: regular classPattern = %s\n", e->mods[modifiers_start].classMatch.classPattern);
                    }

                    RefTypeID* typeID = RefHandler->find_ByClazzname(e->mods[modifiers_start].classMatch.classPattern);
                    if(typeID == NULL) {
                        //e->isremoved = TRUE;
                        //e->triggered = FALSE;
                        // why am i getting weirdos?
                        jdwpSend_error(INVALID_CLASS);
                        break;
                    }
                    break;
                }
                case MODKIND_ClassExclude: { // 6
                    e->mods[modifiers_start].classExclude.classPattern = readstr();
                    printf("MODKIND_ClassExclude: classPattern = %s\n", e->mods[modifiers_start].classExclude.classPattern);
                    break;
                }
                case MODKIND_LocationOnly: { // 7
                    e->mods[modifiers_start].locationOnly.loc = *readlocation();
                    if(e->eventKind == BREAKPOINT) {
                        printf("MODKIND_LocationOnly: clazzid = %i, mid = %i, index = %i, tagID = %i\n",
                               e->mods[modifiers_start].locationOnly.loc.classID,
                               e->mods[modifiers_start].locationOnly.loc.methodID,
                               e->mods[modifiers_start].locationOnly.loc.index,
                               e->mods[modifiers_start].locationOnly.loc.tag);
                    }
                    break;
                }
                case MODKIND_ExceptionOnly: { // 8
                    e->mods[modifiers_start].exceptionOnly.refTypeId = readobjectid();
                    e->mods[modifiers_start].exceptionOnly.caught = readu1();
                    e->mods[modifiers_start].exceptionOnly.uncaught = readu1();
                    e->isremoved = TRUE;
                    e->triggered = FALSE;
                    printf("MODKIND_ExceptionOnly: clazzid = %i, caught = %i, uncaught = %i\n",
                           e->mods[modifiers_start].exceptionOnly.refTypeId,
                           e->mods[modifiers_start].exceptionOnly.caught,
                           e->mods[modifiers_start].exceptionOnly.uncaught);
                    break;
                }
                case MODKIND_FieldOnly: { // 9
                    e->mods[modifiers_start].fieldOnly.refTypeId = readobjectid();
                    e->mods[modifiers_start].fieldOnly.fieldId = readfieldid();
                    printf("MODKIND_FieldOnly: clazzid = %i, fieldid = %i\n",
                           e->mods[modifiers_start].fieldOnly.refTypeId,
                           e->mods[modifiers_start].fieldOnly.fieldId);
                    break;
                }
                case MODKIND_Step: { // 10
                    e->mods[modifiers_start].step.threadId  = readthreadid();
                    e->mods[modifiers_start].step.size      = readu4();
                    e->mods[modifiers_start].step.depth     = readu4();
                    printf("MODKIND_Step: threadid = %i, size = %i, depth = %i\n",
                           e->mods[modifiers_start].step.threadId,
                           e->mods[modifiers_start].step.size,
                           e->mods[modifiers_start].step.depth);
                    break;
                }
                case MODKIND_InstanceOnly: { // 11
                    e->mods[modifiers_start].instanceOnly.objectId = readobjectid();
                    printf("MODKIND_InstanceOnly: clazzid = %i\n", e->mods[modifiers_start].instanceOnly.objectId);
                    break;
                }
                case MODKIND_SourceNameMatch: { // 12
                    e->mods[modifiers_start].sourceNameMatch.sourceNamePattern = readstr();
                    printf("MODKIND_InstanceOnly: sourceNamePattern = %s\n", e->mods[modifiers_start].sourceNameMatch.sourceNamePattern);
                    break;
                }
                default: {
                    printf("Error at parsing modKinds in EventRequest CommandSet!\n");
                    break;
                }
            }
            modifiers_start++;
        }
    }

    if(e->isremoved == FALSE) {
        EventHandler->add(e);
    }

    Packet* p = newReply(int__SIZE);
    PacketHandler->write_u4(p, e->requestId);
    jdwpSend(p);

    //if(e->eventKind == THREAD_START && init_jdwp == FALSE) {
        //EventHandler->dispatch_VM_INIT();
        //release_jdwpInitHalt();
    //}
}

inline static void onSend_CS_ER_Clear() {
    printf("onSend_CS_ER_Clear\n");

    u1 eventKind = readu1(); // skip EventKind
    u4 requestID = readu4();

    EventHandler->remove(eventKind, requestID);
    jdwpSend(newReply(0));
}

inline static void onSend_CS_ER_ClearAllBreakpoints() {
    printf("onSend_CS_ER_ClearAllBreakpoints\n");

    EventHandler->removeAllEventKind(BREAKPOINT);
}

inline static void onSend_CS_ER_UnknownCommand() {
    printf("onSend_CS_CLR_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// END   - CS-Handles: ######################## EventRequest ########################

// Command Set "EventRequest" Main Handler
inline static void onCSEventRequest() {
    printf("onCSEventRequest\n");

    switch(readcommand()) {
        case ERSet:
            onSend_CS_ER_Set();
            break;
        case ERClear:
            onSend_CS_ER_Clear();
            break;
        case ERClearAllBreakpoints:
            onSend_CS_ER_ClearAllBreakpoints();
            break;
        default:
            onSend_CS_ER_UnknownCommand();
            break;
    }
}

// Command Set "StackFrame" Main Handler
inline static void onCSStackFrame() {
    printf("onCSStackFrame\n");
}

// Command Set "ClassObjectReference" Main Handler
inline static void onCSClassObjectReference() {
    printf("onCSClassObjectReference\n");
}

inline static void onSend_CS_E_UnknownCommand() {
    printf("onSend_CS_E_UnknownCommand\n");
    jdwpSend_Not_Implemented();
}

// UNKNOWN Command Set Main Handler
inline static void onCSUnknown() {
    printf("onCSUnknown\n");
    jdwpSend_Not_Implemented();
}

// CommandSet classifier
inline static void onCommand() {
    switch(readcommandset()) { // CommandSet classification
        case CSVirtualMachine:
            onCSVirtualMachine();
            break;
        case CSReferenceType:
            onCSReferenceType();
            break;
        case CSClassType:
            onCSClassType();
            break;
        case CSArrayType:
            onCSArrayType();
            break;
        case CSInterfaceType:
            onCSInterfaceType();
            break;
        case CSMethod:
            onCSMethod();
            break;
        case CSField:
            onCSField();
            break;
        case CSObjectReference:
            onCSObjectReference();
            break;
        case CSStringReference:
            onCSStringReference();
            break;
        case CSThreadReference:
            onCSThreadReference();
            break;
        case CSThreadGroupReference:
            onCSThreadGroupReference();
            break;
        case CSArrayReference:
            onCSArrayReference();
            break;
        case CSClassLoaderReference:
            onCSClassLoaderReference();
            break;
        case CSEventRequest:
            onCSEventRequest();
            break;
        case CSStackFrame:
            onCSStackFrame();
            break;
        case CSClassObjectReference:
            onCSClassObjectReference();
            break;
        default:
            onCSUnknown();
            break;
    }
}

inline static void onPacket() {
    if(PacketHandler->isPacketReply(_MPacket)) {
        printf("Received a replyPacket! VM-Client mode not supported!\n");
        jdwpSend_error(SCHEMA_CHANGE_NOT_IMPLEMENTED);
    }
    else {
        onCommand();
    }
}

inline static void readStream(u4 count, u1* mbuff) {
    if(recv(_CLIENTSOCKET, mbuff, count, 0) <= 0) // -1 Error/Disconnected, 0 Remote Client closed connection
        error_exit("[readStream()] - IDE disconnected, SVM terminating...");
}

// implements extern functions
inline static u1 is_initedJDWP() { // returns jdwp init status, to break halter on application launcher
    return init_jdwp;
}

// check mpayload cap
inline static void ensurePayloadCap(u1* buff, u4* mbuff_size, u4 pLen) {
    if(*mbuff_size < pLen) {
        buff = (u1*) malloc(pLen - MIN_PACKET_SIZE);
        *mbuff_size = pLen - MIN_PACKET_SIZE;
    }
}

// OnReceive__CommandPacket Main Handler
inline static void onReceive() {
    u4 mbuff_size     = 0; // let this by dynamic, rather than init value
    u4 pLen           = 0;
    u1* mheader       = (u1*) malloc(MIN_PACKET_SIZE);
    u1* mpayload      = (u1*) malloc(mbuff_size);

    release_jdwpInitHalt();
    while(TRUE) {
        readStream(MIN_PACKET_SIZE, mheader);

        pLen = PacketHandler->read_u4_buff(mheader + LENGTH_POS);
        ensurePayloadCap(mpayload, &mbuff_size, pLen);

        if(pLen < MIN_PACKET_SIZE) { // crap received <.<
            printf("[onReceive()] - Packet size Failure!\n");
            jdwpSend_error(SCHEMA_CHANGE_NOT_IMPLEMENTED);
            continue;
        }
        else if(pLen > MIN_PACKET_SIZE) { // read payload
            readStream(pLen - MIN_PACKET_SIZE, mpayload);
            _MPacket = PacketHandler->newPacketFromHeaderPayload(mheader, mpayload);
        }
        else {
            _MPacket = PacketHandler->newPacketFromHeader(mheader);
        }

        onPacket();
    }
}

// Handles the first must packet, which is a custom handshake-packet, to identify the IDE(debugger) is running JDWP
// Then he jumps to onCommand() which handles any upcoming CommandPacket
// NOTE: On this routine, TCP is established but we require a second custom handshake in JDWP given format
inline static void onHandShake() {
    #ifdef _WIN32
        JDWP_TID = (threadid_t) GetCurrentThreadId();
    #else
        JDWP_TID = (threadid_t) pthread_getthreadid_np();
    #endif

    printf("####################################################\n");
    printf("[onHandShake()] - Waiting for ASCII Handshake = '%s'\n", HANDSHAKE_STR);

    u1* hbuffer = (u1*) malloc(HANDSHAKE_SIZE);
    readStream(HANDSHAKE_SIZE, hbuffer); // ASCII here

    printf("[onHandShake()] - Handshake received, validating...\n");

    if(strcmp(hbuffer, HANDSHAKE_STR) == 0) { // ensure the packet was HANDSHAKE_SIZE
        printf("[onHandShake()] - Handshake is valid, VM_DEBUG_MODE Active\n");
        send(_CLIENTSOCKET, hbuffer, HANDSHAKE_SIZE, 0);
        free(hbuffer);
        EventHandler->dispatch_VM_INIT();
        onReceive();
    }
    else
        error_exit("[onHandShake()] - Handshake failed, SVM terminating...\n");
}

#ifdef LOG_PACKETS
    inline static void log_header(FILE* fp, Packet* p) {
        u1* header;
        u1* formatedheader;

        if(PacketHandler->isPacketCommand(p)) {
            header = "Header:\n\tLength = %u\n\tID = %u\n\tFlags = %d\n\tCommandSet = %i\n\tCommand = %i\n";
            formatedheader = (u1*) malloc(strlen(header) + 4 + 4 + 4 + 4 + 4);
            sprintf(formatedheader, header, read_length(p), read_id(p), read_flags(p), read_commandset(p), read_command(p));
        }
        else {
            header = "Header:\n\tLength = %u\n\tID = %u\n\tFlags = %i\n\tErrorcode = %i\n";
            formatedheader = (u1*) malloc(strlen(header) + 4 + 4 + 4 + 4 + 4);
            sprintf(formatedheader, header, read_length(p), read_id(p), read_flags(p), read_errorcode(p));
        }

        fwrite(formatedheader, byte__SIZE, strlen(formatedheader), fp);
    }

    inline static void log_data(FILE* fp, Packet* p) {
        if(p->offset == MIN_PACKET_SIZE) {
            fwrite("DATA: [-]\n---------------------------------\n\n", byte__SIZE, 45, fp);
            return;
        }

        fwrite("DATA: [", byte__SIZE, 7, fp);

        u4 start = DATA_VARIABLE_POS;
        u4 count = p->offset;

        while(start < count) {
            if(start + 1 < count) {
                fprintf(fp, "%i, ", p->data[start]);
            }
            else {
                fprintf(fp, "%i]", p->data[start]);
            }

            start++;
        }
        fwrite("\n---------------------------------\n\n", byte__SIZE, 36, fp);
    }

    inline static void logPacket(Packet* p) {
        FILE* fp_vm    = fopen("log_packets\\vm_packets.plog", "ab+");
        FILE* fp_ide   = fopen("log_packets\\ide_packets.plog", "ab+");

        log_header(fp_vm, p);
        log_header(fp_ide, _MPacket);
        log_data(fp_vm, p);
        log_data(fp_ide, _MPacket);

        fflush(fp_vm);
        fflush(fp_ide);
        fclose(fp_vm);
        fclose(fp_ide);
    }
#endif // LOG_PACKETS

inline static void release_jdwpInitHalt() {
    printf("----## Application execution halt released, application running ##----\n");

    init_jdwp = TRUE; // declare VM as setup and release halt on main program executer
}

inline static void jdwpSend_error(u2 errorcode) {
    PacketHandler->write_errorcode(_PACKET_ERROR, errorcode);
    PacketHandler->write_id(_PACKET_ERROR, readid());
    send(_CLIENTSOCKET, _PACKET_ERROR->data, MIN_PACKET_SIZE, 0);

    #ifdef LOG_PACKETS
        logPacket(_PACKET_ERROR);
    #endif
}

inline static void error_exit(char* error_message) {
    close_socket();
    #ifdef _WIN32
        fprintf(stderr, "%s: %d\n", error_message, WSAGetLastError());
    #else
        fprintf(stderr, "%s: %s\n", error_message, strerror(errno));
    #endif
    exit(EXIT_FAILURE);
}

// Main init. routine. Called by VM is running in VM_DEBUG_MODE
void init_mainHandler() {
    MHandler->onStackOverFlow = onStackOverFlow;
    MHandler->onNullPointerException = onNullPointerException;
    MHandler->error_exit = error_exit;
    MHandler->jdwpSend = jdwpSend;

    _PACKET_ERROR = PacketHandler->newReplyPacket(0, 0); // must be handler!
    init_jdwp = FALSE;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    unsigned int clientaddrsize;

#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);

    if(WSAStartup(wVersionRequested, &wsaData) != 0)
        error_exit("[init. JDWP()] - WINDOWS: Fatal error of Winsock, SVM terminating...");
    else
        printf("[init. JDWP()] - WINDOWS: Winsock is up\n");
#endif

    _SERVERSOCKET = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(_SERVERSOCKET < 0)
        error_exit("[init. JDWP()] - Fatal error, socket creation failed, SVM terminating...");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; // TCP
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // all
    server_addr.sin_port = htons(PORT); // big endian port short

    if(bind(_SERVERSOCKET, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        error_exit("[init. JDWP()] - Error at binding socket to address, SVM terminating...");

    if(listen(_SERVERSOCKET, 0) == -1) // backlog, no queuing here
         error_exit("[init. JDWP()] - Listening for connections failed, SVM terminating...");

    printf("[init. JDWP()] - Listening on %s:%i - Waiting for IDE...\n", LOCAL_IP, PORT);
    clientaddrsize = sizeof(client_addr);
    _CLIENTSOCKET = accept(_SERVERSOCKET, (struct sockaddr*)&client_addr, &clientaddrsize);

    if(_CLIENTSOCKET < 0)
        error_exit("[init. JDWP()] - Couldn't accept IDE incoming connection, SVM terminating...");

    char* client_ip = inet_ntoa(client_addr.sin_addr); // get client/ide ip address
    if(strncmp(client_ip, LOCAL_IP, sizeof(LOCAL_IP)) == 0) // check if is local or remote
        printf("[init. JDWP()] - IDE connected locally from IP: %s\n", client_ip);
    else
        printf("[init. JDWP()] - IDE connected from a #Remote# IP: %s\n", client_ip);

    if(pthread_create(&THREAD_RECV, NULL, &onHandShake, NULL) != 0) // if thread creation failed
        error_exit("[init. JDWP()] - Couldn't create recieve loop into thread, SVM terminating...");

    while(is_initedJDWP() == FALSE) { // halter, flows once IDE HandShaked
        sleep(JDWP_INIT_WAIT);
    }
}

// extern error handlers
inline static void onStackOverFlow(int opcode, Cell* fp, Cell* sp) {
    EventRequest* e = EventHandler->newEvent();
    e->requestId = 0;
    e->eventKind = EXCEPTION;
    e->suspendPolicy = SP_ALL;
    e->mods[0].exceptionOnly.refTypeId = RefHandler->find_ByClazzname("java.lang.Error")->ref_clazzid;

    EventHandler->dispatch(e);
}

inline static void onNullPointerException(int opcode, Cell* fp, Cell* sp) {
    EventRequest* e = EventHandler->newEvent();
    e->requestId = 0;
    e->eventKind = EXCEPTION;
    e->suspendPolicy = SP_ALL;
    e->mods[0].exceptionOnly.refTypeId = RefHandler->find_ByClazzname("java.lang.Error")->ref_clazzid;

    EventHandler->dispatch(e);
}

// packet sender
inline static void jdwpSend(Packet* packet) {
    PacketHandler->write_length(packet, packet->offset);

    send(_CLIENTSOCKET, packet->data, packet->offset, 0);

    #ifdef LOG_PACKETS
        logPacket(packet);
    #endif

    // lets free it
    free(packet->data);
    free(packet);
}

