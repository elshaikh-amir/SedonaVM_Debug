#include "../svm/sedona.h"
#include "misc/MTypes.h"
#include "misc/MConstants.h"
#include "initializer.h"

#define META_FILE_NAME "sedona.meta"
#define SPACE 32
#define NEWLINE 10
#define CARRIAGE_RETURN 13

// parser
#define CLASS_DEF "class"
#define END_DEF "End"
#define FIELDS_DEF "fields"
#define SRCTABLE_DEF "SrcTable"
#define LOCAL_VAR_DEF "LocalVar"

// specific primitives in scode
#define Buf "[B"
#define Str "Ljava/lang/String"
#define Array "["
#define object "Ljava/lang/Object"

// counter
static volatile objectid_t _OID;
static refTypeID_list* refList;
static RefTypeID* startRef;

// internal forwards
static RefTypeID* findref_Byclassname(u1* cname);
static RefTypeID* findref_Bysign(u2* sign);
static RefTypeID* findref_Byrid(referencetypeid_t rid);

static u2* mk_class_sign(u1* clazzname);
static u2* mk_array_sign(u1* clazzname);
static u2* mku2(u1* str);

static Packet* method_bytes(method* m);
static Packet* methodWithGeneric_bytes(method* m);

static PacketList* methods_to_PacketList(RefTypeID* typeID);
static PacketList* methodsWithGeneric_to_PacketList(RefTypeID* typeID);

static Packet* field_bytes(field* f);
static Packet* fieldWithGeneric_bytes(field* f);

static PacketList* fields_to_PacketList(RefTypeID* typeID);
static PacketList* fieldsWithGeneric_to_PacketList(RefTypeID* typeID);

static PacketList* allClassesRefsWithGenerics_to_PacketList();
static PacketList* allClassesRefs_to_PacketList();
static size_t allClassesRefsCount();

static RefTypeID* add_ClassRef(u1* clazzname, RefTypeID* superclass);
static RefTypeID* add_InterfaceRef(u1* clazzname, RefTypeID* superClass);
static RefTypeID* add_ArrayRef(u1* clazzname, RefTypeID* superClass);

static method* get_method(referencetypeid_t rtid, methodid_t mid) ;

static void add_field(RefTypeID* typeID, field* f);
static void add_method(RefTypeID* typeID, method* m);
static void add_interface(RefTypeID* typeID, RefTypeID* interface);
static void set_superClass(RefTypeID* to, RefTypeID* superClazz);

static void setStartClassRef(RefTypeID* ref);
static RefTypeID* getStartRef();

static void add_RefTypeID(RefTypeID* typeID);
static void add_Inner_Classes_Stared_Of(RefTypeID* of);

inline static method* get_method(referencetypeid_t rtid, methodid_t mid) {
    RefTypeID* typeID = findref_Byrid(rtid);

    u2 mcount = typeID->num_methods;
    u2 start = 0;

    while(start < mcount) {
        if(typeID->methods[start]->mid == mid) {
            return typeID->methods[start];
        }
        start++;
    }

    return NULL;
}


inline static method* load_method(size_t offset, u1** wordList) {

}

inline static RefTypeID* load_class(size_t offset, FILE* fhandle) {

}

inline static u1 isSpacer(u1 c) {
    if(c == SPACE || c == NEWLINE || c == CARRIAGE_RETURN)
        return TRUE;

    return FALSE;
}

inline static void setStartClassRef(RefTypeID* ref) {
    startRef = ref;
}

inline static RefTypeID* getStartRef() {
    return startRef;
}

// big dude, he will load all the metas
inline static void load_metadata() {
    u1** metaLines = MFileManager->readFileLines(META_FILE_NAME);
    // parse Sedona Meta objects
}

