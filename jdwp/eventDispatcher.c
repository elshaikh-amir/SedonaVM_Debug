#include <pthread.h>

#include "misc/MTypes.h"
#include "../svm/vm.h"
#include "misc/MTypes.h"
#include "misc/MConstants.h"
#include "misc/MEventkind.h"
#include "commandsets/MCommandsets.h"
#include "commandsets/Event.h"
#include "initializer.h"

#define CS_EVENT_COMPOSITE_EXTRA (byte__SIZE + int__SIZE)
#define SUSPEND_POLICY_POS 0
#define TRUE 1
#define FALSE 0

#define __LOCK sync_start();
#define __UNLOCK sync_end();

#define EVENTKINDS 14

#define MIN_SLEEP 1

// internal vars
static pthread_mutex_t MUTEX_T               = PTHREAD_MUTEX_INITIALIZER; // macro init.
static volatile u1 HOLD_EVENTS; // holdEvent
static EventRequestList* eList;          // later will split this into n eventkind lists
static EventRequest* _erTHREAD_START;
static EventRequestList** eventBags; // will follow the strict reporting rules


static volatile u4 eventRequestid_count;

// internal forwards
// sets hold flag on dispatcher
static void holdEvents(u1 hold);

// single dispatcher
static void dispatchEvent(EventRequest* e);

// events post handler
static void dispatchAll();

// check if reporting events is set on hold
static u1 canDispatchAll();

// creates and returns a new EventRequest
static EventRequest* newEventRequest();

// callback for vm
static void check_forEvents(u1 scode, Cell* sp, Cell* fp, methodid_t method);

static void internalWriteCompositeSet(Packet* p, PacketList* pList);

static void dispatch_VM_INIT();
static void dispatch_thread_start();

static void sleep(long s);

// Syncers
static void sync_start();
static void sync_end();

#ifdef _WIN32
    inline static void sleep(long s) {
        Sleep(s);
    }
#endif
inline static u1* eventKind_to_cstr(u1 kind) {
    switch(kind) {
        case VM_DEATH:                         return (u1*) "VM_DEATH";
        case VM_START_INIT:                    return (u1*) "VM_START_INIT";
        case THREAD_DEATH_END:                 return (u1*) "THREAD_DEATH_END";
        case SINGLE_STEP:                      return (u1*) "SINGLE_STEP";
        case BREAKPOINT:                       return (u1*) "BREAKPOINT";
        case FRAME_POP:                        return (u1*) "FRAME_POP";
        case EXCEPTION:                        return (u1*) "EXCEPTION";
        case USER_DEFINED:                     return (u1*) "USER_DEFINED";
        case THREAD_START:                     return (u1*) "THREAD_START";
        case CLASS_PREPARE:                    return (u1*) "CLASS_PREPARE";
        case CLASS_UNLOAD:                     return (u1*) "CLASS_UNLOAD";
        case CLASS_LOAD:                       return (u1*) "CLASS_LOAD";
        case FIELD_ACCESS:                     return (u1*) "FIELD_ACCESS";
        case FIELD_MODIFICATION:               return (u1*) "FIELD_MODIFICATION";
        case EXCEPTION_CATCH:                  return (u1*) "EXCEPTION_CATCH";
        case METHOD_ENTRY:                     return (u1*) "METHOD_ENTRY";
        case METHOD_EXIT:                      return (u1*) "METHOD_EXIT";
        case METHOD_EXIT_WITH_RETURN_VALUE:    return (u1*) "METHOD_EXIT_WITH_RETURN_VALUE";
        default:                               return (u1*) "UNKNOWN";
    }
}

inline static void print_eventrequests() {
    EventRequestBuff* curr = eList->head;
    printf("\t****EventQueue size = %i****\n", eList->size);
    size_t start = 0;

    while(start++ < eList->size) {
        printf("\t - EventKind = %s,\t - requestID = %i\n",
                eventKind_to_cstr(curr->eventRequest->eventKind),
                curr->eventRequest->requestId);

        curr = curr->next;
    }
}

