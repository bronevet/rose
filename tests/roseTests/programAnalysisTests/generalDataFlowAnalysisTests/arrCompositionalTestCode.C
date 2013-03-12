void bar(int a);

void foo()
{
  int array[10];
  int i=1;
  int j=2;
  array[i]=1;
  array[j]=2;
  int q = array[i];
  if(q) {
    q++;
  }
}