inline static void add_field(RefTypeID* typeID, field* f) {
    if(typeID->max_fields <= typeID->num_fields) {
        u2 newMax_fields = (1 + typeID->max_fields) << 1;
        if((typeID->fields = realloc(typeID->fields, sizeof(field*) * newMax_fields)) == NULL) {
            MHandler->error_exit("failed allocating memory for new fields!");
        }
        else {
            typeID->max_fields = newMax_fields;
        }
    }

    typeID->fields[typeID->num_fields++] = f;
}

inline static void add_method(RefTypeID* typeID, method* m) {
    if(typeID->max_methods <= typeID->num_methods) {
        u2 newMax_methods = (1 + typeID->max_methods) << 1;
        if((typeID->methods = realloc(typeID->methods, sizeof(method*) * newMax_methods)) == NULL) {
            MHandler->error_exit("failed allocating memory for new methods!");
        }
        else {
            typeID->max_methods = newMax_methods;
        }
    }

    typeID->methods[typeID->num_methods++] = m;
}

inline static void add_interface(RefTypeID* typeID, RefTypeID* interfaceref) {
    if(typeID->max_interfaces <= typeID->num_interfaces) {
        u2 newMax_interfaces = (1 + typeID->max_interfaces) << 1;
        if((typeID->interfaces = realloc(typeID->interfaces, sizeof(RefTypeID*) * newMax_interfaces)) == NULL) {
            MHandler->error_exit("failed allocating memory for new interfaces!");
        }
        else {
            typeID->max_interfaces = newMax_interfaces;
        }
    }

    typeID->interfaces[typeID->num_interfaces++] = interfaceref;
}

inline static void set_superClass(RefTypeID* typeID, RefTypeID* superClazz) {
    typeID->superClazz = superClazz;
}

inline static size_t allClassesRefsCount() {
    return refList->size;
}

inline static PacketList* allClassesRefsWithGenerics_to_PacketList() {
    refTypeIDBuff* cur = refList->head;
    size_t lsize = refList->size;
    size_t start = 0;
    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size = 0;
    Packet* p = NULL;

    while(start < lsize) {
        p = PacketHandler->newRawPacket(byte__SIZE + int__SIZE + referenceTypeID__SIZE + PacketHandler->utf_size(cur->reftypeid->signature) + int__SIZE);

        PacketHandler->write_u1(p, cur->reftypeid->typeTag);
        PacketHandler->write_objectid(p, cur->reftypeid->ref_clazzid);
        PacketHandler->write_utf(p, cur->reftypeid->signature);
        PacketHandler->write_u4(p, 0); // none gen str, so 0 byte it
        PacketHandler->write_u4(p, cur->reftypeid->status);

        PacketHandler->add_Packet(p, pList);

        cur = cur->next;
        start++;
    }

    return pList;
}

inline static PacketList* allClassesRefs_to_PacketList() {
    refTypeIDBuff* cur = refList->head;
    size_t lsize = refList->size;
    size_t start = 0;
    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size = 0;
    Packet* p = NULL;

    while(start < lsize) {
        p = PacketHandler->newRawPacket(byte__SIZE + int__SIZE + referenceTypeID__SIZE + PacketHandler->utf_size(cur->reftypeid->signature));

        PacketHandler->write_u1(p, cur->reftypeid->typeTag);
        PacketHandler->write_objectid(p, cur->reftypeid->ref_clazzid);
        PacketHandler->write_utf(p, cur->reftypeid->signature);
        PacketHandler->write_u4(p, cur->reftypeid->status);

        PacketHandler->add_Packet(p, pList);

        cur = cur->next;
        start++;
    }
    printf("YOLOER");
    return pList;
}

inline static PacketList* methodsWithGeneric_to_PacketList(RefTypeID* typeID) {
    u2 num_methods = typeID->num_methods; // 2 bytes method count is enough i guess!

    method** m = typeID->methods;
    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size = 0;

    u2 start = 0;
    while(start < num_methods) {
        PacketHandler->add_Packet(methodWithGeneric_bytes(m[start]), pList);
        start++;
    }

    return pList;
}

