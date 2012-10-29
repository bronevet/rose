int foo(int& c) {
  return c+5;
}

int main()
{
  int a=1;
  int b;
  int c;
  b = a+foo(c);
  return b;
}
