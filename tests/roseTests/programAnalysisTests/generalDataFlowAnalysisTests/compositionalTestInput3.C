int min(int a, int b)
{
	if(a > b) {
		return b;
	} else {
		return a;
	}
}

int max(int a, int b)
{
	if(a < b) {
		return b;
	} else {
		return a;
	}
}

int main()
{
	int w=15;
	int x=10;
	int y=20;
	int z = max(x, y);
	// ConstProp: w=15, x=10, y=20, z=20
	z = min(w,z);
	// ConstProp: w=15, x=10, y=20, z=15
	return z;
}
		