inline static Packet* methodWithGeneric_bytes(method* m) {
    size_t psize = methodID__SIZE + int__SIZE;
    psize += string__SIZE(m->methodName);
    psize += PacketHandler->utf_size(m->methodJNISignature);
    psize += int__SIZE;

    Packet* p = PacketHandler->newRawPacket(psize);

    PacketHandler->write_methodid(p, m->mid);
    PacketHandler->write_str(p, m->methodName); // assume all methods are non-compile generated?
    PacketHandler->write_utf(p, m->methodJNISignature);
    PacketHandler->write_u4(p, 0);
    PacketHandler->write_u4(p, m->modBits);

    return p;
}

inline static PacketList* fieldsWithGeneric_to_PacketList(RefTypeID* typeID) {
    u2 num_fields = typeID->num_fields; // 2 bytes method count is enough i guess!

    field** f = typeID->fields;
    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size = 0;

    u2 start = 0;
    while(start < num_fields) {
        PacketHandler->add_Packet(fieldWithGeneric_bytes(f[start]), pList);
        start++;
    }

    return pList;
}

inline static Packet* fieldWithGeneric_bytes(field* f) {
    size_t psize = fieldID__SIZE + int__SIZE;
    psize += string__SIZE(f->fieldName);
    psize += PacketHandler->utf_size(f->fieldJNIsignature);
    psize += int__SIZE;

    Packet* p = PacketHandler->newRawPacket(psize);

    PacketHandler->write_fieldid(p, f->fid);
    PacketHandler->write_str(p, f->fieldName); // assume all methods are non-compile generated?
    PacketHandler->write_utf(p, f->fieldJNIsignature);
    PacketHandler->write_u4(p, 0); // empty jni generic string signature
    PacketHandler->write_u4(p, f->modBits);

    return p;
}

inline static Packet* field_bytes(field* f) {
    size_t psize = fieldID__SIZE + int__SIZE;
    psize += string__SIZE(f->fieldName);
    psize += PacketHandler->utf_size(f->fieldJNIsignature);

    Packet* p = PacketHandler->newRawPacket(psize);

    PacketHandler->write_fieldid(p, f->fid);
    PacketHandler->write_str(p, f->fieldName); // assume all methods are non-compile generated?
    PacketHandler->write_utf(p, f->fieldJNIsignature);
    PacketHandler->write_u4(p, f->modBits);

    return p;
}

inline static PacketList* fields_to_PacketList(RefTypeID* typeID) {
    u2 num_fields = typeID->num_fields; // 2 bytes method count is enough i guess!

    field** f = typeID->fields;
    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size = 0;

    u2 start = 0;
    while(start < num_fields) {
        PacketHandler->add_Packet(field_bytes(f[start]), pList);
        start++;
    }

    return pList;
}

inline static Packet* method_bytes(method* m) {
    size_t psize = methodID__SIZE + int__SIZE;
    psize += string__SIZE(m->methodName);
    psize += PacketHandler->utf_size(m->methodJNISignature);

    Packet* p = PacketHandler->newRawPacket(psize);
    PacketHandler->write_methodid(p, m->mid);
    PacketHandler->write_str(p, m->methodName); // assume all methods are non-compile generated?
    PacketHandler->write_utf(p, m->methodJNISignature);
    PacketHandler->write_u4(p, m->modBits);

    return p;
}

inline static PacketList* methods_to_PacketList(RefTypeID* typeID) {
    u2 num_methods = typeID->num_methods; // 2 bytes method count is enough i guess!

    method** m = typeID->methods;
    PacketList* pList = (PacketList*) malloc(sizeof(PacketList));
    pList->size = 0;

    u2 start = 0;
    while(start < num_methods) {
        PacketHandler->add_Packet(method_bytes(m[start]), pList);
        start++;
    }

    return pList;
}

