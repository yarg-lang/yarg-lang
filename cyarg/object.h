#ifndef cyarg_object_h
#define cyarg_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"
#include "value_cell.h"

typedef struct ObjConcreteYargType ObjConcreteYargType;

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_BLOB(value)         isObjType(value, OBJ_BLOB)
#define IS_ROUTINE(value)      isObjType(value, OBJ_ROUTINE)
#define IS_CHANNEL(value)      isObjType(value, OBJ_CHANNEL)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_VALARRAY(value)     isObjType(value, OBJ_VALARRAY)
#define IS_UNIFORMARRAY(value) isObjType(value, OBJ_UNIFORMARRAY)
#define IS_YARGTYPE(value)     isObjType(value, OBJ_YARGTYPE)
#define IS_POINTER(value)      isObjType(value, OBJ_POINTER)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_BLOB(value)         ((ObjBlob*)AS_OBJ(value))
#define AS_ROUTINE(value)      ((ObjRoutine*)AS_OBJ(value))
#define AS_CHANNEL(value)      ((ObjChannel*)AS_OBJ(value))
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_VALARRAY(value)     ((ObjValArray*)AS_OBJ(value))
#define AS_UNIFORMARRAY(value) ((ObjUniformArray*)AS_OBJ(value))
#define AS_YARGTYPE(value)     ((ObjConcreteYargType*)AS_OBJ(value))
#define AS_POINTER(value)      ((ObjPointer*)AS_OBJ(value))

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_BLOB,
    OBJ_ROUTINE,
    OBJ_CHANNEL,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_VALARRAY,
    OBJ_UNIFORMARRAY,
    OBJ_YARGTYPE,
    OBJ_POINTER,
    OBJ_AST,
    OBJ_STMT_EXPRESSION,
    OBJ_STMT_PRINT,
    OBJ_STMT_VARDECLARATION,
    OBJ_STMT_BLOCK,
    OBJ_STMT_IF,
    OBJ_STMT_FUNDECLARATION,
    OBJ_STMT_WHILE,
    OBJ_STMT_RETURN,
    OBJ_STMT_YIELD,
    OBJ_STMT_FOR,
    OBJ_STMT_CLASSDECLARATION,
    OBJ_STMT_STRUCTDECLARATION,
    OBJ_STMT_FIELDDECLARATION,
    OBJ_EXPR_NUMBER,
    OBJ_EXPR_OPERATION,
    OBJ_EXPR_GROUPING,
    OBJ_EXPR_NAMEDVARIABLE,
    OBJ_EXPR_NAMEDCONSTANT,
    OBJ_EXPR_LITERAL,
    OBJ_EXPR_STRING,
    OBJ_EXPR_CALL,
    OBJ_EXPR_ARRAYINIT,
    OBJ_EXPR_ARRAYELEMENT,
    OBJ_EXPR_BUILTIN,
    OBJ_EXPR_DOT,
    OBJ_EXPR_SUPER,
    OBJ_EXPR_TYPE,
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};

typedef struct {
    Obj* stash;
    Obj** objects;
    int objectCapacity;
    int objectCount;
} DynamicObjArray;

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef bool (*NativeFn)(ObjRoutine* routine, int argCount, ValueCell* args, Value* result);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

typedef struct {
    Obj obj;
    void* blob;
} ObjBlob;

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    ValueCell* location;
    ValueCell closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* name;
    ValueTable methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    ValueTable fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value reciever;
    ObjClosure* method;
} ObjBoundMethod;

typedef struct {
    Obj obj;
    volatile bool present;
    bool overflow;
    Value data;
} ObjChannel;

typedef struct ObjValArray {
    Obj obj;
    ValueArray array;
} ObjValArray;

typedef struct ObjUniformArray {
    Obj obj;
    ObjConcreteYargType* element_type;
    size_t count;
    size_t element_size;
    void* array;
} ObjUniformArray;

typedef struct {
    Obj obj;
    Value type;
    void* destination;
} ObjPointer;

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

Obj* allocateObject(size_t size, ObjType type);

void initDynamicObjArray(DynamicObjArray* array);
void freeDynamicObjArray(DynamicObjArray* array);
void appendToDynamicObjArray(DynamicObjArray* array, Obj* obj);
Obj* removeLastFromDynamicObjArray(DynamicObjArray* array);

ObjBoundMethod* newBoundMethod(Value receiver, 
                               ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjInstance* newInstance(ObjClass* klass);
ObjNative* newNative(NativeFn function);
ObjBlob* newBlob(size_t size);
ObjValArray* newValArray(size_t capacity);
ObjUniformArray* newUniformArray(ObjConcreteYargType* element_type, size_t capacity);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(ValueCell* slot);
ObjPointer* newPointer(Value type);

void printObject(Value value);
void fprintObject(FILE* op, Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

static inline bool isArray(Value value) {
    return IS_VALARRAY(value) || IS_UNIFORMARRAY(value);
}

#endif