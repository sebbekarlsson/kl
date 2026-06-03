#ifndef KL_MATH_MACROS_H
#define KL_MATH_MACROS_H


#define kl_vec2(X, Y) ((KL_Vec2){ .x = (X), .y = (Y) })
#define kl_vec21(X) kl_vec2(X, X)

#define kl_vec3(X, Y, Z) ((KL_Vec3){ .x = (X), .y = (Y), .z = (Z) })
#define kl_vec31(X) kl_vec3(X, X, X)

#define kl_vec4(X, Y, Z, W) ((KL_Vec4){ .x = (X), .y = (Y), .z = (Z), .w = (W) })
#define kl_vec41(X) kl_vec4(X, X, X, X)


#endif
