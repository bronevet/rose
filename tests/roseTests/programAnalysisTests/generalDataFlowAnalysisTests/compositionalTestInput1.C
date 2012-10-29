int global;

int noProp(int q) {
	q = 101;
	return 102;
}
int PropThroughArg(int& q) {
	q += 201;
	int dummyToTestForNoProp;
	return 202;
}
int PropThroughRet1(int q) {
	q += 301;
	return q;
}
int PropThroughRet2(int q) {
	q += 401;
	return q;
}
int PropThroughRet3(int q) {
	q += 501;
	return q;
}
int PropThroughRet4(int q) {
	q += 601;
	return q;
}

int main()
{
  int a=1;
  int b=2;
  int c=3;
  int d=4;

  // ConstProp: a=1, b=2, c=3, d=4
  // LiveDead: a, b, c, d
  int W = noProp(a);
  // ConstProp: a=1, b=2, c=3, d=4, W=102
  // LiveDead: a, b, c, d, W
  int X = PropThroughArg(W);
  // ConstProp: a=1, b=2, c=3, d=4, X=202, W=303
  // LiveDead: a, b, c, d, X, W
  int Y = PropThroughRet1(X)+W;
  // ConstProp: a=1, b=2, c=3, d=4, Y=806
  // LiveDead: a, b, c, d, Y
  int array[100000];
  // ConstProp: a=1, b=2, c=3, d=4, Y=806
  // LiveDead: a, b, c, d, Y
  array[a] = PropThroughRet3(Y);
  // ConstProp: a=1, b=2, c=3, d=4, Y=806, array[1]=1307
  // LiveDead: a, b, c, d, Y, array[1]
  array[b] = Y;
  // ConstProp: a=1, b=2, c=3, d=4, array[1]=1307, array[2]=806
  // LiveDead: a, b, c, d, array[1], array[2]
  array[array[d-c]+array[b]] = array[a];
  // ConstProp: c=3, array[2113]=1307
  // LiveDead: c, array[2113]
  array[c] = PropThroughRet2(PropThroughRet4(array[2113]));
  // ConstProp: array[3]=3209
  // LiveDead: array[3]

  return array[3];
}