inline static Packet* internalDispatch(EventRequest* e) {
    Packet* p = (Packet*) malloc(sizeof(Packet));
    p->offset = 0;

    size_t mlen;
    u4 modsIndex = 0;
    while(modsIndex == 0 || modsIndex < e->modCount) {
        switch(e->eventKind) {
            case VM_DEATH: {
                if(VMController->isSuspended()) {
                    printf("dispatch VM_DEAD\n");

                    p->data = (u1*) malloc(byte__SIZE + int__SIZE);
                    p->length = byte__SIZE + int__SIZE;
                    PacketHandler->write_u1(p, e->eventKind);
                    PacketHandler->write_u4(p, e->requestId);
                }

                break;
            }
            case VM_START_INIT: { // JDWP.EventKind.VM_INIT
                printf("dispatch VM_START_INIT\n");

                p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE);
                p->length = byte__SIZE + int__SIZE + threadid__SIZE;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->pid());

                break;
            }
            case THREAD_DEATH_END: { // JDWP.EventKind.THREAD_END
                if(VMController->isSuspended()) {
                    printf("dispatch THREAD_DEATH_END\n");

                    p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE);
                    p->length = byte__SIZE + int__SIZE + threadid__SIZE;
                    PacketHandler->write_u1(p, e->eventKind);
                    PacketHandler->write_u4(p, e->requestId);
                    PacketHandler->write_threadid(p, e->mods[modsIndex].threadOnly.threadId);
                }

                break;
            }
            case SINGLE_STEP: {
                printf("dispatch SINGLE_STEP\n");

                p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE);
                p->length = byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());
                PacketHandler->write_location(p, (Location*)&e->mods[modsIndex].locationOnly.loc);

                break;
            }
            case BREAKPOINT: {
                printf("dispatch BREAKPOINT\n");

                p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE);
                p->length = byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());
                PacketHandler->write_location(p, (Location*)&e->mods[modsIndex].locationOnly.loc);

                break;
            }
            case FRAME_POP: {
                printf("dispatch FRAME_POP\n");

                break;
            }
            case EXCEPTION: {
                printf("dispatch EXCEPTION\n");

                p->data = (u1*) malloc(int__SIZE + threadid__SIZE + location__SIZE + taggedobjectid__SIZE + byte__SIZE);
                p->length = int__SIZE + threadid__SIZE + location__SIZE + taggedobjectid__SIZE + byte__SIZE;
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());
                PacketHandler->write_location(p, (Location*)&e->mods[0].locationOnly.loc);

                break;
            }
            case USER_DEFINED: {
                printf("dispatch USER_DEFINED\n");
                break;
            }
            case THREAD_START: {
                printf("dispatch THREAD_START: APP_TID = %i\n", VMController->app_tid());

                p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE);
                p->length = byte__SIZE + int__SIZE + threadid__SIZE;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());

                break;
            }
            case CLASS_PREPARE: {
                u1* classPattern = e->mods[modsIndex].classMatch.classPattern;

                RefTypeID* clazzid = RefHandler->find_ByClazzname(classPattern);
                if(clazzid == NULL) {
                    printf("DISPATCHER ERROR: CLASS not found: %s\n", classPattern);
                    break;
                }

                printf("dispatch CLASS_PREPARE: class = %s\n", clazzid->clazzname);
                mlen = byte__SIZE +
                              int__SIZE +
                              threadid__SIZE +
                              tag__SIZE +
                              referenceTypeID__SIZE +
                              PacketHandler->utf_size(clazzid->signature) +
                              int__SIZE;

                p->data = (u1*) malloc(mlen);
                p->length = mlen;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());
                PacketHandler->write_tag(p, TYPETAG_CLASS);
                PacketHandler->write_objectid(p, clazzid->ref_clazzid);
                PacketHandler->write_utf(p, clazzid->signature);
                PacketHandler->write_u4(p, clazzid->status);

                break;
            }
            case CLASS_UNLOAD: {
                if(e->modCount > 0) { // we never unload a class so we wont report on unload unless its specified
                    RefTypeID* clazzid = RefHandler->find_ByID(e->mods[modsIndex].classOnly.refTypeId);
                    printf("dispatch CLASS_UNLOAD: class = %s\n", clazzid->clazzname);
                    mlen = byte__SIZE + int__SIZE + PacketHandler->utf_size(clazzid->signature);
                    p->data = (u1*) malloc(byte__SIZE + int__SIZE + PacketHandler->utf_size(clazzid->signature));
                    p->length = mlen;
                    PacketHandler->write_u1(p, e->eventKind);
                    PacketHandler->write_u4(p, e->requestId);
                    PacketHandler->write_utf(p, clazzid->signature);
                }

                break;
            }
            case CLASS_LOAD: {
                printf("dispatch CLASS_LOAD\n");
                break;
            }
            case FIELD_ACCESS: {
                printf("dispatch FIELD_ACCESS\n");
                break;
            }
            case FIELD_MODIFICATION: {
                printf("dispatch FIELD_MODIFICATION\n");
                break;
            }
            case EXCEPTION_CATCH: {
                printf("dispatch EXCEPTION_CATCH\n");
                break;
            }
            case METHOD_ENTRY: {
                printf("dispatch METHOD_ENTRY\n");

                p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE);
                p->length = byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());
                PacketHandler->write_location(p, (Location*)&e->mods[modsIndex].locationOnly.loc);

                break;
            }
            case METHOD_EXIT: {
                printf("dispatch METHOD_EXIT\n");

                p->data = (u1*) malloc(byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE);
                p->length = byte__SIZE + int__SIZE + threadid__SIZE + location__SIZE;
                PacketHandler->write_u1(p, e->eventKind);
                PacketHandler->write_u4(p, e->requestId);
                PacketHandler->write_threadid(p, VMController->app_tid());
                PacketHandler->write_location(p, (Location*)&e->mods[modsIndex].locationOnly.loc);

                break;
            }
            case METHOD_EXIT_WITH_RETURN_VALUE: {
                printf("dispatch METHOD_EXIT_WITH_RETURN_VALUE\n");

                break;
            }
            default: {
                printf("dispatch Unknown eventKind = %i\n", e->eventKind);
                break;
            }
        }
        modsIndex++;
    }

    return p;
}

