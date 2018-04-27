#ifndef _H_CNTS
#define _H_CNTS

// Error | 2 bytes
#define NONE 0 //No error has occurred.
#define INVALID_THREAD 10 // Passed thread is null, is not a valid thread or has exited.
#define INVALID_THREAD_GROUP 11 // Thread group invalid.
#define INVALID_PRIORITY 12 // Invalid priority.
#define THREAD_NOT_SUSPENDED 13 // If the specified thread has not been suspended by an event.
#define THREAD_SUSPENDED 14 // Thread already suspended.
#define INVALID_OBJECT 20 // If this reference type has been unloaded and garbage collected.
#define INVALID_CLASS 21 // Invalid class.
#define CLASS_NOT_PREPARED 22 // Class has been loaded but not yet prepared.
#define INVALID_METHODID 23 // Invalid method.
#define INVALID_LOCATION 24 // Invalid location.
#define INVALID_FIELDID 25 // Invalid field.
#define INVALID_FRAMEID 30 // Invalid jframeID.
#define NO_MORE_FRAMES 31 // There are no more Java or JNI frames on the call stack.
#define OPAQUE_FRAME 32 // Information about the frame is not available.
#define NOT_CURRENT_FRAME 33 // Operation can only be performed on current frame.
#define TYPE_MISMATCH 34 // The variable is not an appropriate type for the function used.
#define INVALID_SLOT 35 // Invalid slot.
#define DUPLICATE 40 // Item already set.
#define NOT_FOUND 41 // Desired element not found.
#define INVALID_MONITOR 50 // Invalid monitor.
#define NOT_MONITOR_OWNER 51 // This thread doesn't own the monitor.
#define INTERRUPT 52 // The call has been interrupted before completion.
#define INVALID_CLASS_FORMAT 60 // The virtual machine attempted to read a class file and determined that the file is malformed or otherwise cannot be interpreted as a class file.
#define CIRCULAR_CLASS_DEFINITION 61 // A circularity has been detected while initializing a class.
#define FAILS_VERIFICATION 62 // The verifier detected that a class file, though well formed, contained some sort of internal inconsistency or security problem.
#define ADD_METHOD_NOT_IMPLEMENTED 63 // Adding methods has not been implemented.
#define SCHEMA_CHANGE_NOT_IMPLEMENTED 64 // Schema change has not been implemented.
#define INVALID_TYPESTATE 65 // The state of the thread has been modified, and is now inconsistent.
#define HIERARCHY_CHANGE_NOT_IMPLEMENTED 66 // A direct superclass is different for the new class version, or the set of directly implemented interfaces is different and canUnrestrictedlyRedefineClasses is false.
#define DELETE_METHOD_NOT_IMPLEMENTED 67 // The new class version does not declare a method declared in the old class version and canUnrestrictedlyRedefineClasses is false.
#define UNSUPPORTED_VERSION 68 // A class file has a version number not supported by this VM.
#define NAMES_DONT_MATCH 69 // The class name defined in the new class file is different from the name in the old class object.
#define CLASS_MODIFIERS_CHANGE_NOT_IMPLEMENTED 70 // The new class version has different modifiers and and canUnrestrictedlyRedefineClasses is false.
#define METHOD_MODIFIERS_CHANGE_NOT_IMPLEMENTED 71 // A method in the new class version has different modifiers than its counterpart in the old class version and and canUnrestrictedlyRedefineClasses is false.
#define NOT_IMPLEMENTED 99 // The functionality is not implemented in this virtual machine.
#define NULL_POINTER 100 // Invalid pointer.
#define ABSENT_INFORMATION 101 // Desired information is not available.
#define INVALID_EVENT_TYPE 102 // The specified event type id is not recognized.
#define ILLEGAL_ARGUMENT 103 // Illegal argument.
#define OUT_OF_MEMORY 110 // The function needed to allocate memory and no more memory was available for allocation.
#define ACCESS_DENIED 111 // Debugging has not been enabled in this virtual machine. JVMDI cannot be used.
#define VM_DEAD 112 // The virtual machine is not running.
#define INTERNAL 113 // An unexpected internal error has occurred.
#define UNATTACHED_THREAD 115 // The thread being used to call this function is not attached to the virtual machine. Calls must be made from attached threads.
#define INVALID_TAG 500 // object type id or class tag.
#define ALREADY_INVOKING 502 // Previous invoke not complete.
#define INVALID_INDEX 503 // Index is invalid.
#define INVALID_LENGTH 504 // The length is invalid.
#define INVALID_STRING 506 // The string is invalid.
#define INVALID_CLASS_LOADER 507 // The class loader is invalid.
#define INVALID_ARRAY 508 // The array is invalid.
#define TRANSPORT_LOAD 509 // Unable to load the transport.
#define TRANSPORT_INIT 510 // Unable to initialize the transport.
#define NATIVE_METHOD 511  //
#define INVALID_COUNT 512 // The count is invalid.

