#include "main.cpp"
#include <cstdio>
static int fails = 0, passes = 0;

// Press a sequence of keys given as characters, and check the final display line.
static void press(const char* keys, const char* expect) {
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
  char res[48] = "";
  if (gErr) strcpy(res, "Error");
  else if (resultShown) fmtVal(lastRes, res, sizeof res);
  else strcpy(res, "(no result)");
  bool ok = !strcmp(res, expect);
  if (ok) passes++;
  else fails++;
  printf("%s %-22s -> %s (expected %s)\n", ok ? "PASS" : "FAIL", keys, res, expect);
}

int main() {
  Serial.silent = true;      // the calculator prints its screen to serial; keep that out of the output
  press("2+3*4=",   "14");
  press("9-2=-3=",  "4");
  press("2^3^2=",   "512");
  press("10/5=F",   "2/1");    // F<->D turns the result into a fraction
  press("10/5=FP5=", "10/5");  // COMP: 2/1 x 5 keeps the fraction form
  press("1/0=",     "Error");
  press("(2+3=",    "Error");
  press("2(3+1)=",  "8");      // implicit multiplication
  press("R16=",     "4");      // sqrt key first
  press("16R=",     "4");      // sqrt key after the number
  press("3Q=",      "9");      // x^2 key
  press("2.5*2=",   "5");
  press("5B*3=",    "-15");    // +/- on the number being typed
  press("B5=",      "-5");
  press("2+3=B",    "-5");     // +/- on a result
  press("2-B3=",    "5");

  printf("\n%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
