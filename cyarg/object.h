#ifndef cyarg_object_h
#define cyarg_object_h

/* Memory Management
 *
 * the VM aims to provide a managed memory system for Obj based memory, and additionally
 * to intercept all C memory allocations in target images.
 * 
 * A goal is for memory allocation required in the langauge to be O(1) and therefore useable
 * in contexts like interrupt response, where predictable time is desired.
 * 
 * Currently, an external heap (o1heap) is used, and reserves a large static array for all
 * allocations.
 * 
 * There are therefore a suite of C-like memory management functions (see memory.h), and a 
 * garbage collector to manage Obj instances (see here, and memory.h)
 * 
 * The system is safe for concurrent access from multiple cores and interrupt contexts.
 * 
 * Most VM memory allocation is for Obj instances.
 *
 * The garbage collector is hooked into the standard allocation functions. In time, it will
 * also be designed to be O(1) in cases needed by the language, but this is not yet implemented.
 *
 * Obj instances are tracked in two places. All instances can be traversed, by the linked
 * list formed from the next member of Obj.
 *
 * If the Obj instance should not be deleted, it should be present in a global visible to markRoots()
 *
 * The globals are notably the language stack, some VM state, and the 'temporary root'
 * stack accessible throughout the VM.
 *
 * Since any call to reallocate() can trigger the GC, objects constructed via multiple calls to reallocate()
 * often need care in storage during this phase. Commonly either push()'d onto the
 * language stack, or tempRootPush()'d onto the temporary root stack. Note that a partially constructed object
 * must be safe to free, and object allocation zero's the whole memory cell as a convenience for this
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ObjString ObjString;

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_BUILTIN,
    OBJ_ROUTINE,
    OBJ_CHANNELCONTAINER,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_UNOWNED_PACKEDCONTAINER,
    OBJ_PACKEDCONTAINER,
    OBJ_PACKEDSTORAGE,
    OBJ_YARGTYPE,
    OBJ_YARGTYPE_ARRAY,
    OBJ_YARGTYPE_STRUCT,
    OBJ_YARGTYPE_POINTER,
    OBJ_YARGTYPE_MAP,
    OBJ_SYNCGROUP,
    OBJ_MAP,
    OBJ_STACKSLICE,
    OBJ_AST,
    OBJ_PLACEALIAS,
    OBJ_SCANNER,
    OBJ_PARSER,
    OBJ_STMT_EXPRESSION,
    OBJ_STMT_PRINT,
    OBJ_STMT_POKE,
    OBJ_STMT_VARDECLARATION,
    OBJ_STMT_FIELDDECLARATION,
    OBJ_STMT_PLACEDECLARATION,
    OBJ_STMT_BLOCK,
    OBJ_STMT_IF,
    OBJ_STMT_FUNDECLARATION,
    OBJ_STMT_WHILE,
    OBJ_STMT_RETURN,
    OBJ_STMT_YIELD,
    OBJ_STMT_FOR,
    OBJ_STMT_CLASSDECLARATION,
    OBJ_EXPR_NUMBER,
    OBJ_EXPR_ADDRESS,
    OBJ_EXPR_OPERATION,
    OBJ_EXPR_GROUPING,
    OBJ_EXPR_NAMEDVARIABLE,
    OBJ_EXPR_LITERAL,
    OBJ_EXPR_STRING,
    OBJ_EXPR_CALL,
    OBJ_EXPR_COLLECTION_INITIALIZER,
    OBJ_EXPR_COLLECTION_ELEMENT,
    OBJ_EXPR_PAIR,
    OBJ_EXPR_BUILTIN,
    OBJ_EXPR_DOT,
    OBJ_EXPR_SUPER,
    OBJ_EXPR_TYPE,
    OBJ_EXPR_TYPE_STRUCT,
    OBJ_EXPR_TYPE_INDEXED_COLLECTION,
    OBJ_INT
} ObjType;

typedef struct Obj Obj;

typedef struct Obj {
    ObjType type;
    bool isMarked;
    Obj* next;
} Obj;

// Dynamic array of Object pointers. Takes ownership of the passed object before growing the array.
typedef struct {
    Obj* stash;
    Obj** objects;
    int objectCapacity;
    int objectCount;
} DynamicObjArray;


#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

Obj* allocateObject(size_t size, ObjType type);

ObjString* objectToString(const Obj* value);

void initDynamicObjArray(DynamicObjArray* array);
void freeDynamicObjArray(DynamicObjArray* array);
void gc_freeDynamicObjArray(DynamicObjArray* array);
void appendToDynamicObjArray(DynamicObjArray* array, Obj* obj);

#endif