inline static PacketList* get_by_suspendPolicy(u1 pol) {
    EventRequestBuff* event = eList->head;

    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size       = 0;
    size_t esize = eList->size;

    if(esize == 0) {
        return pList;
    }

    while(esize-- > 0) {
        if(event->eventRequest->isremoved == FALSE &&
           event->eventRequest->triggered == TRUE &&
           event->eventRequest->suspendPolicy == pol) {

            Packet* p = internalDispatch(event->eventRequest);

            event->eventRequest->triggered = FALSE;
            event->eventRequest->isremoved = TRUE;
            if(p->offset > 0) {
                PacketHandler->add_Packet(p, pList);
            }
        }

        event = event->next;
    }

    return pList;
}

inline static EventRequest* get_EventRequest_byRequestID(u4 id) {
    EventRequestBuff* cur = eList->head;

    while(cur != NULL) {
        if(cur->eventRequest->requestId == id)
            return cur->eventRequest;

        cur = cur->next;
    }

    return NULL;
}

inline static void check_forEvents(u1 scode, Cell* sp, Cell* fp, methodid_t method) {
    __LOCK
    size_t count = eList->size;

    //printf("Halt on opcpde = %s\n", opcodeToName(scode));
    if(count == 0) {
        __UNLOCK
        return;
    }

    size_t start = 0;
    EventRequestBuff* curr = eList->head;

    while(start++ < count) {
        if(curr->eventRequest->isremoved == FALSE && curr->eventRequest->triggered == TRUE) { // ignore modifiers
            __UNLOCK
            VMController->suspend();
            return;
        }
        curr = curr->next;
    }
    __UNLOCK
}

// event crafter
inline static EventRequest* newEventRequest() {
    EventRequest* e = (EventRequest*) malloc(sizeof(EventRequest));
    e->requestId = eventRequestid_count++;
    e->isremoved = FALSE;
    e->triggered = TRUE;
    return e;
}

inline static void add_EventRequest(EventRequest* e) {
    EventRequestBuff* erBuff = (EventRequestBuff*) malloc(sizeof(EventRequestBuff));
    erBuff->eventRequest = e;
    if(e->eventKind == THREAD_START) {
        _erTHREAD_START = e;
        //return;
    }

    __LOCK
    erBuff->next = eList->head;

    eList->head = erBuff;
    eList->size++;

    // log events
    print_eventrequests();

    __UNLOCK
}

inline static void internal_remove(u1 kind, u4 id) {
    EventRequestBuff* cur = eList->head;

    if(cur->eventRequest->requestId == id && cur->eventRequest->eventKind == kind) {
        eList->size = 0;
        eList->head = NULL;
        return;
    }

    while(cur->next != NULL) {
        if(cur->next->eventRequest->requestId == id && cur->eventRequest->eventKind == kind) {
            cur->next = cur->next->next;
            eList->size = eList->size - 1;
            return;
        }

        cur = cur->next;
    }
}

inline static void sync_start() {
    if(pthread_mutex_lock(&MUTEX_T) != 0) // must return 0, to tell we own the lock
        MHandler->error_exit("Fatal Error at sync EventList! Mutex could not be locked!");
}

inline static void sync_end() {
    if(pthread_mutex_unlock(&MUTEX_T) != 0)
        MHandler->error_exit("Fatal Error at generating new Packet ID! Mutex could not be Unlocked!");
}

inline static void remove_EventRequest_byKind_RequestID(u1 kind, u4 id) {
    __LOCK
    internal_remove(kind, id);
    __UNLOCK
}

