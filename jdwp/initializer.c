#include "../svm/sedona.h"
#include "initializer.h"
#include "misc/MEventkind.h"

#include "misc/MConstants.h"
#include "../svm/vm.h"
#include "mainHandler.h"
#include "packetHandler.h"
#include "refTypHandler.h"
#include "objectHandler.h"
#include "fileManager.h"

// Java base classes file name
#define JAVA_BASE_CLASSES_META "jbases.meta"

// JBASE Prefixes & Attributes

#define JAVA_BASE_ARRAY_CLASS '['
#define JAVA_BASE_PARSE_DIL ':'
#define JAVA_BASE_ROOT_CLASS "java.lang.Object"

// Type Ref
#define JBASE_TYPE_INTERFACE "Interface"
#define JBASE_TYPE_CLASS "Class"

// Type Class
#define JBASE_CLASS_NAME "ClassName"
#define JBASE_CLASS_MODS "ClassMods"
#define JBASE_CLASS_SUPERCLASS_NAME "ClassSuperClassName"

// Type Interface
#define JBASE_INTERFACE_NAME "InterfaceName"
#define JBASE_INTERFACE_MODS "InterfaceMods"

// Shared
#define JBASE_INTERFACE_COUNT "InterfacesCount"
#define JBASE_INTERFACE_IMPLEMENTED "ImplementedInterfaceName"

// Fields
#define JBASE_FIELD_COUNT "FieldsCount"
#define JBASE_FIELD_NAME "FieldName"
#define JBASE_FIELD_MODS "FieldMods"
#define JBASE_FIELD_TYPE "FieldType"

// Methods
#define JBASE_METHOD_COUNT "Methods-Count"
#define JBASE_METHOD_NAME "MethodName"
#define JBASE_METHOD_MODS "MethodMods"
#define JBASE_METHOD_SIGNATURE "MethodSignature"

// Method-Parameters
#define JBASE_METHOD_PARAMETER_COUNT "MethodParametersCount"
#define JBASE_METHOD_PARAMETER_NAME "ParameterName"
#define JBASE_METHOD_PARAMETER_MODS "ParameterMods"
#define JBASE_METHOD_PARAMETER_TYPE "ParameterType"


// debugger main class options
#define IS_ECLIPSE

#ifdef IS_ECLIPSE
    #define MAIN_CLASS "TestClass"
    #define MAIN_CLASS_SOURCE "TestClass.java"
#else
    #define MAIN_CLASS "com.company.Main" // "TestClass"
#define MAIN_CLASS_SOURCE "com/company/Main.java" // TestClass.java
#endif // IS_ECLIPSE


// Belongs to EventDispatcher!
    #define ACC_PUBLIC	0x0001	//Declared public; may be accessed from outside its package.
    #define ACC_FINAL	0x0010	//Declared final; no subclasses allowed.
    #define ACC_SUPER	0x0020	//Treat superclass methods specially when invoked by the invokespecial instruction.
    #define ACC_INTERFACE	0x0200	//Is an interface, not a class.
    #define ACC_ABSTRACT	0x0400	//Declared abstract; must not be instantiated.
    #define ACC_SYNTHETIC	0x1000	//Declared synthetic; not present in the source code.
    #define ACC_ANNOTATION	0x2000	//Declared as an annotation type.
    #define ACC_ENUM	0x4000	//Declared as an enum type.
    #define ACC_STATIC 8

// End of belongs to EventDispatcher!
// Globals
HMainHandler* MHandler;
HEventDispatcher* EventHandler;
HRefHandler* RefHandler;
HPacketHandler* PacketHandler;
HVariableHandler* ObjectHandler;
HVMHandler* VMController;
MFManager* MFileManager;

// internal forwards
static void load_Jbases();
static RefTypeID* main_class();

