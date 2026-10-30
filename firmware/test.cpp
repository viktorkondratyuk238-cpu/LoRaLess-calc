#include "main.cpp"
#include <cstdio>
// Press a sequence of keys given as characters, then print the final display line.
static void press(const char* name, const char* keys) {
  clearCalc();
  for (const char* p = keys; *p; p++) {
    Key k = K_NONE;
    switch (*p) {
      case '0': k=K_0; break; case '1': k=K_1; break; case '2': k=K_2; break;
      case '3': k=K_3; break; case '4': k=K_4; break; case '5': k=K_5; break;
      case '6': k=K_6; break; case '7': k=K_7; break; case '8': k=K_8; break;
      case '9': k=K_9; break; case '.': k=K_DOT; break;
      case '+': k=K_ADD; break; case '-': k=K_SUB; break; case '*': k=K_MUL; break;
      case '/': k=K_DIV; break; case '^': k=K_POW; break; case '(': k=K_LP; break;
      case ')': k=K_RP; break; case '=': k=K_EQ; break; case 'C': k=K_C; break;
      case 'F': k=K_FD; break; case 'P': k=K_COMP; break; case 'Q': k=K_SQ; break;
      case 'R': k=K_SQRT; break;
      case 'B': k=K_SIGN; break;
    }
    if (k != K_NONE) onKey(k);
  }
  printf("TEST %-28s -> ", name);
  char res[48] = "";
  if (gErr) strcpy(res, "Error");
  else if (resultShown) fmtVal(lastRes, res, sizeof res);
  else strcpy(res, "(no result)");
  printf("%s\n", res);
}
int main() {
  press("2+3*4=",            "2+3*4=");      // expect 14
  press("9-2=-3=",           "9-2=-3=");     // expect 4
  press("2^3^2=",            "2^3^2=");      // expect 512
  press("10/5=  then F<->D", "10/5=F");      // expect 2/1
  press("10/5= F COMP 5 =",  "10/5=FP5=");   // expect 10/5
  press("1/0=",              "1/0=");        // expect Error
  press("(2+3=",             "(2+3=");       // expect Error
  press("2(3+1)=",           "2(3+1)=");     // expect 8
  press("sqrt16=",           "R16=");        // expect 4
  press("16 sqrt=",          "16R=");        // expect 4
  press("x^2 of 3=",         "3Q=");         // expect 9
  press("2.5*2=",            "2.5*2=");      // expect 5
  press("5 +/- *3=",         "5B*3=");       // expect -15
  press("+/- 5 =",           "B5=");         // expect -5
  press("2+3= then +/-",     "2+3=B");       // expect -5
  press("2 - +/- 3 =",       "2-B3=");       // expect 5
  return 0;
}
