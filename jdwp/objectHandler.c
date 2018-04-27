#include "../svm/sedona.h"
#include "misc/MTypes.h"
#include "initializer.h"

#define Array_TypeID_Offset (1 << 5)  // yyy1 xxxx
#define Object_TypeID_Offset (1 << 6) // yy1y xxxx

#define mkArray(t) (t | Array_TypeID_Offset)
#define mkObject(o) (o | Object_TypeID_Offset)

static VariableList* vList;
static volatile objectid_t vid;

// internal forwards
static Variable* get_Variable_byoID(objectid_t id);

// returns TRUE if v defines a primitive type | BufTypeID is *NOT* primitive "cheating" here
static u1 isPrimitive(Variable* v);

// returns TRUE if v defines any types of type array, only array, not object!
static u1 isArray(Variable* v);

// returns TRUE if v defines an object | only object, not array!
static u1 isObject(Variable* v);

// returns array 1 byte primitive values
static u1*  getArrayValues_u1(Variable* v);

// returns array 2 bytes primitive values
static u2* getArrayValues_u2(Variable* v);

// returns array 4 bytes primitive values
static u4* getArrayValues_u4(Variable* v);

// returns array 8 bytes primitive values
static u8* getArrayValues_u8(Variable* v);

// returns 1 byte primitive value
static u1 getValue_u1(Variable* v);

// returns 2 bytes primitive value
static u2 getValue_u2(Variable* v);

// returns 4 bytes primitive value
static u4 getValue_u4(Variable* v);

// returns 8 bytes primitive value
static u8 getValue_u8(Variable* v);

static void dispose();

// external loader
void init_objectHandler() {
    vList = (VariableList*) malloc(sizeof(VariableList));
    vid = 1;

    ObjectHandler->dispose = dispose;
    ObjectHandler->get = get_Variable_byoID;
    ObjectHandler->isPrimitive = isPrimitive;
    ObjectHandler->isArray = isArray;
    ObjectHandler->isObject = isObject;
    ObjectHandler->getArrayValues_u1 = getArrayValues_u1;
    ObjectHandler->getArrayValues_u2 = getArrayValues_u2;
    ObjectHandler->getArrayValues_u4 = getArrayValues_u4;
    ObjectHandler->getArrayValues_u8 = getArrayValues_u8;
    ObjectHandler->getValue_u1 = getValue_u1;
    ObjectHandler->getValue_u2 = getValue_u2;
    ObjectHandler->getValue_u4 = getValue_u4;
    ObjectHandler->getValue_u8 = getValue_u8;
}

inline static void dispose() {
    vList->head = NULL;
    vList->size = 0;
}

inline static void add_Variable(Variable* v) {
    VariableBuff* vbuff = (VariableBuff*) malloc(sizeof(VariableBuff));
    vbuff->variable = v;

    vbuff->next = vList->head;
    vList->head = vbuff;
    vList->size++;
}

inline static Variable* get_Variable_byoID(objectid_t id) {
    VariableBuff* cur = vList->head;

    while(cur != NULL) {
        if(cur->variable->v_id == id)
            return cur->variable;

        cur = cur->next;
    }
    return NULL;
}

inline static void remove_Variable_byoID(objectid_t id) {
    VariableBuff* cur = vList->head;

    if(cur->variable->v_id == id) {
        vList->size = 0;
        free(vList->head);
        vList->head = NULL;
        return;
    }

    while(cur->next != NULL) {
        if(cur->next->variable->v_id == id) {
            VariableBuff* tbuff = cur->next;
            cur->next = cur->next->next;
            free(tbuff);
            vList->size--;
            return;
        }

        cur = cur->next;
    }
}

inline static u1 isPrimitive(Variable* v) {
    u1 type = v->v_type;
    return type >= VoidTypeId && type < BufTypeId;
}

// Returns the primitive type held by array or self
inline static u1 getPrimitiveType(Variable* v) {
    return v->v_type & 0xf;
}

u1 getNonPrimitiveType(Variable* v) {
    return v->v_type & 0xf0;
}

// Type array
inline static u1 isArray(Variable* v) {
    return (v->v_type & Array_TypeID_Offset) == Array_TypeID_Offset;
}

// Type object
inline static u1 isObject(Variable* v) {
    return (v->v_type & Object_TypeID_Offset) == Object_TypeID_Offset;
}

inline static u1* getArrayValues_u1(Variable* v) {
    u4 sizearr = v->v_size;
    u1* arrItems = (u1*) malloc(sizearr * byte__SIZE);

    Cell* itemp = v->v_stack;
    u4 start = 0;

    while(start++ < sizearr) {
        arrItems = *(u1*)itemp;
        itemp++;
        arrItems++;
    }

    return arrItems - sizearr;
}

inline static u2* getArrayValues_u2(Variable* v) {
    return NULL;
}

inline static u4* getArrayValues_u4(Variable* v) {
    return NULL;
}

inline static u8* getArrayValues_u8(Variable* v) {
    return NULL;
}

inline static u1 getValue_u1(Variable* v) {
    return *(u1*)v->v_stack;
}

inline static u2 getValue_u2(Variable* v) {
    return *(u2*)v->v_stack;
}

inline static u4 getValue_u4(Variable* v) {
    return *(u4*)v->v_stack;
}

inline static u8 getValue_u8(Variable* v) {
    return *(u8*)v->v_stack;
}