inline static u2* mk_array_sign(u1* clazzname) {
    size_t len = strlen(clazzname) + 3;
    u2* clazzsignature = (u2*) malloc(len * sizeof(u2)); //"Lcom.company.Main;";

    clazzsignature[0] = (u2) '[';
    clazzsignature[1] = (u2) 'L';

    size_t start = 2;
    while(start < len - 3) {
        clazzsignature[start] = (u2) clazzname[start - 1];
        start++;
    }
    clazzsignature[start] = (u2) ';';
    return clazzsignature;
}

inline static u2* mk_class_sign(u1* clazzname) {
    size_t len = strlen(clazzname) + 2;
    u2* clazzsignature = (u2*) malloc((len + 1) * sizeof(u2));

    clazzsignature[0] = (u2) 'L';

    size_t start = 1;
    while(start < len - 2) {
        clazzsignature[start] = (u2) clazzname[start - 1];
        start++;
    }

    clazzsignature[start + 0]   = (u2) ';';
    clazzsignature[start + 1]   = (u2) '\0'; // null it
    return clazzsignature;
}

inline static u2* mku2(u1* str) {
    size_t len = strlen(str);
    size_t start = 0;

    u2* u2str = (u2*) malloc(len * sizeof(u2));
    while(start < len) {
        u2str[start] = (u2) str[start];
        start++;
    }

    return u2str;
}

inline static void add_RefTypeID(RefTypeID* typeID) {
    typeID->ref_clazzid     = _OID++;
    refTypeIDBuff* objbuff  = (refTypeIDBuff*) malloc(sizeof(refTypeIDBuff));
    objbuff->reftypeid      = typeID;
    objbuff->next           = refList->head;
    refList->head           = objbuff;

    refList->size++;
}

inline static RefTypeID* add_ref(u1* clazzname, RefTypeID* superClass) {
    refTypeIDBuff* objbuff  = (refTypeIDBuff*) malloc(sizeof(refTypeIDBuff));
    objbuff->reftypeid      = (RefTypeID*) malloc(sizeof(RefTypeID));
    memset(objbuff->reftypeid, 0, sizeof(RefTypeID));

    objbuff->reftypeid->ref_clazzid     = _OID++;
    objbuff->reftypeid->signature       = mk_class_sign(clazzname);
    objbuff->reftypeid->clazzname       = clazzname;
    objbuff->reftypeid->status          = (CLASSSTATUS_VERIFIED | CLASSSTATUS_PREPARED | CLASSSTATUS_INITIALIZED);
    objbuff->reftypeid->superClazz      = superClass;
    //objbuff->reftypeid->typeTag         = TYPETAG_CLASS;

    objbuff->reftypeid->num_fields      = 0; // memset already done this but for yoloer
    objbuff->reftypeid->num_methods     = 0;
    objbuff->reftypeid->num_interfaces  = 0;
    objbuff->reftypeid->max_fields      = 0;
    objbuff->reftypeid->max_methods     = 0;
    objbuff->reftypeid->max_interfaces  = 0;

    objbuff->next = refList->head;
    refList->head = objbuff;

    refList->size++;
    return objbuff->reftypeid;
}

inline static void add_Inner_Classes_Stared_Of(RefTypeID* of) {
    size_t len = strlen(of->clazzname);

    u1* stared = (u1*) malloc(len + 3);
    memcpy(stared, of->clazzname, len);
    stared[len + 0] = '$';
    stared[len + 1] = '*';
    stared[len + 2] = '\0';
    RefTypeID* ret = NULL;
    switch(of->typeTag) {
        case TYPETAG_ARRAY:
            add_ArrayRef(stared, of->superClazz);
            break;
        case TYPETAG_CLASS:
            add_ClassRef(stared, of->superClazz);
            break;
        case TYPETAG_INTERFACE:
            add_InterfaceRef(stared, of->superClazz);
            break;
        default:
            MHandler->error_exit("Unknown RefTypeID to star inner classes!");
    }
}