// must follow handler init order. Dispatcher must be init last to report vm init event over mhandler and packethandler
void init__VM_DEBUG_MODE() {
    printf("####################################################\n");
    printf("####!! WARNING:  SVM RUNNING IN VM_DEBUG_MODE !!####\n");
    printf("####################################################\n");

    printf("\t-- MethodID  = %i bytes\n", methodID__SIZE);
    printf("\t-- FieldID   = %i bytes\n", fieldID__SIZE);
    printf("\t-- FrameID   = %i bytes\n", frameID__SIZE);
    printf("\t-- ObjectID  = %i bytes\n", objectid__SIZE);
    printf("\t-- Location  = %i bytes\n", location__SIZE);
    printf("\t-- size_t    = %i bytes\n", sizeof(size_t));
    printf("####################################################\n");

    MHandler        = (HMainHandler*) malloc(sizeof(HMainHandler));
    EventHandler    = (HEventDispatcher*) malloc(sizeof(HEventDispatcher));
    RefHandler      = (HRefHandler*) malloc(sizeof(HRefHandler));
    PacketHandler   = (HPacketHandler*) malloc(sizeof(HPacketHandler));
    ObjectHandler   = (HVariableHandler*) malloc(sizeof(HVariableHandler));
    VMController    = (HVMHandler*) malloc(sizeof(HVMHandler));
    MFileManager    = (MFManager*) malloc(sizeof(MFManager));

    init_fileManager();     // inits the file and string methonds, no halt
    init_packetHandler();  // inits the PacketHandler,                          no halt
    init_refTypeHandler(); // inits the (ClassSignature-Typing) refTypeHandler, no halt
    init_objectHandler();// inits the (Meta-Data) VariableHandler,            no halt
    init_eventDispatcher();// inits the (EventRequest-Handler) EventDispatcher, no halt
    init_vmController();   // inits the VMController,                           no halt

    load_Jbases();
    RefHandler->setStartClassRef(main_class());

    // Starts the Sedona JDWP server, located in mainHandler
    init_mainHandler();    // inits the (JDWPServer) mainHandler,               HALT
}

inline static RefTypeID* main_class() {
    RefTypeID* typeID = RefHandler->add_Classref(MAIN_CLASS, NULL);
    typeID->num_methods = 2;
    typeID->num_fields  = 0;
    typeID->sourceFile  = MAIN_CLASS_SOURCE; // required to map back?
    typeID->status = (CLASSSTATUS_VERIFIED | CLASSSTATUS_PREPARED | CLASSSTATUS_INITIALIZED);

    typeID->methods = (method**) malloc(sizeof(method*) * 2); // dummy size

    // Constructor
    method* constructorMethod = (method*) malloc(sizeof(method));
    constructorMethod->methodName = "<init>";
    constructorMethod->mid = 1;
    constructorMethod->methodJNISignature = MFileManager->mku2("()V");
    constructorMethod->modBits = ACC_PUBLIC | ACC_SYNTHETIC;
    constructorMethod->startline = 1;
    constructorMethod->endline = 2;
    constructorMethod->lines = 1;

    constructorMethod->lineTable = (LineTable**) malloc(sizeof(LineTable*) * 1); // dummy size
    LineTable* cline_table = (LineTable*) malloc(sizeof(LineTable));
    cline_table->lineCodeIndex   = 0;
    cline_table->lineNumber      = 1;
    constructorMethod->lineTable[0] = cline_table;

    typeID->methods[0] = constructorMethod;

    // Main Method
    method* mainMethod = (method*) malloc(sizeof(method));
    mainMethod->methodName = "main";
    mainMethod->methodJNISignature = MFileManager->mku2("([Ljava/lang/String;)V");
    mainMethod->modBits = ACC_PUBLIC | ACC_STATIC;
    mainMethod->mid = 1;
    mainMethod->startline = 3;
    mainMethod->endline   = 4;
    mainMethod->lines = 1;
    mainMethod->lineTable = (LineTable**) malloc(sizeof(LineTable*) * mainMethod->lines); // dummy size
    LineTable* line_table = (LineTable*) malloc(sizeof(LineTable));
    line_table->lineCodeIndex   = 2;
    line_table->lineNumber      = 3;

    mainMethod->lineTable[0]    = line_table;
    typeID->methods[1]          = mainMethod;
    RefHandler->add_startedOf(typeID);
    printf("[Main_Method]  - %s.%s: %s, ModBits: %i, Lines: %i [%lli - %lli]\n",
           MAIN_CLASS,
           mainMethod->methodName,
           MFileManager->u2Tou1(mainMethod->methodJNISignature),
           mainMethod->modBits,
           mainMethod->lines,
           mainMethod->startline,
           mainMethod->endline);
    return typeID;
}

