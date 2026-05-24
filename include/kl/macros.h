#ifndef KL_MACROS_H
#define KL_MACROS_H

#define KL_MAX(A, B) ((A) > (B) ? (A) : (B))
#define KL_MIN(A, B) ((A) > (B) ? (B) : (A))
#define KL_CLAMP(V, MI, MA) KL_MIN(KL_MAX(V, MI), MA)

#define KL_ARRAY_LEN(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

#define KL_ENUM_STR(NAME) #NAME,
#define KL_ENUM_VALUE(NAME) NAME,

#endif