inline static void removeAll_EventRequests_byKind(u1 ekind) {
    __LOCK;

    EventRequestBuff* cur = eList->head;
    while(cur != NULL) {
        if(cur->eventRequest->eventKind == ekind) {
            internal_remove(cur->eventRequest->eventKind, cur->eventRequest->requestId);
            __UNLOCK

            removeAll_EventRequests_byKind(ekind);
            return;
        }

        cur = cur->next;
    }

    __UNLOCK
}

inline static void holdEvents(u1 hold) {
    HOLD_EVENTS = hold;
}

inline static u1 canDispatchAll() {
    return HOLD_EVENTS == FALSE;
}

inline static void dispatch_thread_start() {
    if(_erTHREAD_START != NULL) {
        dispatchEvent(_erTHREAD_START);
    }
}

inline static void dispatchEvent(EventRequest* e) {
    if(e->suspendPolicy != SP_NONE) {
        VMController->suspend();
    }
    printf("Dispatch single Event: %s\n", eventKind_to_cstr(e->eventKind));

    e->triggered = FALSE;
    Packet* p = internalDispatch(e);
    Packet* cmd = PacketHandler->newCommandPacket(p->offset + CS_EVENT_COMPOSITE_EXTRA, EComposite, CSEvent);
    PacketHandler->write_u1(cmd, e->suspendPolicy);
    PacketHandler->write_u4(cmd, 1);
    PacketHandler->write_packet(cmd, p);

    MHandler->jdwpSend(cmd);
}

inline static internalDispatchSet(PacketList* pList_suspend, u1 suspol) { // must follow the strict rules for reporting
    if(pList_suspend->size == 0) {
        return;
    }

    Packet* cmd = PacketHandler->newCommandPacket(
                    CS_EVENT_COMPOSITE_EXTRA +
                    PacketHandler->get_ByteSize(pList_suspend),
                    EComposite, CSEvent);

    printf("SuspendPolicy [SP_ALL, SP_EVENT_THREAD, SP_NONE]: %i, %i, %i - found = %i\n", SP_ALL, SP_EVENT_THREAD, SP_NONE, suspol);
    PacketHandler->write_u1(cmd, suspol);
    PacketHandler->write_u4(cmd, pList_suspend->size);
    PacketHandler->write_list(cmd, pList_suspend);

    MHandler->jdwpSend(cmd);

    printf("DispatchAll(): Composite = %i events, bytes = %i\n",
           pList_suspend->size,
           cmd->offset);
}

// Callback for VM
inline static void dispatchAll() {
    __LOCK

    PacketList* pList_suspend_none  = get_by_suspendPolicy(SP_NONE);
    PacketList* pList_suspend_tid   = get_by_suspendPolicy(SP_EVENT_THREAD);
    PacketList* pList_suspend_all   = get_by_suspendPolicy(SP_ALL);

    __UNLOCK

    internalDispatchSet(pList_suspend_none, SP_NONE);
    internalDispatchSet(pList_suspend_tid, SP_EVENT_THREAD);
    internalDispatchSet(pList_suspend_all, SP_ALL);
}

inline static void dispatch_VM_Init() {
    EventRequest* e = (EventRequest*) malloc(sizeof(EventRequest));
    e->eventKind = VM_START_INIT;
    e->modCount = 0;
    e->suspendPolicy = SP_NONE;
    e->triggered = TRUE;
    e->isremoved = FALSE;
    e->requestId = 0;
    dispatchEvent(e);
}

void init_eventDispatcher() {
    eventBags            = (EventRequestList**) malloc(sizeof(EventRequestList*) * EVENTKINDS);
    eList                = (EventRequestList*) malloc(sizeof(EventRequestList));
    eList->size          = 0;
    eventRequestid_count = 1;
    HOLD_EVENTS          = FALSE;

    EventHandler->eventKind_to_cstr = eventKind_to_cstr;
    EventHandler->dispatch_THREAD_START = dispatch_thread_start;
    EventHandler->dispatch_VM_INIT = dispatch_VM_Init;
    EventHandler->add = add_EventRequest;
    EventHandler->dispatch = dispatchEvent;
    EventHandler->dispatchAll = dispatchAll;
    EventHandler->getByRequestID = get_EventRequest_byRequestID;
    EventHandler->remove = remove_EventRequest_byKind_RequestID;
    EventHandler->removeAllEventKind = removeAll_EventRequests_byKind;
    EventHandler->holdEvents = holdEvents;
    EventHandler->canDispatchAll = canDispatchAll;
    EventHandler->newEvent = newEventRequest;
    EventHandler->check_forEvents = check_forEvents;

    int mret;
    mret = pthread_mutex_init(&MUTEX_T, NULL);
}