// This seems to be buggy
inline static bool progress_jbase_line(u1* entry) {
    RefTypeID* typeID       = (RefTypeID*) malloc(sizeof(RefTypeID));
    typeID->status          = (CLASSSTATUS_VERIFIED | CLASSSTATUS_PREPARED | CLASSSTATUS_INITIALIZED);

    typeID->num_fields      = 0;
    typeID->num_methods     = 0;
    typeID->num_interfaces  = 0;
    typeID->max_fields      = 0;
    typeID->max_methods     = 0;
    typeID->max_interfaces  = 0;
    size_t start;
    size_t num;
    size_t offset           = 0;
    u1** items              = MFileManager->split(entry, ',');
    //printf("Entry = %s\n", entry);

    bool isClass            = strcmp(items[0], JBASE_TYPE_CLASS) == 0;
    bool isArray_type       = isClass == TRUE && items[0] == JAVA_BASE_ARRAY_CLASS;
    bool isInterface        = isClass == FALSE && isArray_type == FALSE;
    u1** AV                 = MFileManager->split(items[1], JAVA_BASE_PARSE_DIL);

    typeID->clazzname       = AV[1];
    AV                      = MFileManager->split(items[2], JAVA_BASE_PARSE_DIL);
    typeID->modBits         = MFileManager->castToInt(AV[1]);

    if(isInterface == TRUE) {
        typeID->typeTag     = TYPETAG_INTERFACE;

        // InterfacesCount
        offset              = 3;
    }
    else {
        if(isClass == TRUE) {
            typeID->typeTag     = TYPETAG_CLASS;
        }
        else if(isArray_type == TRUE) {
            typeID->typeTag     = TYPETAG_ARRAY;
        }
        else {
            MHandler->error_exit("Unknown ref TYPETAG!\n");
        }

        AV                  = MFileManager->split(items[3], JAVA_BASE_PARSE_DIL);
        if(strcmp(AV[1], JAVA_BASE_ROOT_CLASS) != 0) {
            typeID->superClazz  = RefHandler->find_ByClazzname(AV[1]);
        }

        // InterfacesCount
        offset              = 4;
    }

    // InterfacesCount
    AV                      = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
    num                     = MFileManager->castToInt(AV[1]);
    start                   = 0;
    while(start < num) {
        // InterfaceName -> ClassName
        AV                  = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        RefTypeID* interfaceFound = RefHandler->find_ByClazzname(AV[1]);
        if(interfaceFound == NULL) {
           // return FALSE;
            //printf("ERROR: Implemented Interface '%s' of Class '%s' not found!\n", AV[1], typeID->clazzname);
        }
        else {
            //printf("Adding Implemented Interface '%s' to '%s'\n", AV[1], typeID->clazzname);
            RefHandler->add_interface(typeID, interfaceFound);
        }

        start++;
    }

    // FieldsCount
    AV                      = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
    num                     = MFileManager->castToInt(AV[1]);
    start                   = 0;
    while(start < num) {
        field* mfield       = (field*) malloc(sizeof(field));
        // FieldUID
        mfield->fid         = (fieldid_t) start;
        // FieldName
        AV                  = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        mfield->fieldName   = AV[1];
        //printf("Field name = %s\n", mfield->fieldName);
        // FieldMods
        AV                  = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        mfield->modBits     = MFileManager->castToInt(AV[1]);
        // FieldType -> Signature
        AV                  = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        mfield->fieldJNIsignature = MFileManager->mku2(AV[1]);

        RefHandler->add_field(typeID, mfield);
       // printf("Added Field: id=%i, mods=%i, name='%s', type='%s', class='%s'\n",
               //mfield->fid, mfield->modBits, mfield->fieldName, MFileManager->u2Tou1(mfield->fieldJNIsignature), typeID->clazzname);

        start++;
    }

    // Methods-Count
    AV                          = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
    num                         = MFileManager->castToInt(AV[1]);
    start                       = 0;
    while(start < num) {
        //printf("Entered Method Addition\n");
        method* mMethod         = (method*) malloc(sizeof(method));
        // MethodUID
        mMethod->mid            = (methodid_t) start;
        // MethodName
        AV                      = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        mMethod->methodName     = AV[1];
        //printf("Method Name = %s\n", AV[1]);
        // MethodMods
        AV                      = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        mMethod->modBits        = MFileManager->castToInt(AV[1]);
        //printf("Method modBits = %i\n", mMethod->modBits);
        // MethodSignature
        AV                      = MFileManager->split(items[offset++], JAVA_BASE_PARSE_DIL);
        //printf("Method Signature = %s\n", AV[1]);
        mMethod->methodJNISignature = MFileManager->mku2(AV[1]);

        RefHandler->add_method(typeID, mMethod);
        start++;
    }
    //printf("Added %s with %i interfaces, %i fields and %i methods\n", typeID->clazzname, typeID->num_interfaces, typeID->num_fields, typeID->num_methods);
    RefHandler->add_RefTypeID(typeID);
    return TRUE;
}

inline static void load_Jbases() {
    u1** metas = MFileManager->readFileLines(JAVA_BASE_CLASSES_META);
    size_t line_index = 0;
    size_t mErrors    = 0;
    while(metas[line_index] != NULL) {
        if(progress_jbase_line(metas[line_index]) == FALSE) {
            mErrors++;
        }
        line_index++;
    }

    free(metas);
    printf("[Init. jBase]  - Success %i RefTypeIDs!\n", RefHandler->allClassesCount());
    printf("[Init. jBase]  - Error %i RefTypeIDs!\n", mErrors);
    if(line_index != mErrors + RefHandler->allClassesCount()) {
        printf("[Init. jBase]  - RefHandler Error! Expected %i RefTypeIDs, Found %i!\n", line_index, mErrors + RefHandler->allClassesCount());
    }
}
