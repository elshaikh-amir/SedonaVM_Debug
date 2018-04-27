// - CommandSet := EventRequest
// ---> Given CS Commands

#define ERSet 1
#define ERClear 2
#define ERClearAllBreakpoints 3

// modKind start index buff
#define MODKIND_POS (DATA_VARIABLE_POS + byte__SIZE * 2 + int__SIZE)

// modKind
#define MODKIND_COUNT 1
#define MODKIND_Conditional 2
#define MODKIND_ThreadOnly 3
#define MODKIND_ClassOnly 4
#define MODKIND_ClassMatch 5
#define MODKIND_ClassExclude 6
#define MODKIND_LocationOnly 7
#define MODKIND_ExceptionOnly 8
#define MODKIND_FieldOnly 9
#define MODKIND_Step 10
#define MODKIND_InstanceOnly 11
#define MODKIND_SourceNameMatch 12
