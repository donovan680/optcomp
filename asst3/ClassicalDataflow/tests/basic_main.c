int main(void)
{
  int x = 5;
  int r = 10;
  for (int i = 0; i < 42*x; i++) {
    r += x;
  }
  return 0;
};

//TODO: Benchmark w/ nested loops

//TODO: 3rd benchmark w/ more interesting loop invariant stuff to pull up (dependencies)