// ThreadStatus | 1 byte
#define ZOMBIE 0
#define RUNNING 1
#define SLEEPING 2
#define MONITOR 3
#define WAIT 4

// SuspendStatus | 1 byte
#define SUSPEND_STATUS_SUSPENDED 0x1

// ClassStatus | 4 byte
#define CLASSSTATUS_VERIFIED 1
#define CLASSSTATUS_PREPARED 2
#define CLASSSTATUS_INITIALIZED 4
#define CLASSSTATUS_ERROR 8

// TypeTag | 1 byte
#define TYPETAG_CLASS 1 // ReferenceType is a class.
#define TYPETAG_INTERFACE 2 // ReferenceType is an interface.
#define TYPETAG_ARRAY 3 // ReferenceType is an array.

// Tag | 1 byte
#define TAG_ARRAY 91 // '[' - an array object (objectID size).
#define TAG_BYTE 66 // 'B' - a byte value (1 byte).
#define TAG_CHAR 67 // 'C' - a character value (2 bytes).
#define TAG_OBJECT 76 // 'L' - an object (objectID size).
#define TAG_FLOAT 70 // 'F' - a float value (4 bytes).
#define TAG_DOUBLE 68 // 'D' - a double value (8 bytes).
#define TAG_INT 73 // 'I' - an int value (4 bytes).
#define TAG_LONG 74 // 'J' - a long value (8 bytes).
#define TAG_SHORT 83 // 'S' - a short value (2 bytes).
#define TAG_VOID 86 // 'V' - a void value (no bytes).
#define TAG_BOOLEAN 90 // 'Z' - a boolean value (1 byte).
#define TAG_STRING 115 // 's' - a String object (objectID size).
#define TAG_THREAD 116  // 't' - a Thread object (objectID size).
#define TAG_THREAD_GROUP 103 // 'g' - a ThreadGroup object (objectID size).
#define TAG_CLASS_LOADER 108 // 'l' - a ClassLoader object (objectID size).
#define TAG_CLASS_OBJECT 99 // 'c' - a class object object (objectID size).

// StepDepth | 1 byte
#define INTO 0 // Step into any method calls that occur before the end of the step.
#define OVER 1 // Step over any method calls that occur before the end of the step.
#define OUT  2 // Step out of the current method.

// StepSize | 1 byte
#define MIN  0 // Step by the minimum possible amount (often a bytecode instruction).
#define LINE 1 // Step to the next source line unless there is no line number information in which case a MIN step is done instead.

// SuspendPolicy | 1 byte
#define SP_NONE         0 // Suspend no threads when this event is encountered.
#define SP_EVENT_THREAD 1 // Suspend the event thread when this event is encountered.
#define SP_ALL          2 // Suspend all threads when this event is encountered.

// InvokeOptions (The invoke options are a combination of zero or more of the following bit flags) | 1 byte
#define INVOKE_SINGLE_THREADED   0x01 // otherwise, all threads started.
#define INVOKE_NONVIRTUAL        0x02 // otherwise, normal virtual invoke (instance methods only)


#endif // _H_CNTS
