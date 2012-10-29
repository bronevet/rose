int global;

/*int bar(int arg)
{
  return global+arg;
}*/

// ConstProp:
// LiveDead: 
int noLDProp(int& q) {
	q = 101;
	return 102;
}
// ConstProp:
// LiveDead: 

// ConstProp:
// LiveDead: 
int LDPropThroughArg(int& q) {
	q += 201;
	int dummyToTestForNoProp;
	return 202;
}
// ConstProp: returned 202
// LiveDead: 

// ConstProp: q=0
// LiveDead: q
int LDPropThroughArgAndRet0(int& q) {
	q += 301;
	return q;
}
// ConstProp: q=301, returned 301
// LiveDead: q

// ConstProp: q=105
// LiveDead: q
int LDPropThroughArgAndRet1(int& q) {
	q += 401;
	return q;
}
// ConstProp: q=506, returned 506
// LiveDead: q

// ConstProp: q=top
// LiveDead: q
int LDPropThroughArgAndRet2(int& q) {
	q += 501;
	return q;
}
// ConstProp: q=top, returned top
// LiveDead: q

int main()
{
  int a=1;
  int b=2;
  int c=3;
  int d=4;
  int e=5;

  int array[10];
  // ConstProp: b=2, d=4
  // LiveDead: b (d if context insensitive), array[0], array[1] (array[*] if no constant propagation available)
  array[0] = 0;
  // ConstProp: b=2, d=4, array[0]=0
  // LiveDead: b (d if context insensitive), array[0], array[1] (array[*] if no constant propagation available)
  array[301] = 103;
  // ConstProp: b=2, d=4, array[0]=0, array[301]=103
  // LiveDead: b (d if context insensitive), array[0], array[1], array[301] (array[*] if no constant propagation available)
  array[1] = LDPropThroughArgAndRet0(array[0]);
  // ConstProp: b=2, d=4, array[0]=301, array[1]=301, array[301]=103
  // LiveDead: b (d if context insensitive), array[1], array[301] (array[*] if no constant propagation available)
  b += array[array[1]];

  // ConstProp: b=105, d=4
  // LiveDead: b (d if context insensitive)
  e = noLDProp(a);
  // ConstProp: b=105, d=4, e=102
  // LiveDead: e, b (d if context insensitive)
  e += LDPropThroughArgAndRet1(b);
  // ConstProp: e=608, d=4
  // LiveDead: e (d if context insensitive)
  e += LDPropThroughArg(c);
  // ConstProp: e=810, d=4
  // LiveDead: e (d if context insensitive)
  int Y = LDPropThroughArgAndRet2(d);
  // ConstProp: e=810
  // LiveDead: e
  int Z = LDPropThroughArgAndRet2(e);

  // ConstProp: Z=top
  // LiveDead: Z
  return Z;
}