inline static RefTypeID* add_ClassRef(u1* clazzname, RefTypeID* superClass) {
    RefTypeID* clazz    = add_ref(clazzname, superClass);
    clazz->typeTag      = TYPETAG_CLASS;
    return clazz;
}

inline static RefTypeID* add_InterfaceRef(u1* clazzname, RefTypeID* superClass) {
    RefTypeID* inter    = add_ref(clazzname, superClass);
    inter->typeTag      = TYPETAG_INTERFACE;
    return inter;
}

inline static RefTypeID* add_ArrayRef(u1* clazzname, RefTypeID* superClass) {
    RefTypeID* arry      = add_ref(clazzname, superClass);
    arry->typeTag        = TYPETAG_ARRAY;

    return arry;
}

inline static RefTypeID* findref_Bysign(u2* sign) {
    if(sign == NULL) {
        return NULL;
    }

    refTypeIDBuff* cur = refList->head;
    size_t lsize = refList->size;
    size_t start = 0;
    u2 tchar;
    u2 schar;
    while(lsize-- > 0) {
        do {
            tchar = *(cur->reftypeid->signature + start);
            schar = *(sign + start);

            if(tchar != schar) {
                break;
            }

            start++;
        } while(tchar != NULL && schar != NULL);

        if(tchar == tchar) {
            return cur->reftypeid;
        }

        cur = cur->next;
    }

    return NULL;
}

inline static RefTypeID* findref_Byclassname(u1* clazznamep) {
    if(clazznamep == NULL) {
        return NULL;
    }

    refTypeIDBuff* cur = refList->head;
    size_t lsize = refList->size;

    while(lsize-- > 0) {
        if(strcmp(cur->reftypeid->clazzname, clazznamep) == 0) {
            return cur->reftypeid;
        }

        cur = cur->next;
    }

    return NULL;
}

inline static RefTypeID* findref_Byrid(referencetypeid_t rid) {
    refTypeIDBuff* cur = refList->head;
    size_t lsize = refList->size;

    while(lsize-- > 0) {
        if(cur->reftypeid->ref_clazzid == rid) {
            return cur->reftypeid;
        }

        cur = cur->next;
    }

    return NULL;
}

void init_refTypeHandler() {
    _OID = 1;
    refList = (refTypeID_list*) malloc(sizeof(refTypeID_list));
    refList->size                                       = 0;

    RefHandler->find_ByID                               = findref_Byrid;
    RefHandler->find_BySignature                        = findref_Bysign;
    RefHandler->methods_to_PacketList                   = methods_to_PacketList;
    RefHandler->fields_to_PacketList                    = fields_to_PacketList;
    RefHandler->fields_WithGeneric_to_PacketList        = fieldsWithGeneric_to_PacketList;
    RefHandler->methods_WithGeneric_to_PacketList       = methodsWithGeneric_to_PacketList;
    RefHandler->allClazzRefs_to_PacketList              = allClassesRefs_to_PacketList;
    RefHandler->allClazzRefsWithGenetrics_to_PacketList = allClassesRefsWithGenerics_to_PacketList;
    RefHandler->allClassesCount                         = allClassesRefsCount;
    RefHandler->get_method                              = get_method;
    RefHandler->add_startedOf                           = add_Inner_Classes_Stared_Of;
    RefHandler->add_Classref                            = add_ClassRef;
    RefHandler->add_Arrayref                            = add_ArrayRef;
    RefHandler->add_Interfaceref                        = add_InterfaceRef;
    RefHandler->find_ByClazzname                        = findref_Byclassname;
    RefHandler->add_field                               = add_field;
    RefHandler->add_method                              = add_method;
    RefHandler->add_interface                           = add_interface;
    RefHandler->set_superClass                          = set_superClass;
    RefHandler->getStartRef                             = getStartRef;
    RefHandler->setStartClassRef                        = setStartClassRef;
    RefHandler->add_RefTypeID                           = add_RefTypeID;
    load_metadata();
}
