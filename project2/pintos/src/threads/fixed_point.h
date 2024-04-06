#ifndef FIXED_POINT_H
#define FIXED_POINT_H
#include<ctype.h>
#include<stdio.h>
#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

// Integer to fixed point
int int_to_fp(int n);
// FP to int (round)
int fp_to_int_round(int x);
// FP to int (round down)
int fp_to_int(int x);
// addtion of FP
int add_fp(int x, int y);
// addition with FP and integer
int add_mixed(int x, int n);
// subtraction of FP
int sub_fp(int x, int y);
// subtraction with FP and integer
int sub_mixed(int x, int n);
// multiplication of FP
int mult_fp(int x, int y);
// multiplication of FP and integer
int mult_mixed(int x, int y);
// division of FP
int div_fp(int x, int y);
// division with FP and integer
int div_mixed(int x, int n);


int int_to_fp (int n) {
  return n * F;
}

int fp_to_int (int x) {
  //return x / F;
  /* Kwak */
  return x >> 14;
}

int fp_to_int_round (int x) {
  if (x >= 0) return (x + F / 2)>>14;
  else return (x - F / 2)>>14;
}

int add_fp (int x, int y) {
  return x + y;
}

int sub_fp (int x, int y) {
  return x - y;
}

int add_mixed (int x, int n) {
  return x + (n<<14);
}

int sub_mixed (int x, int n) {
  return x - (n<<14);
}

int mult_fp (int x, int y) {
  return (((int64_t) x) * y >> 14);
}

int mult_mixed (int x, int n) {
  return x * n;
}

int div_fp (int x, int y) {
  return (int)(((int64_t)x<<14) / y);;
}

int div_mixed (int x, int n) {
  return x / n;
}

#endif